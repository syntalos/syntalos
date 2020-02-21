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
#include <QFileInfo>
#include "streams/frametype.h"

#include "videowriter.h"
#include "recordersettingsdialog.h"

class VideoRecorderModule : public AbstractModule
{
    Q_OBJECT

private:
    bool m_recording;
    bool m_initDone;
    QString m_vidSavePathBase;
    std::unique_ptr<VideoWriter> m_videoWriter;

    RecorderSettingsDialog *m_settingsDialog;

    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    std::shared_ptr<StreamSubscription<Frame>> m_inSub;

    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;
public:
    explicit VideoRecorderModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_settingsDialog(nullptr)
    {
        m_inPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_ctlPort = registerInputPort<ControlCommand>(QStringLiteral("control-in"), QStringLiteral("Control"));

        m_settingsDialog = new RecorderSettingsDialog;
        m_settingsDialog->setSaveTimestamps(true);
        addSettingsWindow(m_settingsDialog);
        setName(name());

        m_settingsDialog->setVideoName(QStringLiteral("video"));
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_EVENTS |
               ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject&) override
    {
        if (!m_settingsDialog->videoNameFromSource() && m_settingsDialog->videoName().isEmpty()) {
            raiseError("Video recording name is not set. Please set it in the settings to continue.");
            return false;
        }

        m_videoWriter.reset(new VideoWriter);
        m_videoWriter->setContainer(m_settingsDialog->videoContainer());
        m_videoWriter->setCodec(m_settingsDialog->videoCodec());
        m_videoWriter->setLossless(m_settingsDialog->isLossless());
        m_videoWriter->setFileSliceInterval(m_settingsDialog->sliceInterval());

        m_recording = false;
        m_initDone = false;
        m_inSub.reset();
        m_ctlSub.reset();
        if (!m_inPort->hasSubscription())
            return true;

        // get controller subscription, if we have any
        if (m_ctlPort->hasSubscription())
            m_ctlSub = m_ctlPort->subscription();

        m_inSub = m_inPort->subscription();
        m_recording = true;

        // don't permit configuration changes while we are running
        m_settingsDialog->setEnabled(false);

        return true;
    }

    void start() override
    {
        AbstractModule::start();

        // we may be actually idle in case we e.g. aren't connected to any source
        if (!m_recording && (state() != ModuleState::ERROR))
            setStateIdle();

        if (m_inSub.get() == nullptr)
            return;

        if (m_settingsDialog->videoNameFromSource())
            m_vidSavePathBase = getDataStoragePath(QStringLiteral("videos/unknown"), m_inSub->metadata());
        else
            m_vidSavePathBase = getDataStoragePath(m_settingsDialog->videoName());
    }

    bool runEvent() override
    {
        if (!m_recording) {
            // just exit if we aren't subscribed to any data source
            setStateReady();
            return true;
        }

        const auto maybeFrame = m_inSub->peekNext();
        if (!maybeFrame.has_value())
            return true;
        const auto frame = maybeFrame.value();

        if (!m_initDone) {
            const auto mdata = m_inSub->metadata();
            auto frameSize = mdata.value("size", QSize()).toSize();
            const auto framerate = mdata.value("framerate", 0).toInt();
            const auto useColor = mdata.value("hasColor", true).toBool();

            if (!frameSize.isValid()) {
                // we didn't get the dimensions from metadata - let's see if the current frame can
                // be used to get dimensions.
                frameSize = QSize(frame.mat.cols, frame.mat.rows);
            }

            if (!frameSize.isValid()) {
                raiseError(QStringLiteral("Frame source did not provide image dimensions!"));
                return false;
            }
            if (framerate == 0) {
                raiseError(QStringLiteral("Frame source did not provide a framerate!"));
                return false;
            }

            try {
                m_videoWriter->initialize(m_vidSavePathBase.toStdString(),
                                          frameSize.width(),
                                          frameSize.height(),
                                          framerate,
                                          useColor,
                                          m_settingsDialog->saveTimestamps());
            } catch (const std::runtime_error& e) {
                raiseError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
                return false;
            }

            // write info video info file with auxiliary information about the video we encoded
            // (this is useful to gather intel about the video without opening the video file)
            auto infoPath = QStringLiteral("%1_videoinfo.json").arg(m_vidSavePathBase);
            QJsonObject vInfo;
            vInfo.insert("name", m_settingsDialog->videoName());
            vInfo.insert("frameWidth", frameSize.width());
            vInfo.insert("frameHeight", frameSize.height());
            vInfo.insert("framerate", framerate);
            vInfo.insert("colored", useColor);

            QFile vInfoFile(infoPath);
            if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                raiseError("Unable to open video info file for writing.");
                return false;
            }

            QTextStream vInfoFileOut(&vInfoFile);
            vInfoFileOut << QJsonDocument(vInfo).toJson();

            // signal that we are actually recording this session
            m_initDone = true;
            statusMessage(QStringLiteral("Recording video..."));
        }

        // write video data
        if (!m_videoWriter->pushFrame(frame)) {
            raiseError(QString::fromStdString(m_videoWriter->lastError()));
            return false;
        }

        return true;
    }

    void stop() override
    {
        if (m_initDone && m_recording && m_videoWriter.get() != nullptr)
            m_videoWriter->finalize();

        statusMessage(QStringLiteral("Recording stopped."));
        m_videoWriter.reset(nullptr);

        // permit settings canges again
        m_settingsDialog->setEnabled(true);
    }

    QByteArray serializeSettings(const QString&) override
    {
        QJsonObject jset;
        jset.insert("videoNameFromSource", m_settingsDialog->videoNameFromSource());
        jset.insert("videoName", m_settingsDialog->videoName());
        jset.insert("saveTimestamps", m_settingsDialog->saveTimestamps());

        jset.insert("videoCodec", static_cast<int>(m_settingsDialog->videoCodec()));
        jset.insert("videoContainer", static_cast<int>(m_settingsDialog->videoContainer()));
        jset.insert("lossless", m_settingsDialog->isLossless());

        jset.insert("sliceInterval", static_cast<int>(m_settingsDialog->sliceInterval()));

        return jsonObjectToBytes(jset);
    }

    bool loadSettings(const QString&, const QByteArray &data) override
    {
        auto jset = jsonObjectFromBytes(data);

        m_settingsDialog->setVideoNameFromSource(jset.value("videoNameFromSource").toBool());
        m_settingsDialog->setVideoName(jset.value("videoName").toString());
        m_settingsDialog->setSaveTimestamps(jset.value("saveTimestamps").toBool());

        m_settingsDialog->setVideoCodec(static_cast<VideoCodec>(jset.value("videoCodec").toInt()));
        m_settingsDialog->setVideoContainer(static_cast<VideoContainer>(jset.value("videoContainer").toInt()));
        m_settingsDialog->setLossless(jset.value("lossless").toBool());

        m_settingsDialog->setSliceInterval(static_cast<uint>(jset.value("sliceInterval").toInt()));

        return true;
    }
};

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

#include "videorecordmodule.moc"
