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
#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>
#include "streams/frametype.h"

#include "videowriter.h"
#include "recordersettingsdialog.h"

enum class RecordingState
{
    RUNNING,
    PAUSED,
    STOPPED
};

class VideoRecorderModule : public AbstractModule
{
    Q_OBJECT

private:
    bool m_recording;
    bool m_initDone;
    bool m_recordingFinished;
    bool m_startStopped;
    std::shared_ptr<EDLDataset> m_vidDataset;
    std::unique_ptr<VideoWriter> m_videoWriter;

    RecorderSettingsDialog *m_settingsDialog;

    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    std::shared_ptr<StreamSubscription<Frame>> m_inSub;

    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;
    bool m_checkCommands;
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

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        // We use CORE_AFFINITY here mainly to avoid other modules being scheduled on the same core, since
        // video encoding is a very CPU-heavy task which may starve other stuff running on the same CPU.
        // Usually, CORE_AFFINITY is used to prevent the scheduler moving a thread to a new CPU once it blocks
        // (but it's unlikely that we will block)
        return ModuleFeature::CORE_AFFINITY |
               ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject&) override
    {
        if (!m_settingsDialog->videoNameFromSource() && m_settingsDialog->videoName().isEmpty()) {
            raiseError("Video recording name is not set. Please set it in the settings to continue.");
            return false;
        }

        m_videoWriter.reset(new VideoWriter);
        auto codecProps = m_settingsDialog->codecProps();
        m_videoWriter->setContainer(m_settingsDialog->videoContainer());

        codecProps.setThreadCount((potentialNoaffinityCPUCount() >= 2)? potentialNoaffinityCPUCount() : 2);
        m_videoWriter->setCodecProps(codecProps);

        m_videoWriter->setFileSliceInterval(0); // no slicing allowed, unless changed later
        if (m_settingsDialog->slicingEnabled())
            m_videoWriter->setFileSliceInterval(m_settingsDialog->sliceInterval());

        m_recording = false;
        m_initDone = false;
        m_recordingFinished = true;
        m_startStopped = m_settingsDialog->startStopped();
        m_inSub.reset();
        m_ctlSub.reset();
        if (!m_inPort->hasSubscription())
            return true;

        // get controller subscription, if we have any
        m_checkCommands = false;
        if (m_ctlPort->hasSubscription()) {
            m_ctlSub = m_ctlPort->subscription();
            m_checkCommands = true;
        }

        m_inSub = m_inPort->subscription();
        m_recording = true;
        m_recordingFinished = false;

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
            m_vidDataset = getOrCreateDefaultDataset(name(), m_inSub->metadata());
        else
            m_vidDataset = getOrCreateDefaultDataset(m_settingsDialog->videoName());
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        if (!m_recording) {
            // just exit if we aren't subscribed to any data source
            setStateReady();
            m_recordingFinished = true;
            return;
        }
        m_recordingFinished = false;

        // base path to save our video to
        QString vidSavePathBase;

        // section suffix, in case a controller wants to slice the video manually
        QString currentSecSuffix;
        int secCount = 0;

        // state of the recording - we are supposed to be running, unless explicitly
        // requested to be stopped
        auto state = m_startStopped? RecordingState::STOPPED : RecordingState::RUNNING;

        // wait for the current run to actually launch
        startWaitCondition->wait(this);

        // immediately suspend our input subscription in case we are starting in STOPPED mode
        if (state != RecordingState::RUNNING) {
            m_inSub->suspend();
            statusMessage(QStringLiteral("Waiting for start command."));
        }

        while (m_running) {
            if (state != RecordingState::RUNNING) {
                // sanity check
                if (!m_checkCommands) {
                    // we just jump out of our stopped state in case we are not
                    // subscribed to a controlling module
                    state = RecordingState::RUNNING;
                    continue;
                }

                // wait for the next command
                const auto ctlCmd = m_ctlSub->next();
                if (!ctlCmd.has_value())
                    break; // we can quit here, a nullopt means we should terminate

                if (ctlCmd->kind == ControlCommandKind::START) {
                    if (state == RecordingState::PAUSED) {
                        // hurray, we can just resume normal operation!
                        state = RecordingState::RUNNING;
                        m_inSub->resume();
                        continue;
                    } else if (state == RecordingState::STOPPED) {
                        // we were stopped before, so we will now have to create a new
                        // section to store the new data in
                        secCount++;
                        currentSecSuffix = QStringLiteral("_sec%1").arg(secCount);

                        // we can only start a new section if we were already initialized
                        // if we weren't for some reason, the section initialization will simply
                        // be deferred to that point
                        if (m_initDone) {
                            // start our new section
                            if (!m_videoWriter->startNewSection(QStringLiteral("%1%2").arg(vidSavePathBase).arg(currentSecSuffix))) {
                                raiseError(QStringLiteral("Unable to initialize recording of a new section: %1").arg(QString::fromStdString(m_videoWriter->lastError())));
                                return;
                            }
                        }

                        // resume normal operation
                        state = RecordingState::RUNNING;
                        m_inSub->resume();
                        statusMessage(QStringLiteral("Recording video %1...").arg(secCount));
                        continue;
                    }
                }

                // we are not running, so don't execute the frame encoding code
                // until we received a START command again
                continue;
            }

            const auto maybeFrame = m_inSub->next();
            // getting a nullopt means we can quit this thread, as the experiment has stopped or
            // the data source has completed delivering data and will not send any more
            if (!maybeFrame.has_value())
                break;
            const auto frame = maybeFrame.value();

            if (m_checkCommands && m_ctlSub->hasPending()) {
                // process control commands - we only do this when we also have got a frame,
                // but we're not doing anything without a frame anyway, so this is fine
                const auto ctlCmd = m_ctlSub->peekNext();

                // we have to check for nullopt, because we may end up here because the
                // stream has ended (in which case we will terminate this thread very soon)
                if (ctlCmd.has_value()) {
                    if (ctlCmd->kind == ControlCommandKind::PAUSE) {
                        // switch to our paused state
                        state = RecordingState::PAUSED;
                        // stop receiving new data
                        m_inSub->suspend();
                        statusMessage(QStringLiteral("Recording paused."));
                        continue;
                    } else if (ctlCmd->kind == ControlCommandKind::STOP) {
                        // switch to our stopped state
                        state = RecordingState::STOPPED;
                        // stop receiving new data
                        m_inSub->suspend();
                        statusMessage(QStringLiteral("Recording stopped."));
                        continue;
                    }
                }
            }

            if (!m_initDone) {
                const auto mdata = m_inSub->metadata();
                auto frameSize = mdata.value("size", QSize()).toSize();
                const auto framerate = mdata.value("framerate", 0).toDouble();
                const auto depth = mdata.value("depth", CV_8U).toInt();
                const auto useColor = mdata.value("has_color", frame.mat.channels() > 1).toBool();

                if (!frameSize.isValid()) {
                    // we didn't get the dimensions from metadata - let's see if the current frame can
                    // be used to get dimensions.
                    frameSize = QSize(frame.mat.cols, frame.mat.rows);
                }

                if (!frameSize.isValid()) {
                    raiseError(QStringLiteral("Frame source did not provide image dimensions!"));
                    return;
                }
                if (framerate == 0) {
                    raiseError(QStringLiteral("Frame source did not provide a framerate!"));
                    return;
                }

                const auto dataBasename = dataBasenameFromSubMetadata(m_inSub->metadata(), "video");
                vidSavePathBase = m_vidDataset->pathForDataBasename(dataBasename);
                m_vidDataset->setDataScanPattern(QStringLiteral("%1*").arg(dataBasename));
                m_vidDataset->setAuxDataScanPattern(QStringLiteral("%1*.tsync").arg(dataBasename));

                auto vidSecFnameBase = vidSavePathBase;
                if (!currentSecSuffix.isEmpty())
                    vidSecFnameBase = QStringLiteral("%1%2").arg(vidSecFnameBase).arg(currentSecSuffix);

                try {
                    m_videoWriter->initialize(vidSecFnameBase,
                                              name(),
                                              m_vidDataset->collectionId(),
                                              frameSize.width(),
                                              frameSize.height(),
                                              framerate,
                                              depth,
                                              useColor,
                                              m_settingsDialog->saveTimestamps());
                } catch (const std::runtime_error& e) {
                    raiseError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
                    return;
                }

                // write info video info file with auxiliary information about the video we encoded
                // (this is useful to gather intel about the video without opening the video file)
                QVariantHash vInfo;
                vInfo.insert("frame_width", frameSize.width());
                vInfo.insert("frame_height", frameSize.height());
                vInfo.insert("framerate", framerate);
                vInfo.insert("colored", useColor);
                m_vidDataset->insertAttribute(QStringLiteral("video"), vInfo);

                // signal that we are actually recording this session
                m_initDone = true;
                if (secCount == 0)
                    statusMessage(QStringLiteral("Recording video..."));
                else
                    statusMessage(QStringLiteral("Recording video %1...").arg(secCount));
            }

            // encode current frame
            if (!m_videoWriter->encodeFrame(frame.mat, frame.time)) {
                if (m_videoWriter->lastError().empty())
                    raiseError(QStringLiteral("Unable to encode frame"));
                else
                    raiseError(QString::fromStdString(m_videoWriter->lastError()));
                m_running = false;
                break;
            }
        }

        m_recordingFinished = true;
    }

    void stop() override
    {
        // this will terminate the thread
        m_running = false;

        if (m_initDone && m_recording && m_videoWriter.get() != nullptr) {
            // wait until the thread has shut down and we are no longer encoding frames,
            // the finalize the video. Otherwise we might crash the encoder, as it isn't
            // threadsafe (for a tiny performance gain)
            while (!m_recordingFinished) { QCoreApplication::processEvents(); }

            // now shut down the recorder
            m_videoWriter->finalize();
        }

        statusMessage(QStringLiteral("Recording stopped."));
        m_videoWriter.reset(nullptr);

        // permit settings canges again
        m_settingsDialog->setEnabled(true);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        const auto codecProps = m_settingsDialog->codecProps();

        settings.insert("video_name_from_source", m_settingsDialog->videoNameFromSource());
        settings.insert("video_name", m_settingsDialog->videoName());
        settings.insert("save_timestamps", m_settingsDialog->saveTimestamps());
        settings.insert("start_stopped", m_settingsDialog->startStopped());

        settings.insert("video_codec", static_cast<int>(codecProps.codec()));
        settings.insert("video_container", static_cast<int>(m_settingsDialog->videoContainer()));
        settings.insert("lossless", codecProps.isLossless());
        settings.insert("vaapi_enabled", codecProps.useVaapi());

        settings.insert("slices_enabled", static_cast<int>(m_settingsDialog->slicingEnabled()));
        settings.insert("slices_interval", static_cast<int>(m_settingsDialog->sliceInterval()));
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDialog->setVideoNameFromSource(settings.value("video_name_from_source", true).toBool());
        m_settingsDialog->setVideoName(settings.value("video_name").toString());
        m_settingsDialog->setSaveTimestamps(settings.value("save_timestamps", true).toBool());
        m_settingsDialog->setStartStopped(settings.value("start_stopped", false).toBool());

        m_settingsDialog->setVideoContainer(static_cast<VideoContainer>(settings.value("video_container").toInt()));
        CodecProperties codecProps(static_cast<VideoCodec>(settings.value("video_codec").toInt()));
        codecProps.setLossless(settings.value("lossless").toBool());
        codecProps.setUseVaapi(settings.value("vaapi_enabled").toBool());
        m_settingsDialog->setCodecProps(codecProps);

        m_settingsDialog->setSlicingEnabled(settings.value("slices_enabled").toBool());
        m_settingsDialog->setSliceInterval(static_cast<uint>(settings.value("slices_interval").toInt()));

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

QString VideoRecorderModuleInfo::storageGroupName() const
{
    return QStringLiteral("videos");
}

AbstractModule *VideoRecorderModuleInfo::createModule(QObject *parent)
{
    return new VideoRecorderModule(parent);
}

#include "videorecordmodule.moc"
