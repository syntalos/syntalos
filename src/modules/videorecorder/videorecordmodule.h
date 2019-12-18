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

#include "imagesinkmodule.h"
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

class VideoRecorderModule : public ImageSinkModule
{
    Q_OBJECT
public:
    explicit VideoRecorderModule(QObject *parent = nullptr);
    ~VideoRecorderModule() override;

    void setName(const QString& name) override;
    ModuleFeatures features() const override;

    bool initialize(ModuleManager *manager) override;
    bool prepare(const QString& storageRootDir, const TestSubject& testSubject) override;
    void stop() override;

    bool canRemove(AbstractModule *mod) override;

    void showSettingsUi() override;

    QByteArray serializeSettings(const QString& confBaseDir) override;
    bool loadSettings(const QString& confBaseDir, const QByteArray& data) override;

public slots:
    void receiveFrame(const Frame& frame) override;

private slots:
    void recvModuleCreated(ModuleInfo *info, AbstractModule *mod);
    void recvModulePreRemove(AbstractModule *mod);

private:
    QList<ImageSourceModule*> m_frameSourceModules;
    QString m_vidStorageDir;
    std::unique_ptr<VideoWriter> m_videoWriter;
    RecorderSettingsDialog *m_settingsDialog;
    std::shared_ptr<StreamInputPort> m_inPort;
};

#endif // VIDEORECORDMODULE_H
