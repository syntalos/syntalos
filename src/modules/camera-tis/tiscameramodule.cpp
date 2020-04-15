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

#include "tiscameramodule.h"

#include <QDebug>
#include "streams/frametype.h"

class TISCameraModule : public AbstractModule
{
    Q_OBJECT
private:

    std::shared_ptr<DataStream<Frame>> m_outStream;

public:
    explicit TISCameraModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));
    }

    ~TISCameraModule()
    {
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_THREADED |
               ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject &) override
    {
        return true;
    }

    void runThread(OptionalWaitCondition *waitCondition) override
    {
        // wait until we actually start acquiring data
        waitCondition->wait(this);

        while (m_running) {

        }
    }

    void stop() override
    {
    }
};

QString TISCameraModuleInfo::id() const
{
    return QStringLiteral("camera-tis");
}

QString TISCameraModuleInfo::name() const
{
    return QStringLiteral("TIS Camera");
}

QString TISCameraModuleInfo::description() const
{
    return QStringLiteral("Capture video with a camera from The Imaging Source.");
}

QPixmap TISCameraModuleInfo::pixmap() const
{
    return QPixmap(":/module/camera-tis");
}

QString TISCameraModuleInfo::license() const
{
    return QStringLiteral("This module embeds code from <a href=\"https://www.theimagingsource.com/\">The Imaging Source</a> "
                          "which is distributed under the terms of the Apache-2.0 license.");
}

AbstractModule *TISCameraModuleInfo::createModule(QObject *parent)
{
    return new TISCameraModule(parent);
}

#include "tiscameramodule.moc"
