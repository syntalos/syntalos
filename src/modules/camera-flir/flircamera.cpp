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
#include <QSet>
#include <QDebug>

namespace spn_gic = Spinnaker::GenICam;

Q_LOGGING_CATEGORY(logModFlirCam, "mod.cam-flir")

enum class FLIRCamValueChange
{
    GAIN,
    GAMMA,
    EXPOSURE
};

inline uint qHash(FLIRCamValueChange key, uint seed)
{
    return ::qHash(static_cast<uint>(key), seed);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class FLIRCamera::Private
{
public:
    Private()
    {}

    QString lastError;
    std::chrono::time_point<symaster_clock> startTime;

    QString camSerial;

    std::atomic_bool running;
    std::thread::id acqThreadId;
    spn::SystemPtr system;
    spn::CameraPtr activeCam;

    cv::Size resolution;
    int framerate;
    double actualFramerate;

    long exposureTimeUs;
    double gainDb;
    double gamma;

    time_t timestampIncrementValue;

    std::atomic_bool haveValueChange;
    std::mutex valChangeMutex;
    QSet<FLIRCamValueChange> valChangeNotify;
};
#pragma GCC diagnostic pop

FLIRCamera::FLIRCamera()
    : d(new FLIRCamera::Private())
{
    d->running = false;
    d->camSerial = QString();
    d->activeCam = nullptr;
    d->system = nullptr;
    d->framerate = 30;
    d->resolution = cv::Size(540, 540);
    d->exposureTimeUs = 500;
    d->haveValueChange = false;
}

FLIRCamera::~FLIRCamera()
{
    if (d->activeCam.IsValid())
        qCWarning(logModFlirCam).noquote() << "Deleting camera class while camera was still loaded and active. Acquisition should have been stopped before.";
    terminateRun();
}

void FLIRCamera::setSerial(const QString &serial)
{
    d->camSerial = serial;
}

QString FLIRCamera::serial() const
{
    return d->camSerial;
}

void FLIRCamera::terminateRun()
{
    if (d->acqThreadId != std::this_thread::get_id()) {
        d->running = false;
        qCCritical(logModFlirCam).noquote() << "Attempt to shut down camera in a different thread than where it was initialized in was ignored, since the Spinnaker API is not threadsafe or reentrant."
                                            << "Please fix this API usage error.";
        return;
    }

    if (d->activeCam.IsValid()) {
        if (d->activeCam->IsInitialized()) {
            qCDebug(logModFlirCam).noquote() << "Camera deinitialize";
            d->activeCam->DeInit();
        }
    }
    d->activeCam = nullptr;

    if (d->system.IsValid()) {
        qCDebug(logModFlirCam).noquote() << "Spinnaker system instance release";
        d->system->ReleaseInstance();
    }

    d->system = nullptr;
    d->running = false;
}

bool FLIRCamera::isRunning() const
{
    return d->running;
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
        if (ptrDeviceType->GetIntValue() == spn::DeviceTypeEnum::DeviceType_GigEVision) {
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

bool FLIRCamera::applyInitialCamParameters(spn_ga::INodeMap &nodeMap)
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
    d->activeCam->ChunkSelector.SetValue(spn::ChunkSelector_Timestamp);
    d->activeCam->ChunkEnable.SetValue(true);

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
    d->activeCam->ExposureAuto.SetValue(spn::ExposureAuto_Off);
    d->activeCam->ExposureTime.SetValue(d->exposureTimeUs);

    // gain settings
    d->activeCam->GainAuto.SetValue(spn::GainAuto_Off);
    d->activeCam->Gain.SetValue(d->gainDb);

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
    if (d->camSerial.isEmpty()) {
        d->lastError = QStringLiteral("No valid FLIR camera set to acquire data from!");
        return false;
    }

    {
        // we are already setting all camera values as default, so any value changes that are still in the queue
        // do not have to be processed
        const std::lock_guard<std::mutex> lock(d->valChangeMutex);
        d->valChangeNotify.clear();
        d->haveValueChange = false;
    }

    // obtain system instance for the current thread
    d->system = spn::System::GetInstance();
    d->acqThreadId = std::this_thread::get_id();

    try {
        auto camList = d->system->GetCameras();
        d->activeCam = camList.GetBySerial(d->camSerial.toStdString());
        camList.Clear();
    } catch (Spinnaker::Exception& e) {
        d->lastError = QStringLiteral("Unable to get camera list: %1").arg(QString::fromStdString(e.what()));
        terminateRun();
        return false;
    }

    if (!d->activeCam.IsValid()) {
        d->lastError = QStringLiteral("Unable to set up FLIR Camera: Couldn't find device with serial %1").arg(d->camSerial);
        terminateRun();
        return false;
    }

    // update camera serial number, just in case
    spn_ga::INodeMap& nodeMapTLDevice = d->activeCam->GetTLDeviceNodeMap();
    spn_ga::CStringPtr ptrDeviceID = nodeMapTLDevice.GetNode("DeviceID");
    if (IsAvailable(ptrDeviceID) && IsReadable(ptrDeviceID)) {
        const auto deviceId = ptrDeviceID->ToString();
        d->camSerial = QString::fromLatin1(deviceId.c_str());
    }

    try {
        // initialize camera
        if (!d->activeCam->IsInitialized())
            d->activeCam->Init();

#ifdef QT_DEBUG
        disableGEVHeartbeat(d->activeCam->GetNodeMap(), d->activeCam->GetTLDeviceNodeMap());
#endif

        // apply parameters - the function will set lastError already if it returns false,
        // so we can just exist in that case.
        if (!applyInitialCamParameters(d->activeCam->GetNodeMap())) {
            terminateRun();
            return false;
        }

        // set acquisition mode to continuous
        spn_ga::CEnumerationPtr ptrAcquisitionMode = d->activeCam->GetNodeMap().GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode)) {
            d->lastError = QStringLiteral("Unable to set acquisition mode to continuous (node retrieval; camera %1)").arg(serial());
            terminateRun();
            return false;
        }

        spn_ga::CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous)) {
            d->lastError = QStringLiteral("Unable to set acquisition mode to continuous (entry 'continuous' retrieval camera %1)").arg(serial());
            terminateRun();
            return false;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        // begin acquiring images
        d->activeCam->BeginAcquisition();
        d->running = true;
    } catch (Spinnaker::Exception& e) {
        d->lastError = QStringLiteral("Unable to initialize data acquisition: %1").arg(QString::fromStdString(e.what()));
        terminateRun();
        return false;
    }

    return true;
}

void FLIRCamera::endAcquisition()
{
    if (!d->running)
        return;

    if (d->acqThreadId != std::this_thread::get_id()) {
        d->running = false;
        qCCritical(logModFlirCam).noquote() << "Ignored attempt to shut down camera acquisition in a different thread than where it was started, since the Spinnaker API is not threadsafe or reentrant."
                                            << "Please fix this API usage error.";
        return;
    }

    try {
        // end acquisition
        qCDebug(logModFlirCam).noquote() << "Camera end acquisition";
        d->activeCam->EndAcquisition();

        // deinitialize camera
        qCDebug(logModFlirCam).noquote() << "Camera cleanup runtime objects";
        terminateRun();
    } catch (Spinnaker::Exception& e) {
        qWarning().noquote().nospace() << "FLIR Camera: Issue while trying to end data acquisition. " << e.what();
    }
}

bool FLIRCamera::acquireFrame(Frame &frame, SecondaryClockSynchronizer *clockSync)
{
    if (!d->running) {
        d->lastError = QStringLiteral("Unable to acquire image, camera was not initialized.");
        return false;
    }

    if (d->haveValueChange) {
        // we have a camera value change!
        const std::lock_guard<std::mutex> lock(d->valChangeMutex);

        QSetIterator<FLIRCamValueChange> i(d->valChangeNotify);
        while (i.hasNext()) {
            const auto change = i.next();

            try {
                switch (change) {
                case FLIRCamValueChange::EXPOSURE:
                    d->activeCam->ExposureTime.SetValue(d->exposureTimeUs);
                    break;
                case FLIRCamValueChange::GAIN:
                    d->activeCam->Gain.SetValue(d->gainDb);
                    break;
                case FLIRCamValueChange::GAMMA:
                    if (d->gamma < 0) {
                        d->activeCam->GammaEnable.SetValue(false);
                    } else {
                        d->activeCam->GammaEnable.SetValue(true);
                        d->activeCam->Gamma.SetValue(d->gamma);
                    }
                    break;
                default:
                    qCDebug(logModFlirCam).noquote() << "No value change code implemented for property change for" << static_cast<int>(change);
                }
            } catch (Spinnaker::Exception&) {
                // Ignore any problems when changing the values for now.
            };
        }
        d->valChangeNotify.clear();
        d->haveValueChange = false;
    }

    try {
        // retrieve next received image and ensure image completion
        spn::ImagePtr image;
        auto frameRecvTime = FUNC_DONE_TIMESTAMP(d->startTime, image = d->activeCam->GetNextImage(10000));
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
    const std::lock_guard<std::mutex> lock(d->valChangeMutex);

    d->exposureTimeUs= time.count();

    d->valChangeNotify.insert(FLIRCamValueChange::EXPOSURE);
    d->haveValueChange = true;
}

double FLIRCamera::gain() const
{
    return d->gainDb;
}

void FLIRCamera::setGain(double gainDb)
{
    const std::lock_guard<std::mutex> lock(d->valChangeMutex);

    d->gainDb = gainDb;

    d->valChangeNotify.insert(FLIRCamValueChange::GAIN);
    d->haveValueChange = true;
}

double FLIRCamera::gamma() const
{
    return d->gamma;
}

void FLIRCamera::setGamma(double gamma)
{
    const std::lock_guard<std::mutex> lock(d->valChangeMutex);

    d->gamma = gamma;

    d->valChangeNotify.insert(FLIRCamValueChange::GAMMA);
    d->haveValueChange = true;
}

double FLIRCamera::actualFramerate() const
{
    return d->actualFramerate;
}

void FLIRCamera::printLibraryVersion()
{
    auto system = spn::System::GetInstance();
    const auto spinnakerLibraryVersion = system->GetLibraryVersion();
    qCDebug(logModFlirCam).noquote().nospace() << "Using Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
                                               << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build;
    system->ReleaseInstance();
}

QList<QPair<QString, QString> > FLIRCamera::availableCameras()
{
    QList<QPair<QString, QString>> res;

    auto system = spn::System::GetInstance();
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
    system->ReleaseInstance();
    return res;
}
