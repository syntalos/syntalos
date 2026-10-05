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
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

#include "equeueshared.h"
#include "utils/misc.h"
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
    RecorderSettingsDialog *m_settingsDialog;

    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    // the subscription we record from in the current run, to look at its metadata
    std::shared_ptr<StreamSubscription<Frame>> m_inSub;

    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;

    bool m_isEphemeralRun = false;

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

    bool prepare(const RunInfo &info) override
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

        auto videoWriter = std::make_unique<VideoWriter>();
        videoWriter->setLogger(m_log);
        videoWriter->setContainer(m_settingsDialog->videoContainer());

        auto codecProps = m_settingsDialog->codecProps();
        codecProps.setThreadCount((potentialNoaffinityCPUCount() >= 2) ? potentialNoaffinityCPUCount() : 2);

        if (m_settingsDialog->deferredEncoding()) {
            // Deferred encoding is enabled, so we store a fast lossless intermediate file now
            // and run the (expensive) final codec later. FFVHuff is nearly as cheap to write as
            // uncompressed video, but needs considerably less disk space.
            videoWriter->setContainer(VideoContainer::Matroska);
            CodecProperties cprops(VideoCodec::FFVHuff);
            cprops.setExactColors(codecProps.isLossless() && codecProps.exactColors());
            cprops.setThreadCount(codecProps.threadCount());
            codecProps = cprops;
        }
        videoWriter->setCodecProps(codecProps);

        videoWriter->setFileSliceInterval(0); // no slicing allowed, unless changed later
        if (m_settingsDialog->slicingEnabled())
            videoWriter->setFileSliceInterval(m_settingsDialog->sliceInterval());

        m_inSub.reset();
        if (m_inPort->isDormant()) {
            // we aren't subscribed to any data source, so there is nothing for us to do this run
            setStateDormant();
            return true;
        }

        // get controller subscription, if we have any
        std::shared_ptr<StreamSubscription<ControlCommand>> ctlSub;
        if (m_ctlPort->hasSubscription())
            ctlSub = m_ctlPort->subscription();

        // we can record!
        m_inSub = m_inPort->subscription();
        m_isEphemeralRun = info.isEphemeral;

        setWorker(
            Worker{
                .inSub = m_inSub,
                .ctlSub = ctlSub,
                .checkCommands = ctlSub != nullptr,
                .startStopped = m_settingsDialog->startStopped(),
                .saveTimestamps = m_settingsDialog->saveTimestamps(),
                .subjectName = info.subject.id,
                // copy codec properties so the worker thread has direct access to a copy
                .activeCodecProps = codecProps,
                .videoWriter = std::move(videoWriter),
            });

        // don't permit configuration changes while we are running
        m_settingsDialog->setEnabled(false);

        return true;
    }

    void start() override
    {
        AbstractModule::start();

        // we may be actually idle in case we e.g. aren't connected to any source
        if (!m_inSub && (state() != ModuleState::ERROR))
            setStateDormant();

        if (!m_inSub)
            return;

        std::shared_ptr<EDLDataset> vidDataset;
        if (m_settingsDialog->videoNameFromSource())
            vidDataset = createDefaultDataset(name(), m_inSub->metadata());
        else
            vidDataset = createDefaultDataset(m_settingsDialog->videoName());

        // the basename of the files we record into, unless the frame source has suggested one
        std::string dataBasename;
        if (vidDataset) {
            dataBasename = dataBasenameFromSubMetadata(
                m_inSub->metadata(),
                std::format(
                    "{}-{}",
                    vidDataset->collectionShortTag(),
                    simplifyStrForFileBasename(vidDataset->name(), true, 22)));
        }

        // our worker is waiting for the run to start, give it the dataset to record into
        modifyWorker<Worker>([&](Worker &w) {
            w.vidDataset = vidDataset;
            w.dataBasename = dataBasename;
        });
    }

    void enqueueVideosForDeferredEncoding(const std::shared_ptr<EDLDataset> &vidDataset, const QString &subjectName)
    {
        if (m_isEphemeralRun) {
            LOG_INFO(m_log, "Not performing deferred encoding, run was ephemeral.");
            return;
        }
        if (vidDataset == nullptr) {
            LOG_INFO(
                m_log,
                "Not performing deferred encoding, video dataset was not set (we probably failed the run early).");
            return;
        }

        QEventLoop loop;
        QDBusServiceWatcher watcher(
            EQUEUE_DBUS_SERVICE,
            QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration);
        connect(&watcher, &QDBusServiceWatcher::serviceRegistered, [&](const QString &busName) {
            if (busName != EQUEUE_DBUS_SERVICE)
                return;
            loop.quit();
        });

        auto iface = new QDBusInterface(
            EQUEUE_DBUS_SERVICE,
            "/",
            EQUEUE_DBUS_MANAGERINTF,
            QDBusConnection::sessionBus(),
            this);

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
            raiseError(QStringLiteral(
                           "Unable to connect to the encode queue service via D-Bus. "
                           "Videos of this run will remain unencoded. Did the encoding service crash? Message: %1")
                           .arg(QDBusConnection::sessionBus().lastError().message()));
            return;
        }

        // set maximum number of parallel encoding jobs
        iface->call("setParallelCount", m_settingsDialog->deferredEncodingParallelCount());

        // display some "project name" useful for humans
        const auto time = QDateTime::currentDateTime();
        const auto projectName = subjectName.isEmpty() ? QStringLiteral("%1 on %2")
                                                             .arg(
                                                                 QString::fromStdString(vidDataset->name()),
                                                                 time.toString("HH:mm yy-MM-dd"))
                                                       : QStringLiteral("%1 @ %2 on %3")
                                                             .arg(
                                                                 subjectName,
                                                                 QString::fromStdString(vidDataset->name()),
                                                                 time.toString("HH:mm yy-MM-dd"));

        // we need to explicitly save the dataset here to ensure any globs are finalized into
        // actual data- and aux file parts.
        const auto dsSaveRes = vidDataset->save();
        if (!dsSaveRes) {
            raiseError(
                std::format(
                    "Unable to save video dataset prior to deferred encoding, so no encoding jobs "
                    "were scheduled. Videos of this run will remain unencoded. Message: {}",
                    dsSaveRes.error()));
            return;
        }

        // schedule encoding jobs in the external encoder process
        for (auto &dataPart : vidDataset->dataFile().parts) {
            QVariantHash mdata;
            mdata["mod-name"] = QVariant::fromValue(name());
            mdata["src-mod-name"] = QString::fromStdString(
                m_inSub->metadataValue(CommonMetadataKey::SrcModName, std::string{}));
            mdata["collection-id"] = QString::fromStdString(vidDataset->collectionId().toHex());
            mdata["subject-name"] = subjectName;
            mdata["save-timestamps"] = m_settingsDialog->saveTimestamps();
            mdata["video-container"] = static_cast<int>(m_settingsDialog->videoContainer());

            QDBusReply<bool> reply = iface->call(
                "enqueueVideo",
                projectName,
                QString::fromStdString(vidDataset->pathForDataPart(dataPart)),
                m_settingsDialog->codecProps().toVariant(),
                mdata);
            if (!reply.isValid() || !reply.value())
                raiseError(QStringLiteral("Unable to submit video data for encoding: %1").arg(reply.error().message()));
        }

        if (m_settingsDialog->deferredEncodingInstantStart()) {
            QDBusReply<bool> reply = iface->call("processVideos");
            if (!reply.isValid() || !reply.value())
                LOG_WARNING(m_log, "Unable to request immediate video encoding: {}", reply.error().message());
        }
    }

    /**
     * Runs the recording state machine and encodes the frames it receives, in the module's thread.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<StreamSubscription<Frame>> inSub;
        std::shared_ptr<StreamSubscription<ControlCommand>> ctlSub;
        bool checkCommands;
        bool startStopped;
        bool saveTimestamps;
        QString subjectName;
        CodecProperties activeCodecProps;

        // The video writer isn't threadsafe (for a tiny performance gain), so only the owner of this
        // worker uses it: Our thread during the run, and the module once the thread has ended.
        std::unique_ptr<VideoWriter> videoWriter;

        // The dataset is only created when the run is started, as it can be named after metadata of the frame source
        std::shared_ptr<EDLDataset> vidDataset{};
        std::string dataBasename{};

        bool initDone = false;
        qint64 framesReceived = 0;
        qint64 framesEncoded = 0;

        void run()
        {
            // base path to save our video to
            std::string vidSavePathBase;

            // section suffix, in case a controller wants to slice the video manually
            std::string currentSecSuffix;
            int secCount = 0;

            // set when a new section was requested but its file has not been created yet - we defer
            // creation until the section's first frame actually arrives, so that sections which never
            // receive a frame do not leave an empty, header-only file on disk.
            bool pendingNewSection = false;

            // state of the recording - we are supposed to be running, unless explicitly
            // requested to be stopped
            auto state = startStopped ? RecordingState::STOPPED : RecordingState::RUNNING;

            // wait for the current run to actually launch
            mod.waitForStart();

            // immediately suspend our input subscription in case we are starting in STOPPED mode
            if (state != RecordingState::RUNNING) {
                inSub->suspend();
                mod.setStatusMessage(QStringLiteral("Waiting for start command."));
            }

            // exit immediately if we don't have a dataset
            if (!vidDataset) {
                // an error is already emitted at this point, via createDefaultDataset()
                return;
            }

            while (mod.running()) {
                if (state != RecordingState::RUNNING) {
                    // sanity check
                    if (!checkCommands) {
                        // we just jump out of our stopped state in case we are not
                        // subscribed to a controlling module
                        state = RecordingState::RUNNING;
                        continue;
                    }

                    // wait for the next command
                    const auto ctlCmd = ctlSub->next();
                    if (!ctlCmd.has_value())
                        break; // we can quit here, a nullopt means we should terminate

                    if (ctlCmd->kind == ControlCommandKind::START) {
                        if (state == RecordingState::PAUSED) {
                            // hurray, we can just resume normal operation!
                            state = RecordingState::RUNNING;
                            inSub->resume();
                            continue;
                        } else if (state == RecordingState::STOPPED) {
                            // we were stopped before, so we will now have to create a new
                            // section to store the new data in
                            secCount++;
                            currentSecSuffix = std::format("_sec{}", secCount);

                            // Defer creating the new section's file until its first frame actually
                            // arrives, if we were already initialized. If we weren't for some reason,
                            // the section initialization will simply be deferred to the regular first-frame
                            // init path below (which folds the section suffix into the filename); otherwise
                            // we flag a pending section that startNewSection() will create once we have a
                            // frame to write.
                            if (initDone)
                                pendingNewSection = true;

                            // resume normal operation
                            state = RecordingState::RUNNING;
                            inSub->resume();
                            mod.setStatusMessage(QStringLiteral("Recording video %1...").arg(secCount));
                            continue;
                        }
                    }

                    // we are not running, so don't execute the frame encoding code
                    // until we received a START command again
                    continue;
                }

                const auto maybeFrame = inSub->next();
                // getting a nullopt means we can quit this thread, as the experiment has stopped or
                // the data source has completed delivering data and will not send any more
                if (!maybeFrame.has_value())
                    break;
                const auto &frame = maybeFrame.value();
                framesReceived++;

                if (checkCommands && ctlSub->hasPending()) {
                    // process control commands - we only do this when we also have got a frame,
                    // but we're not doing anything without a frame anyway, so this is fine
                    const auto ctlCmd = ctlSub->peekNext();

                    // we have to check for nullopt, because we may end up here because the
                    // stream has ended (in which case we will terminate this thread very soon)
                    if (ctlCmd.has_value()) {
                        if (ctlCmd->kind == ControlCommandKind::PAUSE) {
                            // switch to our paused state
                            state = RecordingState::PAUSED;
                            // stop receiving new data
                            inSub->suspend();
                            mod.setStatusMessage(QStringLiteral("Recording paused."));
                            continue;
                        } else if (ctlCmd->kind == ControlCommandKind::STOP) {
                            // switch to our stopped state
                            state = RecordingState::STOPPED;
                            // stop receiving new data
                            inSub->suspend();
                            mod.setStatusMessage(QStringLiteral("Recording stopped."));
                            continue;
                        }
                    }
                }

                if (!initDone) {
                    const auto mdata = inSub->metadata();
                    auto frameSize = mdata.valueOr<MetaSize>("size", {});
                    const auto framerate = mdata.valueOr<double>("framerate", 0.0);
                    const auto depth = static_cast<int>(mdata.valueOr<int64_t>("depth", CV_8U));
                    const auto useColor = mdata.valueOr<bool>("has_color", frame.mat.channels() > 1);

                    if (frameSize.isEmpty()) {
                        // we didn't get the dimensions from metadata - let's see if the current frame can
                        // be used to get dimensions.
                        frameSize = MetaSize(frame.mat.cols, frame.mat.rows);
                    }

                    if (frameSize.isEmpty()) {
                        mod.raiseError(QStringLiteral("Frame source did not provide image dimensions!"));
                        return;
                    }
                    if (framerate == 0) {
                        mod.raiseError(QStringLiteral("Frame source did not provide a framerate!"));
                        return;
                    }

                    const auto inSubSrcModName = inSub->metadataValue<std::string>(CommonMetadataKey::SrcModName, {});
                    vidSavePathBase = vidDataset->pathForDataBasename(dataBasename);
                    vidDataset->setDataScanPattern(
                        dataBasename + "*",
                        inSubSrcModName.empty() ? std::string()
                                                : std::format("Video recording from {}", inSubSrcModName));
                    vidDataset->addAuxDataScanPattern(std::format("{}*.tsync", dataBasename), "Video timestamps");

                    auto vidSecFnameBase = vidSavePathBase;
                    if (!currentSecSuffix.empty())
                        vidSecFnameBase = vidSecFnameBase + currentSecSuffix;

                    try {
                        videoWriter->initialize(
                            QString::fromStdString(vidSecFnameBase),
                            mod.moduleName(),
                            QString::fromStdString(inSubSrcModName),
                            vidDataset->collectionId(),
                            subjectName,
                            frameSize.width,
                            frameSize.height,
                            framerate,
                            depth,
                            useColor,
                            saveTimestamps);
                    } catch (const std::runtime_error &e) {
                        mod.raiseError(std::format("Unable to initialize recording: {}", e.what()));
                        return;
                    }

                    // write info video info file with auxiliary information about the video we encoded
                    // (this is useful to gather intel about the video without opening the video file)
                    MetaStringMap vInfo;
                    vInfo["frame_width"] = frameSize.width;
                    vInfo["frame_height"] = frameSize.height;
                    vInfo["framerate"] = framerate;
                    vInfo["colored"] = useColor;

                    MetaStringMap encInfo;
                    encInfo["name"] = videoWriter->selectedEncoderName().toStdString();
                    // NOTE: We read the lossless flag back from the writer, as it may have adjusted
                    // the setting to match what the selected encoder is actually capable of.
                    encInfo["lossless"] = videoWriter->codecProps().isLossless();
                    if (useColor)
                        encInfo["exact_colors"] = videoWriter->hasExactColors();
                    encInfo["thread_count"] = activeCodecProps.threadCount();
                    if (activeCodecProps.useVaapi())
                        encInfo["vaapi_enabled"] = true;
                    if (activeCodecProps.mode() == CodecProperties::ConstantBitrate)
                        encInfo["target_bitrate_kbps"] = activeCodecProps.bitrateKbps();
                    else
                        encInfo["target_quality"] = activeCodecProps.quality();
                    vidDataset->insertAttribute("video", vInfo);
                    vidDataset->insertAttribute("encoder", encInfo);

                    // signal that we are actually recording this session
                    initDone = true;
                    if (secCount == 0)
                        mod.setStatusMessage(QStringLiteral("Recording video..."));
                    else
                        mod.setStatusMessage(QStringLiteral("Recording video %1...").arg(secCount));
                }

                // create the file for a freshly-requested section now that we have a frame to write
                if (pendingNewSection) {
                    pendingNewSection = false;
                    if (!videoWriter->startNewSection(QStringLiteral("%1%2").arg(
                            QString::fromStdString(vidSavePathBase),
                            QString::fromStdString(currentSecSuffix)))) {
                        mod.raiseError(
                            std::format(
                                "Unable to initialize recording of a new section: {}",
                                videoWriter->lastError()));
                        return;
                    }
                }

                // encode current frame
                if (!videoWriter->encodeFrame(frame.mat, frame.time)) {
                    if (videoWriter->lastError().empty())
                        mod.raiseError(QStringLiteral("Unable to encode frame"));
                    else
                        mod.raiseError(videoWriter->lastError());
                    return;
                }
                framesEncoded++;
            }
        }
    };

    void stop() override
    {
        // our thread has shut down at this point and we are no longer encoding frames,
        // so we can finalize the video. Doing that any earlier might crash the encoder, as it isn't
        // threadsafe (for a tiny performance gain)
        auto worker = takeWorker<Worker>();

        bool finalizeOk = true;
        if (worker && worker->videoWriter.get() != nullptr) {
            // now shut down the recorder
            const auto res = worker->videoWriter->finalize();
            if (!res) {
                finalizeOk = false;
                raiseError(
                    std::format(
                        "Failed to finalize the recorded video file: {}\n"
                        "The intermediate video data has been left on disk for manual recovery, but deferred "
                        "encoding will be skipped to avoid processing a potentially corrupt file.",
                        res.error()));
            }
        }

        statusMessage(QStringLiteral("Recording stopped."));
        if (worker)
            worker->videoWriter.reset(nullptr);

        setRunStatistic(QStringLiteral("frames_received"), worker ? worker->framesReceived : qint64(0));
        setRunStatistic(QStringLiteral("frames_encoded"), worker ? worker->framesEncoded : qint64(0));

        if (finalizeOk && m_settingsDialog->deferredEncoding()) {
            if (worker)
                enqueueVideosForDeferredEncoding(worker->vidDataset, worker->subjectName);
            else
                enqueueVideosForDeferredEncoding(nullptr, QString());
        }

        // drop reference on dataset
        worker.reset();

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
        settings.insert("exact_colors", codecProps.exactColors());
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
        auto codec = static_cast<VideoCodec>(settings.value("video_codec").toInt());
        if (codec <= VideoCodec::Unknown || codec >= VideoCodec::Last || codec == VideoCodec::FFVHuff) {
            LOG_WARNING(
                m_log,
                "Ignoring invalid video codec setting '{}', falling back to FFV1.",
                settings.value("video_codec").toString());
            codec = VideoCodec::FFV1;
        }
        CodecProperties codecProps(codec);
        codecProps.setMode(CodecProperties::stringToMode(settings.value("mode").toString()));
        codecProps.setLossless(settings.value("lossless").toBool());
        codecProps.setExactColors(settings.value("exact_colors", codecProps.isLossless()).toBool());
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
