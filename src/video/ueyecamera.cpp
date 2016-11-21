/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ueyecamera.h"

#include <QDebug>
#include <ueye.h>
#include <chrono>

using namespace std::chrono;

UEyeCamera::UEyeCamera(QObject *parent)
    : QObject(parent),
      m_hCam(0)
{
}

UEyeCamera::~UEyeCamera()
{
    close();
}

QList<QPair<QString, int>> UEyeCamera::getCameraList() const
{
    INT nNumCam;
    QList<QPair<QString, int>> res;

    if (is_GetNumberOfCameras(&nNumCam) != IS_SUCCESS)
        return res;

    if (nNumCam >= 1) {
        // Create new list with suitable size
        UEYE_CAMERA_LIST* pucl;

        pucl = (UEYE_CAMERA_LIST*) new BYTE [sizeof (DWORD) + nNumCam * sizeof (UEYE_CAMERA_INFO)];
        pucl->dwCount = nNumCam;

        // Retrieve camera info

        if (is_GetCameraList(pucl) == IS_SUCCESS) {
            int iCamera;

            for (iCamera = 0; iCamera < (int)pucl->dwCount; iCamera++) {
                //Test output of camera info on the screen

                auto desc = QString("Camera: %1 (ID: %2)").arg(iCamera).arg(pucl->uci[iCamera].dwCameraID);
                res.append(qMakePair(desc, iCamera));
            }

        }
        delete [] pucl;
    }

    return res;
}

bool UEyeCamera::checkInit()
{
    if (m_hCam <= 0) {
        m_lastError = "Not initialized.";
        return false;
    }

    return true;
}

void UEyeCamera::setError(const QString& message, int code)
{
    if (code == 0)
        m_lastError = message;
    else
        m_lastError = QString("%1 (%2)").arg(message).arg(code);
}

QString UEyeCamera::lastError() const
{
    return m_lastError;
}

bool UEyeCamera::open(int cameraId, const QSize& size)
{
    if (cameraId < 0) {
        setError("Not initialized.");
        return false;
    }

    m_mat = cv::Mat(size.height(), size.width(), CV_8UC3);
    m_frameSize = size;
    qDebug() << "Opening camera with resolution:" << size;

    char* ppcImgMem;
    int pid;
    INT nAOISupported = 0;

    auto res = is_InitCamera(&m_hCam, 0);
    if (res != IS_SUCCESS) {
        setError("Unable to initialize camera", res);
        return false;
    }

    res = is_SetColorMode(m_hCam, IS_CM_BGR8_PACKED);
    if (res != IS_SUCCESS) {
        setError("Unable to set color mode", res);
        return false;
    }

    res = is_ImageFormat(m_hCam, IMGFRMT_CMD_GET_ARBITRARY_AOI_SUPPORTED, (void*) &nAOISupported, sizeof(nAOISupported));
    if (res != IS_SUCCESS) {
        setError("Unable to set image format", res);
        return false;
    }

    res = is_AllocImageMem(m_hCam, size.width(), size.height(), 24, &ppcImgMem, &pid);
    if (res != IS_SUCCESS) {
        setError("Unable to allocate image memory", res);
        return false;
    }

    res = is_SetImageMem(m_hCam, ppcImgMem, pid);
    if (res != IS_SUCCESS) {
        setError("Unable to set image memory", res);
        return false;
    }

    res = is_SetBinning(m_hCam, IS_BINNING_4X_VERTICAL | IS_BINNING_4X_HORIZONTAL);
    if (res != IS_SUCCESS) {
        setError("Unable to set binning", res);
        // FIXME: Binning doesn't seem to work...
        //return false;
    }

    if (!m_confFile.isEmpty()) {
        res = is_ParameterSet(m_hCam, IS_PARAMETERSET_CMD_LOAD_FILE, (wchar_t*) m_confFile.toStdWString().c_str(), 0);
        if (res != IS_SUCCESS) {
            setError("Unable to load uEye settings file", res);
            return false;
        }
    }

    // set auto settings
    if (!setAutoWhiteBalance(true))
        return false;

    if (!setAutoGain(true))
        return false;

    res = is_CaptureVideo(m_hCam, IS_WAIT);
    if (res != IS_SUCCESS) {
        setError("Unable to start video capture", res);
        return false;
    }

    return true;
}

bool UEyeCamera::close()
{
    auto res = is_ExitCamera(m_hCam);
    if (res != IS_SUCCESS) {
        setError("Unable to exit camera", res);
        return false;
    }

    m_hCam = 0;
    return true;
}

bool UEyeCamera::setFramerate(double fps)
{
    if (!checkInit())
        return false;
    is_SetFrameRate (m_hCam, fps, &fps);
    return true;
}

QPair<time_t, cv::Mat> UEyeCamera::getFrame()
{
    QPair<time_t, cv::Mat> frame;

    getFrame(&frame.first, m_mat);
    frame.second = m_mat;

    return frame;
}

void UEyeCamera::getFrame(time_t *time, cv::Mat& buffer)
{
    VOID* pMem;

    auto res = is_GetImageMem(m_hCam, &pMem);
    if (res != IS_SUCCESS) {
        setError("Unable to grab frame", res);
        return;
    }

    // grab millisecond-resolution timestamp
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    *time = ms.count();

    // width * height * depth (depth == 3)
    memcpy(buffer.ptr(), pMem, m_frameSize.width() * m_frameSize.height() * 3);
}

bool UEyeCamera::setAutoWhiteBalance(bool enabled)
{
    if (!checkInit())
        return false;

    double on = enabled ? 1 : 0;
    auto res = is_SetAutoParameter(m_hCam, IS_SET_ENABLE_AUTO_WHITEBALANCE, &on, nullptr);
    if (res != IS_SUCCESS) {
        setError("Unable to set automatic whitebalancing", res);
        return false;
    }

    return true;
}

bool UEyeCamera::setAutoGain(bool enabled)
{
    if (!checkInit())
        return false;

    double on = enabled ? 1 : 0;
    auto res = is_SetAutoParameter(m_hCam, IS_SET_ENABLE_AUTO_GAIN, &on, nullptr);
    if (res != IS_SUCCESS) {
        if (res == IS_NOT_SUPPORTED)
            return true;
        setError("Unable to set automatic gain", res);
        return false;
    }

    return true;
}

bool UEyeCamera::setExposureTime(double val)
{
    if (!checkInit())
        return false;

    auto res = is_Exposure(m_hCam, IS_EXPOSURE_CMD_SET_EXPOSURE, (void*) &val, sizeof(val));
    if (res != IS_SUCCESS) {
        if (res == IS_NOT_SUPPORTED)
            return true;
        setError("Unable to set exposure time", res);
        return false;
    }

    return true;
}

void UEyeCamera::setConfFile(const QString &fileName)
{
    m_confFile = fileName;
}

QString UEyeCamera::confFile() const
{
    return m_confFile;
}

QList<QSize> UEyeCamera::getResolutionList(int cameraId)
{
    QList<QSize> res;

    HIDS hCam = cameraId;
    auto ret = is_InitCamera(&hCam, nullptr);
    if (ret != IS_SUCCESS) {
        setError("Unable to initialize camera", ret);
        return res;
    }

    // Get number of available formats and size of list
    UINT count;
    UINT bytesNeeded = sizeof(IMAGE_FORMAT_LIST);
    ret = is_ImageFormat(hCam, IMGFRMT_CMD_GET_NUM_ENTRIES, &count, sizeof(count));
    bytesNeeded += (count - 1) * sizeof(IMAGE_FORMAT_INFO);

    void* ptr = malloc(bytesNeeded);

    // Create and fill list
    auto pformatList = (IMAGE_FORMAT_LIST*) ptr;
    pformatList->nSizeOfListEntry = sizeof(IMAGE_FORMAT_INFO);
    pformatList->nNumListElements = count;

    ret = is_ImageFormat(hCam, IMGFRMT_CMD_GET_LIST, pformatList, bytesNeeded);

    // Get list of supported resolutions
    IMAGE_FORMAT_INFO formatInfo;
    for (uint i = 0; i < count; i++)
    {
        formatInfo = pformatList->FormatInfo[i];
        res.append(QSize(formatInfo.nWidth, formatInfo.nHeight));
    }

    is_ExitCamera(hCam);
    return res;
}
