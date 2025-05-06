/**
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "videorecordmodule.h"

#include "datactl/frametype.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

#include "equeueshared.h"
#include "recordersettingsdialog.h"
#include "videowriter.h"

SYNTALOS_MODULE(VideoRecorderModule)

enum class RecordingState {
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
    CodecProperties m_activeCodecProps;

    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    std::shared_ptr<StreamSubscription<Frame>> m_inSub;

    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;
    bool m_checkCommands;

    QString m_subjectName;

public:
    explicit VideoRecorderModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_recording(false),
          m_recordingFinished(true),
          m_settingsDialog(nullptr),
          m_subjectName(QString())
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
        // We prevent core affinity here, as using it would limit the encoder to one (or few) CPU cores set
        // by the engine, and encoding almost always benefits from having more CPU cores available.
        // The downside of this is that this may interfere with other modules which do have exclusive CPU
        // core affinity set, as this module may use "their" core's resources.
        return ModuleFeature::PROHIBIT_CPU_AFFINITY | ModuleFeature::SHOW_SETTINGS;
    }

    QString findEncodeHelperBinary()
    {
        QString binFname = moduleRootDir() + "/encodehelper/encodehelper";
        QFileInfo fi(binFname);
        if (!fi.exists())
            binFname = moduleRootDir() + "/encodehelper";
        return binFname;
    }

    bool prepare(const TestSubject &subject) override
    {
        if (!m_settingsDialog->videoNameFromSource() && m_settingsDialog->videoName().isEmpty()) {
            raiseError("Video recording name is not set. Please set it in the settings to continue.");
            return false;
        }

        if (!QDBusConnection::sessionBus().isConnected()) {
            raiseError(
                "Cannot connect to the D-Bus session bus.\nSomething is wrong with the system or session "
                "configuration.");
            return false;
        }

        m_videoWriter.reset(new VideoWriter);
        m_videoWriter->setContainer(m_settingsDialog->videoContainer());

        auto codecProps = m_settingsDialog->codecProps();
        codecProps.setThreadCount((potentialNoaffinityCPUCount() >= 2) ? potentialNoaffinityCPUCount() : 2);

        if (m_settingsDialog->deferredEncoding()) {
            // deferred encoding is enabled, so we actually have to save a raw video file
            m_videoWriter->setContainer(VideoContainer::Matroska);
            CodecProperties cprops(VideoCodec::Raw);
            codecProps = cprops;
        }
        m_videoWriter->setCodecProps(codecProps);

        // copy codec properties so the worker thread has direct access to a copy
        m_activeCodecProps = codecProps;

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

        m_subjectName = subject.id;

        // don't permit configuration changes while we are running
        m_settingsDialog->setEnabled(false);

        return true;
    }

    void start() override
    {
        AbstractModule::start();

        // we may be actually idle in case we e.g. aren't connected to any source
        if (!m_recording && (state() != ModuleState::ERROR))
            setStateDormant();

        if (m_inSub.get() == nullptr)
            return;

        if (m_settingsDialog->videoNameFromSource())
            m_vidDataset = createDefaultDataset(name(), m_inSub->metadata());
        else
            m_vidDataset = createDefaultDataset(m_settingsDialog->videoName());
        if (m_vidDataset.get() == nullptr)
            return;
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
        auto state = m_startStopped ? RecordingState::STOPPED : RecordingState::RUNNING;

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
                            if (!m_videoWriter->startNewSection(
                                    QStringLiteral("%1%2").arg(vidSavePathBase, currentSecSuffix))) {
                                raiseError(QStringLiteral("Unable to initialize recording of a new section: %1")
                                               .arg(QString::fromStdString(m_videoWriter->lastError())));
                                m_recordingFinished = true;
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
                    m_recordingFinished = true;
                    return;
                }
                if (framerate == 0) {
                    raiseError(QStringLiteral("Frame source did not provide a framerate!"));
                    m_recordingFinished = true;
                    return;
                }

                const auto inSubSrcModName = m_inSub->metadataValue(CommonMetadataKey::SrcModName).toString();
                const auto dataBasename = dataBasenameFromSubMetadata(
                    m_inSub->metadata(), QStringLiteral("%1-video").arg(m_vidDataset->collectionShortTag()));
                vidSavePathBase = m_vidDataset->pathForDataBasename(dataBasename);
                m_vidDataset->setDataScanPattern(
                    QStringLiteral("%1*").arg(dataBasename),
                    inSubSrcModName.isEmpty() ? QString()
                                              : QStringLiteral("Video recording from %1").arg(inSubSrcModName));
                m_vidDataset->addAuxDataScanPattern(
                    QStringLiteral("%1*.tsync").arg(dataBasename), QStringLiteral("Video timestamps"));

                auto vidSecFnameBase = vidSavePathBase;
                if (!currentSecSuffix.isEmpty())
                    vidSecFnameBase = QStringLiteral("%1%2").arg(vidSecFnameBase, currentSecSuffix);

                try {
                    m_videoWriter->initialize(
                        vidSecFnameBase,
                        name(),
                        inSubSrcModName,
                        m_vidDataset->collectionId(),
                        m_subjectName,
                        frameSize.width(),
                        frameSize.height(),
                        framerate,
                        depth,
                        useColor,
                        m_settingsDialog->saveTimestamps());
                } catch (const std::runtime_error &e) {
                    raiseError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
                    m_recordingFinished = true;
                    return;
                }

                // write info video info file with auxiliary information about the video we encoded
                // (this is useful to gather intel about the video without opening the video file)
                QVariantHash vInfo;
                vInfo.insert("frame_width", frameSize.width());
                vInfo.insert("frame_height", frameSize.height());
                vInfo.insert("framerate", framerate);
                vInfo.insert("colored", useColor);

                QVariantHash encInfo;
                encInfo.insert("name", m_videoWriter->selectedEncoderName());
                encInfo.insert("lossless", m_activeCodecProps.isLossless());
                encInfo.insert("thread_count", m_activeCodecProps.threadCount());
                if (m_activeCodecProps.useVaapi())
                    encInfo.insert("vaapi_enabled", true);
                if (m_activeCodecProps.mode() == CodecProperties::ConstantBitrate)
                    encInfo.insert("target_bitrate_kbps", m_activeCodecProps.bitrateKbps());
                else
                    encInfo.insert("target_quality", m_activeCodecProps.quality());
                m_vidDataset->insertAttribute(QStringLiteral("video"), vInfo);
                m_vidDataset->insertAttribute(QStringLiteral("encoder"), encInfo);

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
                m_recordingFinished = true;
                break;
            }
        }

        m_recordingFinished = true;
    }

    void enqueueVideosForDeferredEncoding()
    {
        if (isEphemeralRun()) {
            qDebug().noquote().nospace() << name() << ": "
                                         << "Not performing deferred encoding, run was ephemeral.";
            return;
        }
        if (m_vidDataset.get() == nullptr) {
            qDebug().noquote().nospace()
                << name() << ": "
                << "Not performing deferred encoding, video dataset was not set (we probably failed the run early).";
            return;
        }

        QEventLoop loop;
        QDBusServiceWatcher watcher(
            EQUEUE_DBUS_SERVICE, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForRegistration);
        connect(&watcher, &QDBusServiceWatcher::serviceRegistered, [&](const QString &busName) {
            if (busName != EQUEUE_DBUS_SERVICE)
                return;
            loop.quit();
        });

        auto iface = new QDBusInterface(
            EQUEUE_DBUS_SERVICE, "/", EQUEUE_DBUS_MANAGERINTF, QDBusConnection::sessionBus(), this);

        if (!iface->isValid()) {
            // service is not available, start detached queue processor
            // (will not do anything if process is already running)
            QProcess equeueProc;
            equeueProc.setProcessChannelMode(QProcess::ForwardedChannels);
            equeueProc.startDetached(findEncodeHelperBinary(), QStringList());

            // wait for the service to become available
            QTimer timer;
            timer.setSingleShot(true);
            connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

            // try to reach the encode helper a bunch of times
            for (uint i = 0; i < 10; i++) {
                if (iface->isValid())
                    break;
                QCoreApplication::processEvents();
                timer.start(2000);
                loop.exec();
            }
        }

        if (!iface->isValid()) {
            raiseError(
                QStringLiteral("Unable to connect to the encode queue service via D-Bus. "
                               "Videos of this run will remain unencoded. Did the encoding service crash? Message: %1")
                    .arg(QDBusConnection::sessionBus().lastError().message()));
            return;
        }

        // set maximum number of parallel encoding jobs
        iface->call("setParallelCount", m_settingsDialog->deferredEncodingParallelCount());

        // display some "project name" useful for humans
        const auto time = QDateTime::currentDateTime();
        const auto projectName = m_subjectName.isEmpty()
                                     ? QStringLiteral("%1 on %2")
                                           .arg(m_vidDataset->name(), time.toString("HH:mm yy-MM-dd"))
                                     : QStringLiteral("%1 @ %2 on %3")
                                           .arg(m_subjectName, m_vidDataset->name(), time.toString("HH:mm yy-MM-dd"));

        // we need to explicitly save the dataset here to ensure any globs are finalized into
        // actual data- and aux file parts.
        m_vidDataset->save();

        // schedule encoding jobs in the external encoder process
        for (auto &dataPart : m_vidDataset->dataFile().parts) {
            QVariantHash mdata;
            mdata["mod-name"] = QVariant::fromValue(name());
            mdata["src-mod-name"] = m_inSub->metadataValue(CommonMetadataKey::SrcModName).toString();
            mdata["collection-id"] = m_vidDataset->collectionId().toString(QUuid::WithoutBraces);
            mdata["subject-name"] = m_subjectName;
            mdata["save-timestamps"] = m_settingsDialog->saveTimestamps();
            mdata["video-container"] = static_cast<int>(m_settingsDialog->videoContainer());

            QDBusReply<bool> reply = iface->call(
                "enqueueVideo",
                projectName,
                m_vidDataset->pathForDataPart(dataPart),
                m_settingsDialog->codecProps().toVariant(),
                mdata);
            if (!reply.isValid() || !reply.value())
                raiseError(QStringLiteral("Unable to submit video data for encoding: %1").arg(reply.error().message()));
        }

        if (m_settingsDialog->deferredEncodingInstantStart()) {
            QDBusReply<bool> reply = iface->call("processVideos");
            if (!reply.isValid() || !reply.value())
                qWarning().noquote() << "Unable to request immediate video encoding:" << reply.error().message();
        }
    }

    void stop() override
    {
        // this will terminate the thread
        m_running = false;

        if (m_initDone && m_recording) {
            // wait until the thread has shut down and we are no longer encoding frames,
            // then finalize the video. Otherwise we might crash the encoder, as it isn't
            // threadsafe (for a tiny performance gain)
            while (!m_recordingFinished) {
                QCoreApplication::processEvents();
            }
        }
        if (m_videoWriter.get() != nullptr) {
            // now shut down the recorder
            m_videoWriter->finalize();
        }

        statusMessage(QStringLiteral("Recording stopped."));
        m_videoWriter.reset(nullptr);

        if (m_settingsDialog->deferredEncoding())
            enqueueVideosForDeferredEncoding();

        // drop reference on dataset
        m_vidDataset.reset();

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
        settings.insert("bitrate_kbps", codecProps.bitrateKbps());
        settings.insert("quality", codecProps.quality());
        settings.insert("mode", CodecProperties::modeToString(codecProps.mode()));
        if (codecProps.useVaapi())
            settings.insert("render_node", codecProps.renderNode());

        settings.insert("slices_enabled", static_cast<int>(m_settingsDialog->slicingEnabled()));
        settings.insert("slices_interval", static_cast<int>(m_settingsDialog->sliceInterval()));

        settings.insert("deferred_encode_enabled", m_settingsDialog->deferredEncoding());
        settings.insert("deferred_encode_instant_start", m_settingsDialog->deferredEncodingInstantStart());
        settings.insert("deferred_encode_parallel_count", m_settingsDialog->deferredEncodingParallelCount());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        // set codec first, which may apply some default settings
        CodecProperties codecProps(static_cast<VideoCodec>(settings.value("video_codec").toInt()));
        codecProps.setMode(CodecProperties::stringToMode(settings.value("mode").toString()));
        codecProps.setLossless(settings.value("lossless").toBool());
        codecProps.setUseVaapi(settings.value("vaapi_enabled").toBool());
        codecProps.setBitrateKbps(settings.value("bitrate_kbps", codecProps.bitrateKbps()).toInt());
        codecProps.setQuality(settings.value("quality", codecProps.quality()).toInt());
        if (codecProps.useVaapi())
            codecProps.setRenderNode(settings.value("render_node").toString());

        m_settingsDialog->setCodecProps(codecProps);

        // set user settings (possibly overriding codec defaults)
        m_settingsDialog->setVideoNameFromSource(settings.value("video_name_from_source", true).toBool());
        m_settingsDialog->setVideoName(settings.value("video_name").toString());
        m_settingsDialog->setSaveTimestamps(settings.value("save_timestamps", true).toBool());
        m_settingsDialog->setStartStopped(settings.value("start_stopped", false).toBool());

        m_settingsDialog->setVideoContainer(static_cast<VideoContainer>(settings.value("video_container").toInt()));
        m_settingsDialog->setSlicingEnabled(settings.value("slices_enabled").toBool());
        m_settingsDialog->setSliceInterval(static_cast<uint>(settings.value("slices_interval").toInt()));

        m_settingsDialog->setDeferredEncoding(settings.value("deferred_encode_enabled", false).toBool());
        m_settingsDialog->setDeferredEncodingInstantStart(
            settings.value("deferred_encode_instant_start", true).toBool());
        m_settingsDialog->setDeferredEncodingParallelCount(settings.value("deferred_encode_parallel_count", 4).toInt());

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

ModuleCategories VideoRecorderModuleInfo::categories() const
{
    return ModuleCategory::WRITERS;
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
