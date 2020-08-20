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

#include "videotransformmodule.h"

#include <QMessageBox>
#include <QProcess>

class VideoTransformModule : public AbstractModule
{
    Q_OBJECT

private:

public:
    explicit VideoTransformModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
    }

    ~VideoTransformModule()
    {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject&) override
    {
        raiseError("Not implemented yet.");
        setStateReady();
        return false;
    }

    void stop() override
    {

    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
       Q_UNUSED(settings)
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        Q_UNUSED(settings)
        return true;
    }
};

QString VideoTransformModuleInfo::id() const
{
    return QStringLiteral("videotransform");
}

QString VideoTransformModuleInfo::name() const
{
    return QStringLiteral("Video Transformer");
}

QString VideoTransformModuleInfo::description() const
{
    return QStringLiteral("Perform common transformations on frames, such as cropping and scaling.");
}

QPixmap VideoTransformModuleInfo::pixmap() const
{
    return QPixmap(":/module/videotransform");
}

AbstractModule *VideoTransformModuleInfo::createModule(QObject *parent)
{
    return new VideoTransformModule(parent);
}

#include "videotransformmodule.moc"
