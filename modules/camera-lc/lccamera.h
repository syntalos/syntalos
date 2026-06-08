/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QList>
#include <QPair>
#include <QSize>
#include <QString>
#include <opencv2/core.hpp>

#include "logging.h"
#include "datactl/frametype.h"
#include "datactl/syclock.h"
#include "datactl/timesync.h"

using namespace Syntalos;

/**
 * @brief Range and availability of a single camera control.
 */
struct LcControlRange {
    bool available = false;
    double min = 0;
    double max = 0;
    double def = 0;
};

class LcCameraData;

/**
 * @brief libcamera-backed camera abstraction.
 *
 * Wraps a single libcamera Camera and presents a simple, synchronous
 * frame-by-frame capture interface.
 * The libcamera CameraManager is a process-wide singleton shared by all instances.
 */
class LcCamera
{
public:
    explicit LcCamera(QuillLogger *logger);
    ~LcCamera();

    void setLogger(QuillLogger *logger);

    void setCameraId(const QString &id);
    QString cameraId() const;

    void setStartTime(const symaster_timepoint &time);

    cv::Size resolution() const;
    void setResolution(const cv::Size &size);

    double framerate() const;
    void setFramerate(double fps);

    QString pixelFormat() const;
    void setPixelFormat(const QString &fmt);

    bool autoExposure() const;
    void setAutoExposure(bool enabled);

    double exposureTime() const;
    void setExposureTime(double micros);

    double gain() const;
    void setGain(double value);

    double brightness() const;
    void setBrightness(double value);

    double contrast() const;
    void setContrast(double value);

    double saturation() const;
    void setSaturation(double value);

    double gamma() const;
    void setGamma(double value);

    // Power-line (mains) frequency for anti-flicker. Not exposed by libcamera for
    // UVC cameras, so it is handled out-of-band via V4L2 on the backing device node.
    // Values match V4L2_CID_POWER_LINE_FREQUENCY: 0 = disabled, 1 = 50 Hz, 2 = 60 Hz.
    int powerLineFrequency() const;
    void setPowerLineFrequency(int value);
    bool powerLineFrequencySupported();
    int readPowerLineFrequency();

    // Capability enumeration for the currently selected camera. These work
    // without acquiring the camera and are used to populate the settings UI.
    QList<QString> readPixelFormats();
    QList<cv::Size> readFrameSizes(const QString &pixfmt);
    LcControlRange controlRange(const QString &name);

    bool connect();
    void disconnect();
    bool isConnected() const;

    bool getFrame(Frame &frame, SecondaryClockSynchronizer *clockSync);

    QString lastError() const;

    static QList<QPair<QString, QString>> availableCameras();

private:
    std::unique_ptr<LcCameraData> d;

    void fail(const QString &msg);
    void commitV4LSettings();
};
