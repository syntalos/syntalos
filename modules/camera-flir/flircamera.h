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

#pragma once

#include <QObject>
#include <QScopedPointer>
#include <QSize>
#include <Spinnaker.h>

#include "datactl/frametype.h"
#include "datactl/syclock.h"
#include "datactl/timesync.h"

namespace spn = Spinnaker;
namespace spn_ga = Spinnaker::GenApi;

Q_DECLARE_LOGGING_CATEGORY(logModFlirCam)

class FLIRCamera
{
public:
    FLIRCamera();
    ~FLIRCamera();

    /**
     * @brief Set camera serial number.
     * This must be set before initAcquisition() can be called.
     */
    void setSerial(const QString &serial);
    QString serial() const;

    bool isRunning() const;

    void setStartTime(const symaster_timepoint &time);

    QString lastError() const;

    bool initAcquisition();
    bool acquireFrame(Frame &frame, SecondaryClockSynchronizer *clockSync);
    void endAcquisition();

    cv::Size resolution() const;
    void setResolution(const cv::Size &size);
    void setFramerate(int fps);

    microseconds_t exposureTime() const;
    void setExposureTime(microseconds_t time);

    double gain() const;
    void setGain(double gainDb);

    double gamma() const;
    void setGamma(double gamma);

    double actualFramerate() const;

    static void printLibraryVersion();
    static QList<QPair<QString, QString>> availableCameras();

private:
    class Private;
    Q_DISABLE_COPY(FLIRCamera)
    QScopedPointer<Private> d;

    void terminateRun();
    bool applyInitialCamParameters(spn_ga::INodeMap &nodeMap);
};
