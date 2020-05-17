/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "flircamera.h"

#include <QFileInfo>
#include <QDebug>

namespace spn_gic = Spinnaker::GenICam;

#pragma GCC diagnostic ignored "-Wpadded"
class FLIRCamera::Private
{
public:
    Private()
    {}

    QString lastError;
    std::chrono::time_point<symaster_clock> startTime;
    spn::SystemPtr system;
    spn::CameraPtr cam;

    cv::Size resolution;
    int framerate;
    double actualFramerate;

    long exposureTimeUs;
    double gainDb;
    double gamma;

    time_t timestampIncrementValue;
};
#pragma GCC diagnostic pop

FLIRCamera::FLIRCamera(const Spinnaker::SystemPtr system)
    : d(new FLIRCamera::Private())
{
    d->system = system;
    d->framerate = 30;
    d->resolution = cv::Size(540, 540);
    d->exposureTimeUs = 500;
}

FLIRCamera::~FLIRCamera()
{
    if (d->cam->IsInitialized())
        d->cam->DeInit();
    d->cam = nullptr;
}

Spinnaker::SystemPtr FLIRCamera::system() const
{
    return d->system;
}

bool FLIRCamera::setup(const QString &serial)
{
    auto camList = d->system->GetCameras();
    d->cam = camList.GetBySerial(serial.toStdString());
    camList.Clear();

    auto ret = d->cam.IsValid();
    if (!ret)
        d->lastError = QStringLiteral("Unable to find camera for serial %1").arg(serial);
    return ret;
}

bool FLIRCamera::isValid() const
{
    return d->cam.IsValid();
}

bool FLIRCamera::isRunning() const
{
    return isValid() && d->cam->IsInitialized();
}

QString FLIRCamera::serial() const
{
    if (!isValid())
        return QString();

    spn_ga::INodeMap& nodeMapTLDevice = d->cam->GetTLDeviceNodeMap();
    spn_ga::CStringPtr ptrDeviceID = nodeMapTLDevice.GetNode("DeviceID");
    if (IsAvailable(ptrDeviceID) && IsReadable(ptrDeviceID)) {
        const auto deviceId = ptrDeviceID->ToString();
        return QString::fromLatin1(deviceId.c_str());
    }

    return QString();
}

void FLIRCamera::setStartTime(const symaster_timepoint &time)
{
    d->startTime = time;
}

QString FLIRCamera::lastError() const
{
    return d->lastError;
}

#ifdef QT_DEBUG
/**
 * Disables heartbeat on GEV cameras so debugging does not incur timeout errors
 */
static bool disableGEVHeartbeat(spn_ga::INodeMap& nodeMap, spn_ga::INodeMap& nodeMapTLDevice)
{
    // Write to boolean node controlling the camera's heartbeat
    //
    // *** NOTES ***
    // This applies only to GEV cameras and only applies when in DEBUG mode.
    // GEV cameras have a heartbeat built in, but when debugging applications the
    // camera may time out due to its heartbeat. Disabling the heartbeat prevents
    // this timeout from occurring, enabling us to continue with any necessary debugging.
    // This procedure does not affect other types of cameras and will prematurely exit
    // if it determines the device in question is not a GEV camera.
    //
    // *** LATER ***
    // Since we only disable the heartbeat on GEV cameras during debug mode, it is better
    // to power cycle the camera after debugging. A power cycle will reset the camera
    // to its default settings.

    spn_ga::CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
    if (!IsAvailable(ptrDeviceType) && !IsReadable(ptrDeviceType)) {
        qDebug() << "Unable to read FLIR camera device type.";
        return false;
    } else {
        if (ptrDeviceType->GetIntValue() == spn::DeviceType_GEV) {
            qDebug() << "FLIR Camera:" << "Attempting to disable GigE camera heartbeat before continuing";
            spn_ga::CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
            if (!IsAvailable(ptrDeviceHeartbeat) || !IsWritable(ptrDeviceHeartbeat)) {
                qDebug() << "FLIR Camera:" << "Unable to disable heartbeat on camera.";
                return false;
            } else {
                ptrDeviceHeartbeat->SetValue(true);
                qDebug() << "FLIR Camera:" << "WARNING: Heartbeat on GigE camera disabled for the rest of Debug Mode."
                         << "Power cycle camera when done debugging to re-enable the heartbeat.";
            }
        }
    }

    return true;
}
#endif

bool FLIRCamera::applyCamParameters(spn_ga::INodeMap &nodeMap)
{
    // get timestamp increment value
    d->timestampIncrementValue = 1;
    spn_ga::CIntegerPtr ptrTSIncrement = nodeMap.GetNode("TimestampIncrement");
    if (IsAvailable(ptrTSIncrement)) {
        // apparently the device clocks tick in nanoseconds, we need the increment
        // factor to get the actual time later
        d->timestampIncrementValue = ptrTSIncrement->GetValue();
    }

    // activate chunk mode
    spn_ga::CBooleanPtr ptrChunkModeActive = nodeMap.GetNode("ChunkModeActive");
    if (!IsAvailable(ptrChunkModeActive) || !IsWritable(ptrChunkModeActive)) {
        d->lastError = QStringLiteral("Unable to activate chunk mode. Can not continue.");
        return false;
    }
    ptrChunkModeActive->SetValue(true);

    // enable timestamp chunks
    d->cam->ChunkSelector.SetValue(spn::ChunkSelector_Timestamp);
    d->cam->ChunkEnable.SetValue(true);

    // set image width
    spn_ga::CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
    if (IsAvailable(ptrWidth) && IsWritable(ptrWidth)) {
        ptrWidth->SetValue(d->resolution.width);
        d->resolution = cv::Size(ptrWidth->GetValue(), d->resolution.height);
    } else {
        d->lastError = QStringLiteral("Unable to set frame width to %1, this dimension may not be supported")
                                      .arg(d->resolution.width);
        return false;
    }

    // set image height
    spn_ga::CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
    if (IsAvailable(ptrHeight) && IsWritable(ptrHeight)) {
        ptrHeight->SetValue(d->resolution.height);
        d->resolution = cv::Size(d->resolution.width, ptrHeight->GetValue());
    } else {
        d->lastError = QStringLiteral("Unable to set frame height to %1, this dimension may not be supported")
                                      .arg(d->resolution.height);
        return false;
    }

    // exposure settings
    d->cam->ExposureAuto.SetValue(spn::ExposureAuto_Off);
    d->cam->ExposureTime.SetValue(d->exposureTimeUs);

    // gain settings
    d->cam->GainAuto.SetValue(spn::GainAuto_Off);
    d->cam->Gain.SetValue(d->gainDb);

    // refresh gamma settings
    setGamma(d->gamma);

    // set framerate (has to be last, as it ultimately depends on the other settings)
    spn_ga::CBooleanPtr ptrAcqFPSEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
    if (IsAvailable(ptrAcqFPSEnable) && IsWritable(ptrAcqFPSEnable)) {
        ptrAcqFPSEnable->SetValue(true);
    } else {
        d->lastError = QStringLiteral("Unable to get manual control over acquisition framerate. This feature may be unsupported by the selected camera.");
        return false;
    }
    spn_ga::CFloatPtr ptrFramerate = nodeMap.GetNode("AcquisitionFrameRate");
    if (IsAvailable(ptrFramerate) && IsWritable(ptrFramerate)) {
        ptrFramerate->SetValue(d->framerate);
        d->framerate = ptrFramerate->GetValue();
    } else {
        d->lastError = QStringLiteral("Unable to set framerate to %1, this action may be unsupported.")
                                      .arg(d->framerate);
        return false;
    }

    // retrieve actual framerate
    d->actualFramerate = d->framerate;
    spn_ga::CFloatPtr ptrResFramerate = nodeMap.GetNode("AcquisitionResultingFrameRate");
    if (IsAvailable(ptrResFramerate))
        d->actualFramerate = ptrResFramerate->GetValue();

    return true;
}

bool FLIRCamera::initAcquisition()
{
    if (!isValid()) {
        d->lastError = QStringLiteral("No valid FLIR camera set to acquire data from!");
        return false;
    }

    try {
        // initialize camera
        d->cam->Init();

#ifdef QT_DEBUG
        disableGEVHeartbeat(d->cam->GetNodeMap(), d->cam->GetTLDeviceNodeMap());
#endif

        // apply parameters - the function will set lastError already if it returns false,
        // so we can just exist in that case.
        if (!applyCamParameters(d->cam->GetNodeMap())) {
            d->cam->DeInit();
            return false;
        }

        // set acquisition mode to continuous
        spn_ga::CEnumerationPtr ptrAcquisitionMode = d->cam->GetNodeMap().GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode)) {
            d->lastError = QStringLiteral("Unable to set acquisition mode to continuous (node retrieval; camera %1)").arg(serial());
            d->cam->DeInit();
            return false;
        }

        spn_ga::CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous)) {
            d->lastError = QStringLiteral("Unable to set acquisition mode to continuous (entry 'continuous' retrieval camera %1)").arg(serial());
            d->cam->DeInit();
            return false;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        // begin acquiring images
        d->cam->BeginAcquisition();
    } catch (Spinnaker::Exception& e) {
        d->lastError = QString::fromStdString(e.what());
        return false;
    }

    return true;
}

void FLIRCamera::endAcquisition()
{
    try {
        // end acquisition
        d->cam->EndAcquisition();
        // deinitialize camera
        if (d->cam->IsInitialized())
            d->cam->DeInit();
    } catch (Spinnaker::Exception& e) {
        qWarning().noquote().nospace() << "FLIR Camera: Issue while trying to end data acquisition. " << e.what();
    }
}

bool FLIRCamera::acquireFrame(Frame &frame, SecondaryClockSynchronizer *clockSync)
{
    try {
        // retrieve next received image and ensure image completion
        spn::ImagePtr image;
        auto frameRecvTime = FUNC_EXEC_TIMESTAMP(d->startTime, image = d->cam->GetNextImage(1000));
        if (image->IsIncomplete()) {
            d->lastError = QStringLiteral("FLIR Camera %1: Frame dropped, image status was %1").arg(serial()).arg(image->GetImageStatus());
            image->Release();
            return false;
        }

        const auto rows = image->GetHeight();
        const auto cols = image->GetWidth();
        const auto data = image->GetData();
        const auto stride = image->GetStride();
        const auto pixFmt = image->GetPixelFormat();
        cv::Mat tmpMat;
        if (pixFmt == spn::PixelFormatEnums::PixelFormat_Mono8)
            tmpMat = cv::Mat(rows, cols, CV_8UC1, static_cast<unsigned char*>(data), stride);
        else if (pixFmt == spn::PixelFormatEnums::PixelFormat_BGR8)
            tmpMat = cv::Mat(rows, cols, CV_8UC3, static_cast<unsigned char*>(data), stride);
        else if (pixFmt == spn::PixelFormatEnums::PixelFormat_BGR16)
            tmpMat = cv::Mat(rows, cols, CV_16UC3, static_cast<unsigned char*>(data), stride);
        else {
            // Convert image to BGR8 transparently if we can not handle its original format natively
            const auto convertedImage = image->Convert(spn::PixelFormat_BGR8, spn::HQ_LINEAR);
            tmpMat = cv::Mat(rows, cols, CV_8UC3, static_cast<unsigned char*>(data), stride);
        }

        // create deep copy to our final frame
        tmpMat.copyTo(frame.mat);

        const auto chunkData = image->GetChunkData();
        // get the device timestamp. FIXME: The Spinnaker API doesn't explicitly mention the timestamp unit, but it appears
        // to be nanoseconds (at least for non-GigE cameras, those may return ticks (?))
        // This should be tested with more cameras, to ensure we get an accurate time.
        const size_t timestampUs = std::lround(chunkData.GetTimestamp() / 1000.0);

        // adjust the received time if necessary, gather clock sync information
        // for some reason the timestamp occasionally is stuck at zero
        clockSync->processTimestamp(frameRecvTime, microseconds_t(timestampUs));
        frame.time = usecToMsec(frameRecvTime);

        // release image
        image->Release();
    }
    catch (Spinnaker::Exception& e) {
        d->lastError = QStringLiteral("Unable to acquire image: %1").arg(e.what());
        return false;
    }
    return true;
}

cv::Size FLIRCamera::resolution() const
{
    return d->resolution;
}

void FLIRCamera::setResolution(const cv::Size &size)
{
    d->resolution = size;
}

void FLIRCamera::setFramerate(int fps)
{
    d->framerate = fps;
}

microseconds_t FLIRCamera::exposureTime() const
{
    return microseconds_t(d->exposureTimeUs);
}

void FLIRCamera::setExposureTime(microseconds_t time)
{
    d->exposureTimeUs= time.count();
    if (!isRunning())
        return;
    try {
        d->cam->ExposureTime.SetValue(time.count());
    } catch (Spinnaker::Exception&) {};
}

double FLIRCamera::gain() const
{
    return d->gainDb;
}

void FLIRCamera::setGain(double gainDb)
{
    d->gainDb = gainDb;
    if (!isRunning())
        return;
    try {
        d->cam->Gain.SetValue(gainDb);
    } catch (Spinnaker::Exception&) {};
}

double FLIRCamera::gamma() const
{
    return d->gamma;
}

void FLIRCamera::setGamma(double gamma)
{
    d->gamma = gamma;
    if (!isRunning())
        return;
    try {
        if (gamma < 0) {
            d->cam->GammaEnable.SetValue(false);
        } else {
            d->cam->GammaEnable.SetValue(true);
            d->cam->Gamma.SetValue(gamma);
        }
    } catch (Spinnaker::Exception& e) {
        qDebug() << "Unable to set gamma value" << e.what();
    };
}

double FLIRCamera::actualFramerate() const
{
    return d->actualFramerate;
}

void FLIRCamera::printLibraryVersion(const Spinnaker::SystemPtr system)
{
    const auto spinnakerLibraryVersion = system->GetLibraryVersion();
    qDebug().noquote().nospace() << "Using Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
             << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build;
}

QList<QPair<QString, QString> > FLIRCamera::availableCameras(const Spinnaker::SystemPtr system)
{
    QList<QPair<QString, QString>> res;

    auto camList = system->GetCameras();
    unsigned int nCameras = camList.GetSize();

    res.reserve(nCameras);
    for (uint i = 0; i < nCameras; i++) {
        QString camDisplayName;
        QString camSerial;
        auto cam = camList.GetByIndex(i);
        spn_ga::INodeMap& nodeMapTLDevice = cam->GetTLDeviceNodeMap();

        spn_ga::CStringPtr ptrDeviceVendorName = nodeMapTLDevice.GetNode("DeviceVendorName");
        if (IsAvailable(ptrDeviceVendorName) && IsReadable(ptrDeviceVendorName)) {
            const auto deviceVendorName = ptrDeviceVendorName->ToString();
            camDisplayName = QString::fromUtf8(deviceVendorName.c_str());
            if (camDisplayName.isEmpty())
                camDisplayName = QStringLiteral("Unknown");
        }

        spn_ga::CStringPtr ptrDeviceModelName = nodeMapTLDevice.GetNode("DeviceModelName");
        if (IsAvailable(ptrDeviceModelName) && IsReadable(ptrDeviceModelName)) {
            const auto deviceModelName = ptrDeviceModelName->ToString();
            camDisplayName = QStringLiteral("%1 - %2").arg(camDisplayName).arg(QString::fromUtf8(deviceModelName.c_str()));
            if (camDisplayName.isEmpty())
                camDisplayName = QStringLiteral("Unknown Device");
        }

        spn_ga::CStringPtr ptrDeviceSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
        if (IsAvailable(ptrDeviceSerial) && IsReadable(ptrDeviceSerial)) {
            const auto devSerialStr = ptrDeviceSerial->ToString();
            camSerial = QString::fromLatin1(devSerialStr.c_str());
        }

        if (camSerial.isEmpty()) {
            qWarning() << "Ignoring FLIR camera" << camDisplayName << "- Serial number was empty";
            continue;
        }

        res.append(qMakePair(camDisplayName, camSerial));
    }

    camList.Clear();
    return res;
}
