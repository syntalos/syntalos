/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "videorecordmodule.h"

#include <QMessageBox>

VideoRecorderModule::VideoRecorderModule(QObject *parent)
    : ImageSinkModule(parent)
{
    m_name = QStringLiteral("Video Recorder");
}

VideoRecorderModule::~VideoRecorderModule()
{
}

QString VideoRecorderModule::id() const
{
    return QStringLiteral("videorecorder");
}

QString VideoRecorderModule::description() const
{
    return QStringLiteral("Store a video composed of frames from an image source module to disk.");
}

QPixmap VideoRecorderModule::pixmap() const
{
    return QPixmap(":/module/videorecorder");
}

ModuleFeatures VideoRecorderModule::features() const
{
    return ModuleFeature::SETTINGS;
}

bool VideoRecorderModule::initialize(ModuleManager *manager)
{
    assert(!initialized());

    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool VideoRecorderModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(storageRootDir);
    Q_UNUSED(testSubject);

    return true;
}

void VideoRecorderModule::stop()
{

}

void VideoRecorderModule::showDisplayUi()
{
}

void VideoRecorderModule::hideDisplayUi()
{
}

void VideoRecorderModule::receiveFrame(const FrameData &frameData)
{

}
