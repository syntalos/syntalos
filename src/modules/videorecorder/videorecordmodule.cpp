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

#include "videorecordmodule.h"

#include <QMessageBox>
#include <QJsonDocument>
#include <QDebug>
#include "streams/frametype.h"

#include "videowriter.h"
#include "recordersettingsdialog.h"

QString VideoRecorderModuleInfo::id() const
{
    return QStringLiteral("videorecorder");
}

QString VideoRecorderModuleInfo::name() const
{
    return QStringLiteral("Video Recorder");
}

QString VideoRecorderModuleInfo::description() const
{
    return QStringLiteral("Store a video composed of frames from an image source module to disk.");
}

QPixmap VideoRecorderModuleInfo::pixmap() const
{
    return QPixmap(":/module/videorecorder");
}

AbstractModule *VideoRecorderModuleInfo::createModule(QObject *parent)
{
    return new VideoRecorderModule(parent);
}

VideoRecorderModule::VideoRecorderModule(QObject *parent)
    : AbstractModule(parent),
      m_settingsDialog(nullptr)
{
    m_inPort = registerInputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));

    m_settingsDialog = new RecorderSettingsDialog;
    m_settingsDialog->setVideoName("video");
    m_settingsDialog->setSaveTimestamps(true);
    addSettingsWindow(m_settingsDialog);
    setName(name());
}

void VideoRecorderModule::setName(const QString &name)
{
    AbstractModule::setName(name);
    m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
}

ModuleFeatures VideoRecorderModule::features() const
{
    return ModuleFeature::RUN_THREADED |
           ModuleFeature::SHOW_SETTINGS;
}

bool VideoRecorderModule::prepare(const QString &storageRootDir, const TestSubject&)
{
    m_vidStorageDir = QStringLiteral("%1/videos").arg(storageRootDir);

    if (m_settingsDialog->videoName().isEmpty()) {
        raiseError("Video recording name is not set. Please set it in the settings to continue.");
        return false;
    }
    if (!makeDirectory(m_vidStorageDir))
        return false;

    m_videoWriter.reset(new VideoWriter);
    m_videoWriter->setContainer(m_settingsDialog->videoContainer());
    m_videoWriter->setCodec(m_settingsDialog->videoCodec());
    m_videoWriter->setLossless(m_settingsDialog->isLossless());
    m_videoWriter->setFileSliceInterval(m_settingsDialog->sliceInterval());

    return true;
}

void VideoRecorderModule::runThread(OptionalWaitCondition *startWaitCondition)
{

    if (!m_inPort->hasSubscription()) {
        // just exit if we aren't subscribed to any data source
        setStateReady();
        return;
    }
    auto sub = m_inPort->subscription<Frame>();
    const auto mdata = sub->metadata();
    const auto frameSize = mdata["size"].toSize();
    const auto framerate = mdata["framerate"].toInt();
    const auto useColor = mdata.value("hasColor", true).toBool();

    try {
        m_videoWriter->initialize(QStringLiteral("%1/%2").arg(m_vidStorageDir).arg(m_settingsDialog->videoName()).toStdString(),
                                  frameSize.width(),
                                  frameSize.height(),
                                  framerate,
                                  useColor,
                                  m_settingsDialog->saveTimestamps());
    } catch (const std::runtime_error& e) {
        raiseError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
        return;
    }

    // write info video info file with auxiliary information about the video we encoded
    // (this is useful to gather intel about the video without opening the video file)
    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_vidStorageDir).arg(m_settingsDialog->videoName());
    QJsonObject vInfo;
    vInfo.insert("name", m_settingsDialog->videoName());
    vInfo.insert("frameWidth", frameSize.width());
    vInfo.insert("frameHeight", frameSize.height());
    vInfo.insert("framerate", framerate);
    vInfo.insert("colored", useColor);

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        raiseError("Unable to open video info file for writing.");
        return;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    // wait until data acquisition has started
    startWaitCondition->wait(this);

    m_recording = true;
    statusMessage(QStringLiteral("Recording video..."));
    while (true) {
        auto data = sub->next();
        if (!data.has_value())
            break; // subscription has been terminated

        // write video data
        if (!m_videoWriter->pushFrame(data.value())) {
            raiseError(QString::fromStdString(m_videoWriter->lastError()));
            break;
        }
    }
    m_recording = false;
}

void VideoRecorderModule::start()
{
    AbstractModule::start();

    // we may be actually idle in case we e.g. aren't connected to any source
    if (!m_recording && (state() != ModuleState::ERROR))
        setStateIdle();
}

void VideoRecorderModule::stop()
{
    if (m_videoWriter.get() != nullptr)
        m_videoWriter->finalize();

    statusMessage(QStringLiteral("Recording stopped."));
    m_videoWriter.reset(nullptr);
}

void VideoRecorderModule::showSettingsUi()
{
    m_settingsDialog->show();
}

QByteArray VideoRecorderModule::serializeSettings(const QString&)
{
    QJsonObject jset;
    jset.insert("videoName", m_settingsDialog->videoName());
    jset.insert("saveTimestamps", m_settingsDialog->saveTimestamps());

    jset.insert("videoCodec", static_cast<int>(m_settingsDialog->videoCodec()));
    jset.insert("videoContainer", static_cast<int>(m_settingsDialog->videoContainer()));
    jset.insert("lossless", m_settingsDialog->isLossless());

    jset.insert("sliceInterval", static_cast<int>(m_settingsDialog->sliceInterval()));

    return jsonObjectToBytes(jset);
}

bool VideoRecorderModule::loadSettings(const QString&, const QByteArray &data)
{
    auto jset = jsonObjectFromBytes(data);

    m_settingsDialog->setVideoName(jset.value("videoName").toString());
    m_settingsDialog->setSaveTimestamps(jset.value("saveTimestamps").toBool());

    m_settingsDialog->setVideoCodec(static_cast<VideoCodec>(jset.value("videoCodec").toInt()));
    m_settingsDialog->setVideoContainer(static_cast<VideoContainer>(jset.value("videoContainer").toInt()));
    m_settingsDialog->setLossless(jset.value("lossless").toBool());

    m_settingsDialog->setSliceInterval(static_cast<uint>(jset.value("sliceInterval").toInt()));

    return true;
}
