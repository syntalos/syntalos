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

#include "syclock.h"
#include "timesync.h"
#include "streams/frametype.h"

namespace spn = Spinnaker;
namespace spn_ga = Spinnaker::GenApi;

class FLIRCamera
{
public:
    FLIRCamera(const spn::SystemPtr system);
    ~FLIRCamera();

    spn::SystemPtr system() const;

    /**
     * @brief Acquire camera pointer for the given serial
     * No other method may be called on this object before setup() did
     * not complete successfully.
     */
    bool setup(const QString &serial);
    bool isValid() const;
    QString serial() const;

    QString lastError() const;

    bool initAcquisition();
    void endAcquisition();

    bool acquireFrame(Frame &frame);

    void setResolution(const cv::Size &size);
    void setFramerate(int fps);

    double actualFramerate() const;

    static void printLibraryVersion(const spn::SystemPtr system);
    static QList<QPair<QString, QString> > availableCameras(const spn::SystemPtr system);

private:
    class Private;
    Q_DISABLE_COPY(FLIRCamera)
    QScopedPointer<Private> d;

    bool applyCamParameters(spn_ga::INodeMap& nodeMap);
};
