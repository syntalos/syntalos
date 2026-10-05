/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "spikeglxmodule.h"

#include <QDate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrentRun>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>

#include "datactl/datatypes.h"
#include "datactl/timesync.h"
#include "datactl/tsyncfile.h"
#include "globalconfig.h"
#include "sglxclient.h"
#include "sglxutils.h"
#include "spikeglxsettingsdialog.h"
#include "utils/misc.h"

SYNTALOS_MODULE(SpikeGLXModule)

namespace
{

/// Description of one stream as reported by SpikeGLX.
struct StreamInfo {
    Sglx::StreamId sid;
    QString name;
    double sampleRate = 0;
    std::vector<int> acqCounts;
    int totalChans = 0;
    int savedChans = 0;
    QString serial;
    int slotOrType = 0;
};

/// One live-data output port.
struct FetchPort {
    QString portId;
    Sglx::StreamId sid;
    QString streamName;
    SglxUtils::ChanGroup group = SglxUtils::ChanGroup::ALL;
    bool digital = false; /// SY/DW group: published as line events, not sample blocks
    /// exactly one of the two is set, depending on `digital`
    std::shared_ptr<DataStream<SignalBlockI16>> stream;
    std::shared_ptr<DataStream<LineReading>> lineStream;
};

/// One live-data output port together with everything needed to fetch its data in a run.
/// The module creates these for every run, they are then owned by the worker of the run.
struct FetchStream : FetchPort {
    explicit FetchStream(const FetchPort &port)
        : FetchPort(port)
    {
    }

    std::vector<int> relChans; /// word/channel indices relative to the group
    std::vector<int> absChans; /// word/channel indices within the stream
    std::vector<int> lines;    /// digital only: selected line numbers (word * 16 + bit)

    // run state
    double sampleRate = 0;
    uint64_t cursor = 0;
    uint64_t refSampleCount = 0;
    int64_t startSampleOffset = 0;
    int maxSamps = 1;
    std::unique_ptr<FreqCounterSynchronizer> syncer;
    std::vector<int16_t> buffer;
    SignalBlockI16 block;
    std::vector<uint16_t> lineMask; /// per fetched word: the bits we publish
    std::vector<uint16_t> linePrev; /// per fetched word: last seen level
    bool linePrimed = false;
    uint64_t gapCount = 0;
    uint64_t droppedSamples = 0;
    uint64_t fetchedSamples = 0;
    uint64_t emittedEvents = 0;
};

/// One stream whose sample counter is logged against the master clock.
struct SyncStream {
    Sglx::StreamId sid;
    QString name;
    double sampleRate = 0;
    std::unique_ptr<TimeSyncFileWriter> writer;
};

} // namespace

static std::string groupPortSuffix(SglxUtils::ChanGroup group)
{
    return SglxUtils::chanGroupName(group).toLower().toStdString();
}

/**
 * Leave SpikeGLX with an unused run name. SpikeGLX verifies its remembered
 * run name whenever parameters are (re)validated and refuses names that already
 * exist on disk, which would otherwise break remote device detection after a
 * SpikeGLX restart, and requires the user to pick a new name for manual runs.
 */
static void setPlaceholderRunName(Sglx::Client &client, QuillLogger *log)
{
    const auto placeholder = QStringLiteral("syntalos_next_%1")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    if (auto r = client.setRunName(placeholder.toStdString()); !r)
        LOG_WARNING(log, "Unable to set placeholder run name in SpikeGLX: {}", r.error());
}

class SpikeGLXModule : public AbstractModule
{
    Q_OBJECT
private:
    using RunControlMode = SpikeGLXSettingsDialog::RunControlMode;

    SpikeGLXSettingsDialog *m_settingsDlg;
    /// Client for the actions of the settings dialog, which are only available between runs.
    /// Every run uses a client of its own, which is created in prepare() and owned by its worker.
    Sglx::Client m_client;
    std::atomic_bool m_dialogBusy{false};
    bool m_runActive = false;

    std::vector<FetchPort> m_fetchPorts;

    // state used while preparing a run
    std::vector<StreamInfo> m_streams;
    MetaStringMap m_portsMeta;

public:
    explicit SpikeGLXModule(ModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_settingsDlg = new SpikeGLXSettingsDialog(modInfo);
        addSettingsWindow(m_settingsDlg);
        m_settingsDlg->setHost(QStringLiteral("localhost"));
        m_settingsDlg->setSyncStreams({QStringLiteral("imec0")});

        connect(m_settingsDlg, &SpikeGLXSettingsDialog::fetchEntriesChanged, this, [this] {
            rebuildOutputPorts();
        });
        connect(m_settingsDlg, &SpikeGLXSettingsDialog::testConnectionRequested, this, [this] {
            runDialogAction(true);
        });
        connect(m_settingsDlg, &SpikeGLXSettingsDialog::queryStreamsRequested, this, [this] {
            runDialogAction(false);
        });
    }

    ~SpikeGLXModule() override
    {
        // wait for a pending dialog action so it does not outlive the client
        while (m_dialogBusy)
            QThread::msleep(10);
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    /**
     * Derive one output port per configured live-data entry.
     */
    void rebuildOutputPorts()
    {
        QSet<QString> previousIds;
        for (const auto &fs : m_fetchPorts)
            previousIds.insert(fs.portId);

        std::vector<FetchPort> newStreams;
        QSet<QString> currentIds;
        const auto entries = m_settingsDlg->fetchEntries();
        for (const auto &entry : entries) {
            const auto sid = SglxUtils::parseStreamName(entry.stream);
            const auto group = SglxUtils::parseChanGroup(entry.group);
            if (!sid || !group)
                continue;

            FetchPort fs;
            fs.sid = *sid;
            fs.streamName = SglxUtils::streamName(*sid);
            fs.group = *group;
            fs.portId = QStringLiteral("%1-%2").arg(fs.streamName, qstr(groupPortSuffix(*group)));
            for (int n = 2; currentIds.contains(fs.portId); ++n)
                fs.portId = QStringLiteral("%1-%2-%3").arg(fs.streamName, qstr(groupPortSuffix(*group))).arg(n);
            currentIds.insert(fs.portId);

            const auto title = QStringLiteral("%1 %2").arg(fs.streamName, SglxUtils::chanGroupName(*group));
            fs.digital = SglxUtils::isDigitalGroup(*group);
            if (fs.digital)
                fs.lineStream = registerOutputPort<LineReading>(fs.portId, title);
            else
                fs.stream = registerOutputPort<SignalBlockI16>(fs.portId, title);
            newStreams.push_back(std::move(fs));
        }

        for (const auto &oldId : previousIds) {
            if (!currentIds.contains(oldId))
                removeOutPortById(oldId);
        }
        m_fetchPorts = std::move(newStreams);
    }

    /**
     * Run "Test connection" / "Query streams" from the settings dialog.
     */
    void runDialogAction(bool testOnly)
    {
        if (m_runActive) {
            m_settingsDlg->setConnectionStatus(QStringLiteral("Not available while a run is active."), false);
            return;
        }
        if (m_dialogBusy)
            return;
        m_dialogBusy = true;
        m_settingsDlg->setConnectionStatus(QStringLiteral("Connecting…"), true);

        const auto host = m_settingsDlg->host().toStdString();
        const auto port = m_settingsDlg->port();
        const auto timeout = std::chrono::milliseconds(m_settingsDlg->connectTimeoutMs());
        const auto devString = m_settingsDlg->deviceString();

        // worker thread, so the UI stays responsive while we wait for the network
        auto future = QtConcurrent::run([this, host, port, timeout, testOnly, devString] {
            QString status;
            QString streamsText;
            QStringList streamNames;
            bool ok = false;

            do {
                if (auto r = m_client.connect(host, port, timeout); !r) {
                    status = qstr(r.error());
                    break;
                }
                auto initialized = m_client.isInitialized();
                if (!initialized) {
                    status = qstr(initialized.error());
                    break;
                }
                auto probes = m_client.probeList();
                status = QStringLiteral("Connected: %1\nProbes: %2")
                             .arg(qstr(m_client.version()))
                             .arg(probes ? qstr(*probes) : qstr(probes.error()));
                ok = true;

                if (!testOnly) {
                    std::vector<StreamInfo> streams;
                    QString err;
                    if (auto running = m_client.isRunning(); running && !*running) {
                        if (auto r = ensureDevicesSelected(m_client, devString); !r) {
                            streamsText = qstr(r.error());
                            break;
                        }
                    }
                    if (!enumerateStreams(m_client, streams, err)) {
                        streamsText = err;
                        break;
                    }
                    if (streams.empty())
                        streamsText = QStringLiteral("SpikeGLX reports no enabled streams.");
                    QStringList lines;
                    for (const auto &si : streams) {
                        streamNames << si.name;
                        QStringList groups;
                        const auto gl = SglxUtils::chanGroupsForStream(si.sid.js);
                        for (size_t i = 0; i < gl.size() && i < si.acqCounts.size(); ++i)
                            groups
                                << QStringLiteral("%1: %2").arg(SglxUtils::chanGroupName(gl[i])).arg(si.acqCounts[i]);
                        lines << QStringLiteral("%1 - %2 Hz, %3 channels (%4)%5")
                                     .arg(si.name)
                                     .arg(si.sampleRate, 0, 'f', 1)
                                     .arg(si.totalChans)
                                     .arg(groups.join(QStringLiteral(", ")))
                                     .arg(si.serial.isEmpty() ? QString() : QStringLiteral(", SN %1").arg(si.serial));
                    }
                    streamsText = lines.join('\n');
                }
            } while (false);

            QMetaObject::invokeMethod(
                this,
                [this, status, ok, streamsText, streamNames, testOnly] {
                    m_settingsDlg->setConnectionStatus(status, ok);
                    if (!testOnly)
                        m_settingsDlg->setStreamsInfo(streamsText, streamNames);
                    m_dialogBusy = false;
                },
                Qt::QueuedConnection);
        });
    }

    /**
     * Make sure SpikeGLX has validated run parameters. If a device string is
     * configured, remotely perform "Detect" and "Verify | Save" with it.
     * Must only be called while SpikeGLX is idle.
     */
    Sglx::Client::Result<void> ensureDevicesSelected(Sglx::Client &client, const QString &devString)
    {
        // probe whether parameters were validated at all
        auto np = client.streamCount(Sglx::JS_IM);
        if (np || np.error().find("never validated") == std::string::npos)
            return {};

        if (devString.isEmpty())
            return std::unexpected(
                std::string(
                    "SpikeGLX has not validated its run parameters. Either click 'Detect' and 'Verify | Save' in its "
                    "acquisition configuration, or set the devices to select in this module's settings so this can "
                    "happen automatically."));

        LOG_INFO(m_log, "SpikeGLX parameters are not validated, selecting devices: {}", devString);
        if (auto r = client.selectDevices(devString.toStdString(), 1); !r) {
            auto err = "Remote device detection failed: " + r.error();
            if (r.error().find("already in use") != std::string::npos)
                err +=
                    " (SpikeGLX verifies the run name it remembers, which already exists on disk. Set an unused "
                    "run name in SpikeGLX and click 'Verify | Save' once.)";
            return std::unexpected(err);
        }
        return {};
    }

    /**
     * Query the layout of all enabled streams from SpikeGLX.
     * Works while SpikeGLX is idle, as long as its parameters were validated.
     */
    bool enumerateStreams(Sglx::Client &client, std::vector<StreamInfo> &streams, QString &error)
    {
        streams.clear();
        for (const int js : {Sglx::JS_IM, Sglx::JS_OB, Sglx::JS_NI}) {
            auto np = client.streamCount(js);
            if (!np) {
                error = qstr(np.error());
                return false;
            }
            for (int ip = 0; ip < *np; ++ip) {
                StreamInfo si;
                si.sid = Sglx::StreamId{js, ip};
                si.name = SglxUtils::streamName(si.sid);

                auto rate = client.sampleRate(si.sid);
                if (!rate) {
                    error = QStringLiteral("%1: %2").arg(si.name, qstr(rate.error()));
                    return false;
                }
                si.sampleRate = *rate;

                auto counts = client.acqChanCounts(si.sid);
                if (!counts) {
                    error = QStringLiteral("%1: %2").arg(si.name, qstr(counts.error()));
                    return false;
                }
                si.acqCounts = *counts;
                for (const auto c : si.acqCounts)
                    si.totalChans += c;

                if (auto saved = client.saveChans(si.sid))
                    si.savedChans = static_cast<int>(saved->size());

                if (js != Sglx::JS_NI) {
                    if (auto sn = client.streamSN(si.sid)) {
                        si.serial = qstr(sn->serial);
                        si.slotOrType = sn->slotOrType;
                    }
                }
                streams.push_back(std::move(si));
            }
        }
        return true;
    }

    const StreamInfo *findStream(Sglx::StreamId sid) const
    {
        for (const auto &si : m_streams) {
            if (si.sid == sid)
                return &si;
        }
        return nullptr;
    }

    /**
     * Build the SpikeGLX run name for this recording:
     * <yyyyMMdd>_<subject>_<experiment>_<collection short tag>[_<extra>], leaving out unavailable parts.
     */
    QString makeRunName(const RunInfo &info, const std::shared_ptr<EDLDataset> &dataset) const
    {
        const auto now = QDateTime::currentDateTime();
        QStringList parts;
        parts << now.toString(QStringLiteral("yyyyMMdd"));

        const auto subjectId = info.subject.id.trimmed();
        if (!subjectId.isEmpty())
            parts << subjectId;

        const auto experimentId = info.experimentId.trimmed();
        if (!experimentId.isEmpty())
            parts << experimentId;

        if (dataset) {
            const auto tag = qstr(dataset->collectionShortTag());
            if (!tag.isEmpty())
                parts << tag;
        }

        const auto extra = m_settingsDlg->runNameExtra();
        if (!extra.isEmpty())
            parts << extra;

        if (info.isEphemeral) {
            // the user probably wants to delete this, but we won't know for sure (so we label the temporary run)
            auto time = QDateTime::currentDateTime();
            parts.prepend("temp");
            parts << time.toString("hhmm");
        }

        return SglxUtils::sanitizeRunName(parts.join('_'));
    }

    bool prepare(const RunInfo &info) override
    {
        m_streams.clear();

        if (m_dialogBusy) {
            raiseError(QStringLiteral("A connection test is still in progress, please wait for it to finish."));
            return false;
        }

        // Every run talks to SpikeGLX via a client of its own, which is handed over to the worker
        // of the run once we are done preparing. The connection that the settings dialog may have
        // left open is closed, so we still never have more than one connection to SpikeGLX.
        m_client.close();
        auto client = std::make_unique<Sglx::Client>();
        bool sglxRunStartedByUs = false;

        m_runActive = true;
        m_settingsDlg->setRunActive(true);
        auto cleanupOnFailure = qScopeGuard([&] {
            if (sglxRunStartedByUs) {
                LOG_INFO(m_log, "Stopping the SpikeGLX run again after failed preparation");
                if (auto r = client->stopRun(); !r)
                    LOG_WARNING(m_log, "Unable to stop SpikeGLX run: {}", r.error());
                else
                    setPlaceholderRunName(*client, m_log);
                sglxRunStartedByUs = false;
            }
            m_runActive = false;
            m_settingsDlg->setRunActive(false);
        });

        // settings snapshot
        auto mode = m_settingsDlg->runControlMode(); // becomes the effective mode of the run below
        const int syncIntervalMs = m_settingsDlg->syncIntervalMs();
        const bool fetchEnabled = m_settingsDlg->fetchEnabled();
        const int fetchIntervalMs = m_settingsDlg->fetchIntervalMs();
        const int fetchMaxBlockMs = m_settingsDlg->fetchMaxBlockMs();
        const bool abortOnOverrun = m_settingsDlg->overrunPolicy() == SpikeGLXSettingsDialog::AbortRun;
        const auto host = m_settingsDlg->host();
        const auto port = m_settingsDlg->port();

        if (host.isEmpty()) {
            raiseError(QStringLiteral("No SpikeGLX host is set."));
            return false;
        }

        // connect
        setStatusMessage(QStringLiteral("Connecting to %1:%2…").arg(host).arg(port));
        if (auto r = client->connect(
                host.toStdString(),
                port,
                std::chrono::milliseconds(m_settingsDlg->connectTimeoutMs()));
            !r) {
            raiseError(std::format("Unable to connect to SpikeGLX: {}", r.error()));
            return false;
        }
        LOG_INFO(m_log, "Connected to {} on {}:{}", client->version(), host, port);

        auto initialized = client->isInitialized();
        if (!initialized) {
            raiseError(qstr(initialized.error()));
            return false;
        }
        if (!*initialized) {
            raiseError(QStringLiteral(
                "SpikeGLX has not validated its parameters yet. Open its acquisition configuration dialog once "
                "and click 'Verify | Save' (or 'Run'), then try again."));
            return false;
        }

        auto running = client->isRunning();
        if (!running) {
            raiseError(qstr(running.error()));
            return false;
        }
        if (mode == RunControlMode::Automatic) {
            mode = *running ? RunControlMode::GateOnly : RunControlMode::FullControl;
            LOG_INFO(
                m_log,
                "SpikeGLX is {}, using {} mode",
                *running ? "already running" : "idle",
                runControlModeString(mode));
        }
        if (mode == RunControlMode::FullControl && *running) {
            raiseError(QStringLiteral(
                "SpikeGLX is already running a run. Stop it, or switch this module to 'Gate only' mode."));
            return false;
        }
        if (mode != RunControlMode::FullControl && !*running) {
            raiseError(QStringLiteral(
                "SpikeGLX is not running. Start the SpikeGLX run first, or switch this module to 'Full control' "
                "mode."));
            return false;
        }

        // device detection & parameter validation
        if (!*running) {
            if (auto r = ensureDevicesSelected(*client, m_settingsDlg->deviceString()); !r) {
                raiseError(qstr(r.error()));
                return false;
            }
        }

        // stream layout
        QString err;
        if (!enumerateStreams(*client, m_streams, err)) {
            raiseError(QStringLiteral("Unable to query SpikeGLX streams: %1").arg(err));
            return false;
        }
        if (m_streams.empty()) {
            raiseError(QStringLiteral("SpikeGLX reports no enabled data streams."));
            return false;
        }

        // dataset & static attributes
        auto dataset = createDefaultDataset(name());
        if (!dataset)
            return false;
        dataset->insertAttribute("spikeglx_version", client->version());
        dataset->insertAttribute("host", host.toStdString());
        dataset->insertAttribute("port", static_cast<int64_t>(port));
        dataset->insertAttribute("run_control", runControlModeString(mode)); // effective control mode
        if (!m_settingsDlg->deviceString().isEmpty())
            dataset->insertAttribute("device_string", m_settingsDlg->deviceString().toStdString());
        if (auto addrs = client->probeAddrs())
            dataset->insertAttribute("probe_addresses", *addrs);
        dataset->insertAttribute("timestamp_method", "polled-tcp");
        dataset->insertAttribute("live_data_enabled", fetchEnabled);
        if (fetchEnabled)
            dataset->insertAttribute("live_data_overrun_policy", std::string{abortOnOverrun ? "abort" : "skip"});
        {
            MetaStringMap streamsMeta;
            for (const auto &si : m_streams) {
                MetaStringMap sm;
                sm.insert("js", static_cast<int64_t>(si.sid.js));
                sm.insert("ip", static_cast<int64_t>(si.sid.ip));
                sm.insert("sample_rate", si.sampleRate);
                sm.insert("acquired_channels", static_cast<int64_t>(si.totalChans));
                sm.insert("saved_channels", static_cast<int64_t>(si.savedChans));
                MetaArray counts;
                const auto groups = SglxUtils::chanGroupsForStream(si.sid.js);
                MetaStringMap countMap;
                for (size_t i = 0; i < groups.size() && i < si.acqCounts.size(); ++i)
                    countMap.insert(
                        SglxUtils::chanGroupName(groups[i]).toStdString(),
                        static_cast<int64_t>(si.acqCounts[i]));
                sm.insert("channel_counts", countMap);
                if (!si.serial.isEmpty()) {
                    sm.insert("serial", si.serial.toStdString());
                    sm.insert(si.sid.js == Sglx::JS_OB ? "slot" : "probe_type", static_cast<int64_t>(si.slotOrType));
                }
                streamsMeta.insert(si.name.toStdString(), sm);
            }
            dataset->insertAttribute("streams", streamsMeta);
        }
        // live-data ports
        std::vector<FetchStream> fetchStreams;
        for (const auto &fetchPort : m_fetchPorts)
            fetchStreams.emplace_back(fetchPort);
        m_portsMeta.clear();
        if (fetchEnabled) {
            for (auto &fs : fetchStreams) {
                if (!configureFetchStream(fs, *client, fetchMaxBlockMs))
                    return false;

                // our thread starts the synchronizer once it has taken the reference point of the stream
                fs.syncer = initCounterSynchronizer(fs.sampleRate);
                if (fs.syncer) {
                    fs.syncer->setStrategies(
                        TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);
                    fs.syncer->setTolerance(std::chrono::milliseconds(5));
                    fs.syncer->setCalibrationBlocksCount(std::max(20, 20000 / std::max(fetchIntervalMs, 1)));
                }
            }
            dataset->insertAttribute("live_data_ports", m_portsMeta);
        }

        // sample-count log
        std::vector<SyncStream> syncStreams;
        for (const auto &streamName : m_settingsDlg->syncStreams()) {
            const auto sid = SglxUtils::parseStreamName(streamName);
            if (!sid) {
                raiseError(QStringLiteral("Invalid stream name in clock synchronization log: '%1'").arg(streamName));
                return false;
            }
            const auto *si = findStream(*sid);
            if (!si) {
                raiseError(QStringLiteral(
                               "Stream '%1' selected for the clock synchronization log is not enabled "
                               "in SpikeGLX.")
                               .arg(streamName));
                return false;
            }
            SyncStream ss;
            ss.sid = *sid;
            ss.name = si->name;
            ss.sampleRate = si->sampleRate;
            ss.writer = std::make_unique<TimeSyncFileWriter>();
            ss.writer->setSyncMode(TSyncFileMode::SYNCPOINTS);
            ss.writer->setTimeNames("sample-count", "master-time");
            ss.writer->setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
            ss.writer->setTimeDataTypes(TSyncFileDataType::UINT64, TSyncFileDataType::UINT64);
            ss.writer->setChunkSize(120); // new chunk about every 2 min at 1 Hz

            auto fname = dataset->addAuxDataFile(
                QStringLiteral("%1-samplecount.tsync").arg(si->name).toStdString(),
                "tsync");
            if (!fname) {
                raiseError(qstr(fname.error()));
                return false;
            }
            ss.writer->setFileName(fname->string());

            MetaStringMap userData;
            userData.insert("stream", si->name.toStdString());
            userData.insert("js", static_cast<int64_t>(si->sid.js));
            userData.insert("ip", static_cast<int64_t>(si->sid.ip));
            userData.insert("sample_rate", si->sampleRate);
            if (!si->serial.isEmpty())
                userData.insert("serial", si->serial.toStdString());
            if (!ss.writer->open(name().toStdString(), dataset->collectionId(), userData)) {
                raiseError(std::format("Unable to open time-sync file: {}", ss.writer->lastError()));
                return false;
            }
            syncStreams.push_back(std::move(ss));
        }

        // run name & SpikeGLX run start
        const auto runName = makeRunName(info, dataset);
        if (mode == RunControlMode::FullControl) {
            setStatusMessage(QStringLiteral("Starting SpikeGLX run '%1'…").arg(runName));
            if (auto r = client->startRun(runName.toStdString()); !r) {
                raiseError(QStringLiteral("Unable to start SpikeGLX run '%1': %2").arg(runName, qstr(r.error())));
                return false;
            }
            sglxRunStartedByUs = true;

            if (!waitForStreams(*client, fetchEnabled, fetchStreams, syncStreams, 30000))
                return false;
        } else {
            if (auto rn = client->runName())
                dataset->insertAttribute("run_name", rn->c_str());
        }

        // store SpikeGLX's parameters, now that the run is started and all of them are available
        if (auto params = client->params()) {
            MetaStringMap pm;
            for (const auto &[k, v] : *params)
                pm.insert(k, MetaValue(v));
            dataset->insertAttribute("spikeglx_params", pm);
        } else {
            LOG_WARNING(m_log, "Unable to fetch SpikeGLX parameters: {}", params.error());
        }

        for (auto &fs : fetchStreams) {
            if (!fetchEnabled)
                continue;
            if (fs.digital)
                fs.lineStream->start();
            else
                fs.stream->start();
        }

        setStatusMessage(QStringLiteral("Ready (%1)").arg(runName));

        // hand the connection and everything else our thread needs for this run over to it
        setWorker(
            Worker{
                .client = std::move(client),
                .mode = mode,
                .syncIntervalMs = syncIntervalMs,
                .fetchEnabled = fetchEnabled,
                .fetchIntervalMs = fetchIntervalMs,
                .abortOnOverrun = abortOnOverrun,
                .fetchStreams = std::move(fetchStreams),
                .syncStreams = std::move(syncStreams),
                .dataset = dataset,
                .runName = runName,
                .subject = info.subject,
                .isEphemeralRun = info.isEphemeral,
                .experimentId = info.experimentId,
                .instanceId = GlobalConfig().instanceId(),
                .sglxRunStartedByUs = sglxRunStartedByUs,
            });

        cleanupOnFailure.dismiss();
        return true;
    }

    static std::string runControlModeString(RunControlMode mode)
    {
        switch (mode) {
        case RunControlMode::FullControl:
            return "full-control";
        case RunControlMode::GateOnly:
            return "gate-only";
        case RunControlMode::Monitor:
            return "monitor";
        case RunControlMode::Automatic:
            return "automatic";
        }
        return "unknown";
    }

    /**
     * Resolve a configured live-data entry against the real stream layout and
     * set the metadata of its output port.
     */
    bool configureFetchStream(FetchStream &fs, Sglx::Client &client, int fetchMaxBlockMs)
    {
        const auto *si = findStream(fs.sid);
        if (!si) {
            raiseError(QStringLiteral("Live-data stream '%1' is not enabled in SpikeGLX.").arg(fs.streamName));
            return false;
        }

        const auto range = SglxUtils::chanGroupRange(fs.sid.js, si->acqCounts, fs.group);
        if (!range) {
            raiseError(QStringLiteral("Live-data entry '%1 %2': %3")
                           .arg(fs.streamName, SglxUtils::chanGroupName(fs.group), range.error()));
            return false;
        }
        const auto [offset, count] = *range;
        if (count <= 0) {
            raiseError(QStringLiteral("Live-data entry '%1 %2' selects a channel group without channels.")
                           .arg(fs.streamName, SglxUtils::chanGroupName(fs.group)));
            return false;
        }

        QString chanSpec;
        for (const auto &e : m_settingsDlg->fetchEntries()) {
            const auto esid = SglxUtils::parseStreamName(e.stream);
            const auto egroup = SglxUtils::parseChanGroup(e.group);
            if (esid && egroup && *esid == fs.sid && *egroup == fs.group) {
                chanSpec = e.channels;
                break;
            }
        }
        fs.lines.clear();
        if (fs.digital) {
            // For digital groups the spec selects lines, not the 16-bit words they are
            // packed into: SpikeGLX numbers a line as word * 16 + bit, which is also the
            // number its own sync and trigger settings use. We only fetch the words that
            // actually contain a selected line.
            auto sel = SglxUtils::parseChannelSpec(
                chanSpec,
                SglxUtils::digitalLineCount(count),
                QStringLiteral("Line"));
            if (!sel) {
                raiseError(QStringLiteral("Live-data entry '%1 %2': %3")
                               .arg(fs.streamName, SglxUtils::chanGroupName(fs.group), sel.error()));
                return false;
            }
            fs.lines = std::move(*sel);
            fs.relChans = SglxUtils::digitalWordsForLines(fs.lines);
        } else {
            auto rel = SglxUtils::parseChannelSpec(chanSpec, count);
            if (!rel) {
                raiseError(QStringLiteral("Live-data entry '%1 %2': %3")
                               .arg(fs.streamName, SglxUtils::chanGroupName(fs.group), rel.error()));
                return false;
            }
            fs.relChans = std::move(*rel);
        }
        fs.absChans.clear();
        fs.absChans.reserve(fs.relChans.size());
        for (const auto c : fs.relChans)
            fs.absChans.push_back(offset + c);
        fs.sampleRate = si->sampleRate;
        fs.maxSamps = std::clamp(static_cast<int>(std::lround(fs.sampleRate * fetchMaxBlockMs / 1000.0)), 1, 999999);

        // scaling: check the first and last channel of the selection
        double scale = 1.0;
        const bool digital = fs.digital;
        if (!digital) {
            auto first = client.i16ToVolts(fs.sid, fs.absChans.front());
            auto last = client.i16ToVolts(fs.sid, fs.absChans.back());
            if (!first || !last) {
                raiseError(QStringLiteral("Unable to query channel scaling for '%1': %2")
                               .arg(fs.streamName, qstr(first ? last.error() : first.error())));
                return false;
            }
            scale = *first;
            if (std::abs(*first - *last) > 1e-12)
                LOG_WARNING(
                    m_log,
                    "Channels of live-data entry '{} {}' use different gains; the metadata scale is taken from "
                    "channel {}",
                    fs.streamName,
                    SglxUtils::chanGroupName(fs.group),
                    fs.absChans.front());
        }

        const auto groupName = SglxUtils::chanGroupName(fs.group);
        MetaArray absChans;
        for (const auto c : fs.absChans)
            absChans.push_back(static_cast<int64_t>(c));

        const auto suggestedName = QStringLiteral("%1-%2/%3")
                                       .arg(datasetNameSuggestion(), fs.portId, groupName.toLower());
        MetaStringMap portMeta;
        portMeta.insert("stream", fs.streamName.toStdString());
        portMeta.insert("group", groupName.toStdString());
        portMeta.insert("channels", absChans);
        portMeta.insert("sample_rate", fs.sampleRate);

        if (digital) {
            MetaArray lines;
            for (const auto l : fs.lines)
                lines.push_back(static_cast<int64_t>(l));

            // No signal_names / data_scale / data_offset here: this is a sparse event
            // stream, not an indexed channel set with an affine transform. The sample
            // rate is still published, as it is the resolution of the edge times.
            fs.lineStream->setMetadataValue("sample_rate", fs.sampleRate);
            fs.lineStream->setMetadataValue("time_unit", std::string{"microseconds"});
            fs.lineStream->setMetadataValue("data_unit", std::string{"ttl"});
            fs.lineStream->setMetadataValue("is_digital", true);
            fs.lineStream->setMetadataValue("spikeglx_stream", fs.streamName.toStdString());
            fs.lineStream->setMetadataValue("spikeglx_js", static_cast<int64_t>(fs.sid.js));
            fs.lineStream->setMetadataValue("spikeglx_ip", static_cast<int64_t>(fs.sid.ip));
            fs.lineStream->setMetadataValue("spikeglx_group", groupName.toStdString());
            fs.lineStream->setMetadataValue("spikeglx_channels", absChans);
            fs.lineStream->setMetadataValue("spikeglx_lines", lines);
            if (!si->serial.isEmpty())
                fs.lineStream->setMetadataValue("spikeglx_serial", si->serial.toStdString());
            fs.lineStream->setSuggestedDataName(suggestedName);

            portMeta.insert("data_unit", std::string{"ttl"});
            portMeta.insert("lines", lines);

            // Bit masks for the lines we publish, indexed like the fetched words.
            fs.lineMask.assign(fs.relChans.size(), 0);
            for (const auto l : fs.lines) {
                const auto word = l / SglxUtils::digitalLinesPerWord;
                const auto it = std::find(fs.relChans.begin(), fs.relChans.end(), word);
                fs.lineMask[std::distance(fs.relChans.begin(), it)] |= static_cast<uint16_t>(
                    1u << (l % SglxUtils::digitalLinesPerWord));
            }
            fs.linePrev.assign(fs.relChans.size(), 0);
        } else {
            MetaArray names;
            for (const auto c : fs.relChans) {
                if (fs.group == SglxUtils::ChanGroup::ALL)
                    names.push_back(QStringLiteral("CH%1").arg(c).toStdString());
                else
                    names.push_back(QStringLiteral("%1%2").arg(groupName).arg(c).toStdString());
            }

            fs.stream->setMetadataValue("sample_rate", fs.sampleRate);
            fs.stream->setMetadataValue("time_unit", std::string{"index"});
            fs.stream->setMetadataValue("data_unit", std::string{"V"});
            fs.stream->setMetadataValue("data_scale", scale);
            fs.stream->setMetadataValue("data_offset", 0.0);
            fs.stream->setMetadataValue("is_digital", false);
            fs.stream->setMetadataValue("signal_names", names);
            fs.stream->setMetadataValue("spikeglx_stream", fs.streamName.toStdString());
            fs.stream->setMetadataValue("spikeglx_js", static_cast<int64_t>(fs.sid.js));
            fs.stream->setMetadataValue("spikeglx_ip", static_cast<int64_t>(fs.sid.ip));
            fs.stream->setMetadataValue("spikeglx_group", groupName.toStdString());
            fs.stream->setMetadataValue("spikeglx_channels", absChans);
            if (!si->serial.isEmpty())
                fs.stream->setMetadataValue("spikeglx_serial", si->serial.toStdString());
            fs.stream->setSuggestedDataName(suggestedName);

            portMeta.insert("data_unit", std::string{"V"});
            portMeta.insert("data_scale", scale);

            fs.block = SignalBlockI16(1, static_cast<uint>(fs.absChans.size()));
        }
        m_portsMeta.insert(fs.portId.toStdString(), portMeta);

        return true;
    }

    /**
     * After STARTRUN, wait until SpikeGLX reports the run as active and all
     * streams we touch deliver samples.
     */
    bool waitForStreams(
        Sglx::Client &client,
        bool fetchEnabled,
        const std::vector<FetchStream> &fetchStreams,
        const std::vector<SyncStream> &syncStreams,
        int timeoutMs)
    {
        std::vector<Sglx::StreamId> touched;
        for (const auto &fs : fetchStreams) {
            if (fetchEnabled)
                touched.push_back(fs.sid);
        }
        for (const auto &ss : syncStreams)
            touched.push_back(ss.sid);
        if (touched.empty())
            touched.push_back(m_streams.front().sid);

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            appProcessEvents();
            auto running = client.isRunning();
            if (!running) {
                raiseError(qstr(running.error()));
                return false;
            }
            if (*running) {
                bool allUp = true;
                for (const auto &sid : touched) {
                    auto cnt = client.sampleCount(sid);
                    if (!cnt) {
                        allUp = false;
                        break;
                    }
                }
                if (allUp)
                    return true;
            }
            QThread::msleep(100);
        }

        raiseError(QStringLiteral("SpikeGLX did not start acquiring data within %1 s.").arg(timeoutMs / 1000));
        return false;
    }

    /**
     * Controls the SpikeGLX run, logs its sample counters against the master clock and
     * fetches live data, in the module's thread.
     */
    struct Worker {
        WorkerContext mod{};
        using RunControlMode = SpikeGLXSettingsDialog::RunControlMode;

        // The connection to SpikeGLX of this run. It is established in prepare() and then handed over
        // to us, so this worker is the only user of the client for as long as it exists.
        std::unique_ptr<Sglx::Client> client;

        // settings snapshot for the current run
        RunControlMode mode; /// effective mode of the current run
        int syncIntervalMs;
        bool fetchEnabled;
        int fetchIntervalMs;
        bool abortOnOverrun;

        // run state
        std::vector<FetchStream> fetchStreams;
        std::vector<SyncStream> syncStreams;
        std::shared_ptr<EDLDataset> dataset;
        QString runName;
        TestSubject subject;
        bool isEphemeralRun;
        QString experimentId;
        QString instanceId; /// ID of this Syntalos instance, sent to SpikeGLX so it knows who controlled it
        bool sglxRunStartedByUs;

        /// set once our thread was launched; the module has to stop the SpikeGLX run if that never happened
        bool threadStarted = false;

        /**
         * Turn the digital words of one fetch into LineReading edge events.
         *
         * SpikeGLX packs digital lines into 16-bit words, lowest numbered line in the
         * lowest order bit, so a line is `word * 16 + bit` - the very numbering its own
         * sync and trigger settings use. An event is emitted whenever a selected line
         * changes level, plus once per line on the first block of the run so the starting
         * level is recorded. The same happens after a gap in the fetched data: edges inside
         * the gap are lost, so the level of every line is emitted again at the first sample
         * after it, which marks the discontinuity for consumers.
         */
        void emitLineEdges(FetchStream &fs, int n, int nCh)
        {
            for (int s = 0; s < n; ++s) {
                const auto tsUs = microseconds_t(
                    std::llround(static_cast<double>(fs.block.timestamps(s)) * 1e6 / fs.sampleRate));
                const int16_t *row = fs.buffer.data() + static_cast<size_t>(s) * nCh;

                for (int c = 0; c < nCh; ++c) {
                    // SpikeGLX carries the unsigned status/digital word in a signed slot
                    const auto cur = static_cast<uint16_t>(row[c]);
                    uint16_t changed = fs.linePrimed ? ((cur ^ fs.linePrev[c]) & fs.lineMask[c]) : fs.lineMask[c];
                    fs.linePrev[c] = cur;

                    const auto base = static_cast<uint16_t>(fs.relChans[c] * SglxUtils::digitalLinesPerWord);
                    while (changed != 0) {
                        const auto bit = std::countr_zero(changed);
                        changed &= static_cast<uint16_t>(changed - 1);

                        LineReading r;
                        r.lineId = base + bit;
                        r.value = (cur >> bit) & 1;
                        r.time = tsUs;
                        fs.lineStream->push(r);
                        fs.emittedEvents++;
                    }
                }
                fs.linePrimed = true;
            }
        }

        /**
         * Fetch everything new on one stream and publish it, as signal blocks for the
         * analog groups and as line events for the digital ones.
         * Returns false on a fatal error (already reported).
         */
        bool pumpFetchStream(FetchStream &fs)
        {
            for (int iter = 0; iter < 64 && mod.running(); ++iter) {
                auto res = client->fetch(fs.sid, fs.cursor, fs.maxSamps, fs.absChans, fs.buffer);
                const auto recvTs = mod.timer->timeSinceStartUsec();
                if (!res) {
                    if (res.error().find("Too late") != std::string::npos) {
                        if (abortOnOverrun) {
                            mod.raiseError(
                                QStringLiteral(
                                    "Syntalos fell behind SpikeGLX: live data of '%1' was overwritten in the "
                                    "SpikeGLX buffer before it could be fetched. The run was aborted because "
                                    "complete live data was requested; the SpikeGLX files on the remote "
                                    "computer are not affected.")
                                    .arg(fs.streamName));
                            return false;
                        }
                        // we fell behind the server's ring buffer, resynchronize
                        auto cnt = client->sampleCount(fs.sid);
                        if (!cnt) {
                            mod.raiseError(QStringLiteral("Unable to resynchronize with SpikeGLX stream '%1': %2")
                                               .arg(fs.streamName, qstr(cnt.error())));
                            return false;
                        }
                        const auto newCursor = std::max<uint64_t>(*cnt, 1);
                        fs.droppedSamples += newCursor > fs.cursor ? newCursor - fs.cursor : 0;
                        fs.gapCount++;
                        fs.linePrimed = false;
                        LOG_WARNING(
                            mod.log,
                            "Live data of '{}' fell behind the SpikeGLX buffer, skipping {} samples",
                            fs.streamName,
                            newCursor - fs.cursor);
                        fs.cursor = newCursor;
                        return true;
                    }
                    if (auto sglxRunning = client->isRunning(); sglxRunning && !*sglxRunning)
                        mod.raiseError(QStringLiteral("SpikeGLX stopped running unexpectedly."));
                    else
                        mod.raiseError(QStringLiteral("Fetching live data from '%1' failed: %2")
                                           .arg(fs.streamName, qstr(res.error())));
                    return false;
                }

                if (res->nSamps <= 0)
                    return true; // no new data yet

                if (res->headCt != fs.cursor) {
                    // should not happen without a "Too late" error, but keep the counters honest
                    if (abortOnOverrun && res->headCt > fs.cursor) {
                        mod.raiseError(QStringLiteral(
                                           "Live data of '%1' has a gap of %2 samples; the run was aborted "
                                           "because complete live data was requested.")
                                           .arg(fs.streamName)
                                           .arg(res->headCt - fs.cursor));
                        return false;
                    }
                    fs.gapCount++;
                    fs.linePrimed = false;
                    if (res->headCt > fs.cursor)
                        fs.droppedSamples += res->headCt - fs.cursor;
                }

                const int n = res->nSamps;
                const int nCh = res->nChans;
                fs.block.timestamps.resize(n);
                if (!fs.digital) {
                    fs.block.data.resize(n, nCh);
                    // the SDK buffer is sample-major int16, exactly our row-major block layout
                    fs.block.data = Eigen::Map<const MatrixXi16>(fs.buffer.data(), n, nCh);
                }
                for (int s = 0; s < n; ++s) {
                    const int64_t idx = static_cast<int64_t>(res->headCt + s) - static_cast<int64_t>(fs.refSampleCount)
                                        + fs.startSampleOffset;
                    fs.block.timestamps(s) = static_cast<uint64_t>(std::max<int64_t>(idx, 0));
                }
                if (fs.syncer)
                    fs.syncer->processTimestamps(recvTs, 0, 1, fs.block.timestamps);

                if (fs.digital)
                    emitLineEdges(fs, n, nCh);
                else
                    fs.stream->push(fs.block);
                fs.cursor = res->headCt + n;
                fs.fetchedSamples += n;

                if (n < fs.maxSamps)
                    return true; // drained
            }
            return true;
        }

        void run()
        {
            threadStarted = true;

            bool failed = false;
            bool gateOpened = false;

            // Announce ourselves to SpikeGLX before the start barrier: SpikeGLX attaches pending
            // metadata when its next file-set is opened, so this has to happen before SETRECORDENAB 1,
            // and doing it ahead of the barrier keeps the round-trip out of the time-critical path
            // between the start signal and the gate.
            bool metadataSet = pushRunMetadata();

            mod.waitForStart();

            // The engine also wakes us up if the run was aborted before it ever began, so the
            // Syntalos clock may never have been started. In that case we must not open the
            // recording gate - but we still have to fall through to the epilogue below, which
            // stops the SpikeGLX run that we may have started in prepare().
            if (!mod.running())
                failed = true;

            const auto startTime = mod.timer->startTime();
            const auto startWallUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                         mod.timer->startWallTime().time_since_epoch())
                                         .count();
            dataset->insertAttribute("run_start_wall_time_us", startWallUs);

            // Open the recording gate right away (identity and prep data has been sent in prepare())
            if (!failed && mode != RunControlMode::Monitor) {
                Sglx::Client::Result<void> r;
                const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = client->setRecordingEnable(true));
                if (!r) {
                    mod.raiseError(std::format("Unable to enable recording in SpikeGLX: {}", r.error()));
                    failed = true;
                } else {
                    gateOpened = true;
                    dataset->insertAttribute("record_start_master_time_us", static_cast<int64_t>(ts.count()));
                    LOG_INFO(mod.log, "SpikeGLX recording enabled at {} µs", ts.count());
                }
            }

            // reference points: sample count <-> master time
            if (!failed) {
                MetaStringMap refs;
                auto takeReference = [&](Sglx::StreamId sid,
                                         const QString &sname,
                                         uint64_t &count,
                                         microseconds_t &time) {
                    Sglx::Client::Result<uint64_t> cnt;
                    time = FUNC_EXEC_TIMESTAMP(startTime, cnt = client->sampleCount(sid));
                    if (!cnt) {
                        mod.raiseError(
                            QStringLiteral("Unable to read sample count of '%1': %2").arg(sname, qstr(cnt.error())));
                        return false;
                    }
                    count = *cnt;
                    MetaStringMap rm;
                    rm.insert("sample_count", static_cast<int64_t>(count));
                    rm.insert("master_time_us", static_cast<int64_t>(time.count()));
                    refs.insert(sname.toStdString(), rm);
                    return true;
                };

                for (auto &fs : fetchStreams) {
                    if (!fetchEnabled)
                        break;
                    microseconds_t t;
                    if (!takeReference(fs.sid, fs.streamName, fs.refSampleCount, t)) {
                        failed = true;
                        break;
                    }
                    fs.cursor = std::max<uint64_t>(fs.refSampleCount, 1);
                    fs.startSampleOffset = std::llround(static_cast<double>(t.count()) * fs.sampleRate / 1e6);
                    // the edge detector primes on the first block, so the starting
                    // level of every selected line is emitted once
                    fs.linePrimed = false;
                    // the synchronizer was created and configured in prepare()
                    if (fs.syncer) {
                        if (!fs.syncer->start()) {
                            mod.raiseError(
                                QStringLiteral("Unable to start time synchronizer for '%1'.").arg(fs.streamName));
                            failed = true;
                            break;
                        }
                    }
                }
                for (auto &ss : syncStreams) {
                    if (failed)
                        break;
                    uint64_t count;
                    microseconds_t t;
                    if (!takeReference(ss.sid, ss.name, count, t)) {
                        failed = true;
                        break;
                    }
                    ss.writer->writeTimes(count, static_cast<uint64_t>(t.count()));
                }
                dataset->insertAttribute("reference_points", refs);
            }

            // main loop
            auto lastSync = mod.timer->timeSinceStartUsec();
            auto lastHealth = lastSync;
            const auto syncInterval = std::chrono::milliseconds(syncIntervalMs);
            const auto healthInterval = std::chrono::milliseconds(2000);
            const int sleepMs = fetchEnabled ? fetchIntervalMs : std::min(syncIntervalMs, 100);

            while (mod.running() && !failed) {
                if (fetchEnabled) {
                    for (auto &fs : fetchStreams) {
                        if (!pumpFetchStream(fs)) {
                            failed = true;
                            break;
                        }
                    }
                    if (failed)
                        break;
                }

                const auto now = mod.timer->timeSinceStartUsec();
                if (!syncStreams.empty() && now - lastSync >= syncInterval) {
                    lastSync = now;
                    for (auto &ss : syncStreams) {
                        Sglx::Client::Result<uint64_t> cnt;
                        const auto t = FUNC_EXEC_TIMESTAMP(startTime, cnt = client->sampleCount(ss.sid));
                        if (!cnt) {
                            mod.raiseError(QStringLiteral("SpikeGLX stream '%1' stopped delivering samples: %2")
                                               .arg(ss.name, qstr(cnt.error())));
                            failed = true;
                            break;
                        }
                        ss.writer->writeTimes(*cnt, static_cast<uint64_t>(t.count()));
                    }
                    if (failed)
                        break;
                }

                if (now - lastHealth >= healthInterval) {
                    lastHealth = now;
                    auto sglxRunning = client->isRunning();
                    if (!sglxRunning || !*sglxRunning) {
                        mod.raiseError(
                            sglxRunning
                                ? QStringLiteral("SpikeGLX stopped running unexpectedly.")
                                : QStringLiteral("Lost connection to SpikeGLX: %1").arg(qstr(sglxRunning.error())));
                        failed = true;
                        break;
                    }
                    if (mode != RunControlMode::Monitor) {
                        auto saving = client->isSaving();
                        if (saving && !*saving) {
                            mod.raiseError(QStringLiteral(
                                "SpikeGLX stopped writing data while the run was active (was recording disabled "
                                "manually?)."));
                            failed = true;
                            break;
                        }
                    }
                }

                // sleep in small slices so we react to stop requests quickly
                for (int slept = 0; slept < sleepMs && mod.running(); slept += 20)
                    std::this_thread::sleep_for(milliseconds_t(std::min(20, sleepMs - slept)));
            }

            // epilogue: this thread is the only user of the client during a run,
            // so all stop-time commands are issued here.
            if (gateOpened) {
                Sglx::Client::Result<void> r;
                const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = client->setRecordingEnable(false));
                if (r)
                    dataset->insertAttribute("record_stop_master_time_us", static_cast<int64_t>(ts.count()));
                else
                    LOG_WARNING(mod.log, "Unable to disable SpikeGLX recording: {}", r.error());
            }

            // Our keys have served their purpose now, whether a file-set carried them or not.
            // This has to happen before a possible STOPRUN, as SETMETADATA needs a run in progress.
            if (metadataSet)
                clearRunMetadata();

            collectRunInfo();

            if (mode == RunControlMode::FullControl) {
                Sglx::Client::Result<void> r;
                const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = client->stopRun());
                if (r) {
                    dataset->insertAttribute("run_stop_master_time_us", static_cast<int64_t>(ts.count()));
                    sglxRunStartedByUs = false;
                    setPlaceholderRunName(*client, mod.log);
                } else {
                    LOG_WARNING(mod.log, "Unable to stop SpikeGLX run: {}", r.error());
                }
            }

            MetaStringMap fetchStats;
            for (auto &fs : fetchStreams) {
                if (fs.syncer) {
                    safeStopSynchronizer(fs.syncer);
                    fs.syncer.reset();
                }
                if (!fetchEnabled)
                    continue;
                MetaStringMap sm;
                sm.insert("fetched_samples", static_cast<int64_t>(fs.fetchedSamples));
                sm.insert("gap_count", static_cast<int64_t>(fs.gapCount));
                sm.insert("dropped_samples", static_cast<int64_t>(fs.droppedSamples));
                if (fs.digital)
                    sm.insert("emitted_events", static_cast<int64_t>(fs.emittedEvents));
                fetchStats.insert(fs.portId.toStdString(), sm);
                if (fs.digital && fs.emittedEvents > 100000)
                    LOG_WARNING(
                        mod.log,
                        "Live data port '{}' emitted {} line events; check that the selected lines are actually "
                        "connected, a floating input generates events at the stream sample rate",
                        fs.portId,
                        fs.emittedEvents);
                if (fs.gapCount > 0)
                    LOG_WARNING(
                        mod.log,
                        "Live data port '{}' had {} gaps ({} samples lost); the SpikeGLX files on disk are unaffected",
                        fs.portId,
                        fs.gapCount,
                        fs.droppedSamples);
            }
            if (fetchEnabled)
                dataset->insertAttribute("live_data", fetchStats);
        }

        /**
         * @brief The Syntalos identity keys for the current run.
         * @param blank set every key to an empty value instead of its real one
         *
         * SETMETADATA does not queue a key set for the next file-set, it *merges* what we send into
         * a map that SpikeGLX keeps for the whole duration of its run and writes into the `.meta`
         * files whenever a file-set is closed (see TrigBase::setMetaData()). Keys can never be
         * removed again, and the map is not cleared between file-sets - so blanking the values is
         * the only way to stop our identity from labelling a later, unrelated recording.
         */
        std::map<std::string, std::string> runMetadata(bool blank = false) const
        {
            const auto value = [blank](const QString &s) {
                return blank ? std::string() : s.toStdString();
            };

            std::map<std::string, std::string> kv;
            kv["sy_collection_id"] = blank ? std::string() : dataset->collectionId().toHex();
            kv["sy_subject_id"] = value(subject.id);
            kv["sy_subject_group"] = value(subject.group);
            kv["sy_experiment_id"] = value(experimentId);
            kv["sy_run_name"] = value(runName);
            kv["sy_module_name"] = value(mod.moduleName());
            kv["sy_instance_id"] = value(instanceId);
            if (isEphemeralRun)
                kv["sy_ephemeral_run"] = blank ? std::string() : "true";
            return kv;
        }

        /**
         * @brief Identify this Syntalos run to SpikeGLX.
         *
         * The keys end up in the `.meta` files of the file-set that the recording gate creates.
         *
         * @return true if SpikeGLX now holds our identity, but no file-set of ours exists to carry it.
         */
        bool pushRunMetadata()
        {
            if (mode == RunControlMode::Monitor)
                return false;

            if (auto r = client->setMetadata(runMetadata()); !r) {
                LOG_WARNING(mod.log, "Unable to set SpikeGLX metadata: {}", r.error());
                return false;
            }
            return true;
        }

        /**
         * @brief Blank our identity again, so it can not label a later, unrelated file-set.
         *
         * SpikeGLX keeps our keys for the rest of its run and writes them into *every* file-set it
         * closes, so leaving them behind would label whatever is recorded next - after an aborted
         * run that never opened the gate just as much as after a successful one. Keys can not be
         * deleted, so we overwrite them with empty values; an explicit marker key would be worse,
         * as it could not be removed either and would then haunt the rest of the SpikeGLX run.
         *
         * This is the metadata half of leaving SpikeGLX in a neutral state, the run-name half being
         * setPlaceholderRunName(). The two are mutually exclusive by design: SETMETADATA needs a run
         * in progress, SETRUNNAME needs the opposite.
         */
        void clearRunMetadata()
        {
            // SpikeGLX takes its copy of the metadata when it closes a file-set, and the close is
            // asynchronous: SETRECORDENAB 0 returns before it has happened. Blanking too early would
            // strip the identity from our own .meta files, so wait for the files to be closed - at
            // which point the copy is guaranteed to have been taken.
            bool filesClosed = false;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 2000) {
                const auto saving = client->isSaving();
                if (!saving) {
                    LOG_WARNING(mod.log, "Unable to query the SpikeGLX saving state: {}", saving.error());
                    break;
                }
                if (!*saving) {
                    filesClosed = true;
                    break;
                }
                std::this_thread::sleep_for(milliseconds_t(20));
            }
            if (!filesClosed) {
                LOG_WARNING(mod.log, "SpikeGLX is still writing files, leaving our run metadata in place");
                return;
            }

            if (auto r = client->setMetadata(runMetadata(true)); !r)
                LOG_WARNING(mod.log, "Unable to blank SpikeGLX metadata: {}", r.error());
        }

        /** Record where SpikeGLX wrote its files. */
        void collectRunInfo()
        {
            auto dir = client->dataDir();
            auto rn = client->runName();
            auto gt = client->lastGT();
            if (dir)
                dataset->insertAttribute("remote_data_dir", *dir);
            if (rn)
                dataset->insertAttribute("remote_run_name", *rn);
            if (gt) {
                dataset->insertAttribute("last_gate_index", static_cast<int64_t>(gt->first));
                dataset->insertAttribute("last_trigger_index", static_cast<int64_t>(gt->second));
            }
            if (dir && rn && gt && gt->first >= 0) {
                const auto runDir = QStringLiteral("%1/%2_g%3").arg(qstr(*dir), qstr(*rn)).arg(gt->first);
                dataset->insertAttribute("remote_run_dir", runDir.toStdString());
                dataset->insertAttribute(
                    "remote_file_prefix",
                    QStringLiteral("%1_g%2_t%3").arg(qstr(*rn)).arg(gt->first).arg(gt->second).toStdString());

                // list the files SpikeGLX wrote for this run (best effort)
                if (auto files = client->enumDataDir()) {
                    const auto needle = QStringLiteral("/%1_g%2/").arg(qstr(*rn)).arg(gt->first);
                    MetaArray remoteFiles;
                    for (const auto &f : *files) {
                        const auto qf = qstr(f);
                        if (qf.contains(needle)
                            && (qf.endsWith(QLatin1String(".bin")) || qf.endsWith(QLatin1String(".meta"))))
                            remoteFiles.push_back(f);
                    }
                    if (!remoteFiles.empty())
                        dataset->insertAttribute("remote_files", remoteFiles);
                }
            }
        }
    };

    void stop() override
    {
        // Our thread has finished its epilogue at this point, so we get the connection to SpikeGLX
        // and everything else that it has used in this run back.
        auto worker = takeWorker<Worker>();

        if (worker && !worker->threadStarted) {
            // Our thread was never launched, because the run was aborted while modules were
            // still preparing (the engine only starts the module threads once every module
            // has prepared successfully, but calls stop() on all of them regardless).
            // Nothing else talks to SpikeGLX in this case, so we issue the commands that the
            // thread epilogue would have issued right here.
            LOG_INFO(m_log, "Run was aborted before it started, stopping the SpikeGLX run again");
            if (worker->sglxRunStartedByUs) {
                if (auto r = worker->client->stopRun(); !r)
                    LOG_WARNING(m_log, "Unable to stop SpikeGLX run: {}", r.error());
                else
                    setPlaceholderRunName(*worker->client, m_log);
                worker->sglxRunStartedByUs = false;
            }
        }

        if (worker) {
            for (auto &ss : worker->syncStreams) {
                if (ss.writer)
                    ss.writer->close();
            }
        }

        // this closes the connection of the run as well
        worker.reset();

        m_runActive = false;
        m_settingsDlg->setRunActive(false);
        setStatusMessage(QString());
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("host"), m_settingsDlg->host());
        settings.insert(QStringLiteral("port"), m_settingsDlg->port());
        settings.insert(QStringLiteral("connect_timeout_ms"), m_settingsDlg->connectTimeoutMs());
        settings.insert(QStringLiteral("run_control"), qstr(runControlModeString(m_settingsDlg->runControlMode())));
        settings.insert(QStringLiteral("device_string"), m_settingsDlg->deviceString());
        settings.insert(QStringLiteral("run_name_extra"), m_settingsDlg->runNameExtra());
        settings.insert(QStringLiteral("sync_interval_ms"), m_settingsDlg->syncIntervalMs());
        settings.insert(QStringLiteral("sync_streams"), m_settingsDlg->syncStreams());
        settings.insert(QStringLiteral("fetch_enabled"), m_settingsDlg->fetchEnabled());
        settings.insert(QStringLiteral("fetch_interval_ms"), m_settingsDlg->fetchIntervalMs());
        settings.insert(QStringLiteral("fetch_max_block_ms"), m_settingsDlg->fetchMaxBlockMs());
        settings.insert(
            QStringLiteral("fetch_overrun_policy"),
            m_settingsDlg->overrunPolicy() == SpikeGLXSettingsDialog::AbortRun ? QStringLiteral("abort")
                                                                               : QStringLiteral("skip"));

        QVariantList entries;
        for (const auto &e : m_settingsDlg->fetchEntries()) {
            QVariantHash eh;
            eh.insert(QStringLiteral("stream"), e.stream);
            eh.insert(QStringLiteral("group"), e.group);
            eh.insert(QStringLiteral("channels"), e.channels);
            entries << eh;
        }
        settings.insert(QStringLiteral("fetch_entries"), entries);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setHost(settings.value(QStringLiteral("host"), QStringLiteral("localhost")).toString());
        m_settingsDlg->setPort(settings.value(QStringLiteral("port"), 4142).toInt());
        m_settingsDlg->setConnectTimeoutMs(settings.value(QStringLiteral("connect_timeout_ms"), 3000).toInt());

        const auto modeStr = settings.value(QStringLiteral("run_control"), QStringLiteral("automatic")).toString();
        auto mode = RunControlMode::Automatic;
        if (modeStr == QLatin1String("full-control"))
            mode = RunControlMode::FullControl;
        else if (modeStr == QLatin1String("gate-only"))
            mode = RunControlMode::GateOnly;
        else if (modeStr == QLatin1String("monitor"))
            mode = RunControlMode::Monitor;
        m_settingsDlg->setRunControlMode(mode);

        m_settingsDlg->setDeviceString(settings.value(QStringLiteral("device_string")).toString());
        m_settingsDlg->setRunNameExtra(settings.value(QStringLiteral("run_name_extra")).toString());
        m_settingsDlg->setSyncIntervalMs(settings.value(QStringLiteral("sync_interval_ms"), 1000).toInt());
        m_settingsDlg->setSyncStreams(
            settings.value(QStringLiteral("sync_streams"), QStringList{QStringLiteral("imec0")}).toStringList());
        m_settingsDlg->setFetchEnabled(settings.value(QStringLiteral("fetch_enabled"), false).toBool());
        m_settingsDlg->setFetchIntervalMs(settings.value(QStringLiteral("fetch_interval_ms"), 50).toInt());
        m_settingsDlg->setFetchMaxBlockMs(settings.value(QStringLiteral("fetch_max_block_ms"), 250).toInt());
        m_settingsDlg->setOverrunPolicy(
            settings.value(QStringLiteral("fetch_overrun_policy"), QStringLiteral("abort")).toString()
                    == QLatin1String("skip")
                ? SpikeGLXSettingsDialog::SkipAhead
                : SpikeGLXSettingsDialog::AbortRun);

        QList<SpikeGLXSettingsDialog::FetchEntry> entries;
        const auto entryList = settings.value(QStringLiteral("fetch_entries")).toList();
        for (const auto &v : entryList) {
            const auto eh = v.toHash();
            SpikeGLXSettingsDialog::FetchEntry e;
            e.stream = eh.value(QStringLiteral("stream")).toString();
            e.group = eh.value(QStringLiteral("group")).toString();
            e.channels = eh.value(QStringLiteral("channels")).toString();
            if (!e.stream.isEmpty())
                entries << e;
        }
        m_settingsDlg->setFetchEntries(entries);
        rebuildOutputPorts();

        return true;
    }
};

QString SpikeGLXModuleInfo::id() const
{
    return QStringLiteral("spikeglx");
}

QString SpikeGLXModuleInfo::name() const
{
    return QStringLiteral("SpikeGLX Remote");
}

QString SpikeGLXModuleInfo::description() const
{
    return QStringLiteral(
        "Control a SpikeGLX (Neuropixels) recording on another computer via its remote command server, "
        "record how its sample counters relate to the Syntalos master clock, and optionally stream live data "
        "into Syntalos.");
}

ModuleCategories SpikeGLXModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *SpikeGLXModuleInfo::createModule(QObject *parent)
{
    return new SpikeGLXModule(this, parent);
}

#include "spikeglxmodule.moc"
