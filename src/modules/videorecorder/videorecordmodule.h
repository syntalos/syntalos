/**
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

#ifndef VIDEORECORDMODULE_H
#define VIDEORECORDMODULE_H

#include <QObject>
#include <memory>
#include <chrono>

#include "moduleapi.h"

class VideoWriter;
class ImageSourceModule;
class RecorderSettingsDialog;

class VideoRecorderModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class VideoRecorderModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit VideoRecorderModule(QObject *parent = nullptr);

    void setName(const QString& name) override;
    ModuleFeatures features() const override;

    bool prepare(const QString& storageRootDir, const TestSubject& testSubject) override;
    void runThread(OptionalWaitCondition *startWaitCondition) override;
    void start() override;
    void stop() override;

    void showSettingsUi() override;

    QByteArray serializeSettings(const QString& confBaseDir) override;
    bool loadSettings(const QString& confBaseDir, const QByteArray& data) override;

private:
    std::atomic_bool m_recording;
    QString m_vidStorageDir;
    std::unique_ptr<VideoWriter> m_videoWriter;
    RecorderSettingsDialog *m_settingsDialog;
    std::shared_ptr<StreamInputPort> m_inPort;
};

#endif // VIDEORECORDMODULE_H
