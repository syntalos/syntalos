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
#include <atomic>
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
struct FetchStream {
    QString portId;
    Sglx::StreamId sid;
    QString streamName;
    SglxUtils::ChanGroup group = SglxUtils::ChanGroup::ALL;
    std::vector<int> relChans; /// channel indices relative to the group
    std::vector<int> absChans; /// channel indices within the stream
    std::shared_ptr<DataStream<SignalBlockI16>> stream;

    // run state
    double sampleRate = 0;
    uint64_t cursor = 0;
    uint64_t refSampleCount = 0;
    int64_t startSampleOffset = 0;
    int maxSamps = 1;
    std::unique_ptr<FreqCounterSynchronizer> syncer;
    std::vector<int16_t> buffer;
    SignalBlockI16 block;
    uint64_t gapCount = 0;
    uint64_t droppedSamples = 0;
    uint64_t fetchedSamples = 0;
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

class SpikeGLXModule : public AbstractModule
{
    Q_OBJECT
private:
    using RunControlMode = SpikeGLXSettingsDialog::RunControlMode;

    SpikeGLXSettingsDialog *m_settingsDlg;
    Sglx::Client m_client;
    std::atomic_bool m_dialogBusy{false};
    std::atomic_bool m_threadDone{true};
    bool m_runActive = false;

    // settings snapshot for the current run
    RunControlMode m_configuredMode = RunControlMode::Automatic;
    RunControlMode m_mode = RunControlMode::FullControl; /// effective mode of the current run
    bool m_pushMetadata = true;
    int m_syncIntervalMs = 1000;
    bool m_fetchEnabled = false;
    int m_fetchIntervalMs = 50;
    int m_fetchMaxBlockMs = 250;
    bool m_abortOnOverrun = true;

    // run state
    std::vector<StreamInfo> m_streams;
    std::vector<FetchStream> m_fetchStreams;
    std::vector<SyncStream> m_syncStreams;
    std::shared_ptr<EDLDataset> m_dataset;
    MetaStringMap m_portsMeta;
    QString m_runName;
    TestSubject m_subject;
    bool m_isEphemeralRun = false;
    QString m_experimentId;
    QString m_instanceId; /// ID of this Syntalos instance, sent to SpikeGLX so it knows who controlled it
    bool m_sglxRunStartedByUs = false;

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
        for (const auto &fs : m_fetchStreams)
            previousIds.insert(fs.portId);

        std::vector<FetchStream> newStreams;
        QSet<QString> currentIds;
        const auto entries = m_settingsDlg->fetchEntries();
        for (const auto &entry : entries) {
            const auto sid = SglxUtils::parseStreamName(entry.stream);
            const auto group = SglxUtils::parseChanGroup(entry.group);
            if (!sid || !group)
                continue;

            FetchStream fs;
            fs.sid = *sid;
            fs.streamName = SglxUtils::streamName(*sid);
            fs.group = *group;
            fs.portId = QStringLiteral("%1-%2").arg(fs.streamName, qstr(groupPortSuffix(*group)));
            for (int n = 2; currentIds.contains(fs.portId); ++n)
                fs.portId = QStringLiteral("%1-%2-%3").arg(fs.streamName, qstr(groupPortSuffix(*group))).arg(n);
            currentIds.insert(fs.portId);

            const auto title = QStringLiteral("%1 %2").arg(fs.streamName, SglxUtils::chanGroupName(*group));
            fs.stream = registerOutputPort<SignalBlockI16>(fs.portId, title);
            newStreams.push_back(std::move(fs));
        }

        for (const auto &oldId : previousIds) {
            if (!currentIds.contains(oldId))
                removeOutPortById(oldId);
        }
        m_fetchStreams = std::move(newStreams);
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
                        if (auto r = ensureDevicesSelected(devString); !r) {
                            streamsText = qstr(r.error());
                            break;
                        }
                    }
                    if (!enumerateStreams(streams, err)) {
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
    Sglx::Client::Result<void> ensureDevicesSelected(const QString &devString)
    {
        // probe whether parameters were validated at all
        auto np = m_client.streamCount(Sglx::JS_IM);
        if (np || np.error().find("never validated") == std::string::npos)
            return {};

        if (devString.isEmpty())
            return std::unexpected(
                std::string(
                    "SpikeGLX has not validated its run parameters. Either click 'Detect' and 'Verify | Save' in its "
                    "acquisition configuration, or set the devices to select in this module's settings so this can "
                    "happen automatically."));

        LOG_INFO(m_log, "SpikeGLX parameters are not validated, selecting devices: {}", devString);
        if (auto r = m_client.selectDevices(devString.toStdString(), 1); !r) {
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
    bool enumerateStreams(std::vector<StreamInfo> &streams, QString &error)
    {
        streams.clear();
        for (const int js : {Sglx::JS_IM, Sglx::JS_OB, Sglx::JS_NI}) {
            auto np = m_client.streamCount(js);
            if (!np) {
                error = qstr(np.error());
                return false;
            }
            for (int ip = 0; ip < *np; ++ip) {
                StreamInfo si;
                si.sid = Sglx::StreamId{js, ip};
                si.name = SglxUtils::streamName(si.sid);

                auto rate = m_client.sampleRate(si.sid);
                if (!rate) {
                    error = QStringLiteral("%1: %2").arg(si.name, qstr(rate.error()));
                    return false;
                }
                si.sampleRate = *rate;

                auto counts = m_client.acqChanCounts(si.sid);
                if (!counts) {
                    error = QStringLiteral("%1: %2").arg(si.name, qstr(counts.error()));
                    return false;
                }
                si.acqCounts = *counts;
                for (const auto c : si.acqCounts)
                    si.totalChans += c;

                if (auto saved = m_client.saveChans(si.sid))
                    si.savedChans = static_cast<int>(saved->size());

                if (js != Sglx::JS_NI) {
                    if (auto sn = m_client.streamSN(si.sid)) {
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
    QString makeRunName(const RunInfo &info) const
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

        if (m_dataset) {
            const auto tag = qstr(m_dataset->collectionShortTag());
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
        m_subject = info.subject;
        m_isEphemeralRun = info.isEphemeral;
        m_experimentId = info.experimentId;
        m_instanceId = GlobalConfig().instanceId();
        m_streams.clear();
        m_syncStreams.clear();
        m_dataset.reset();
        m_sglxRunStartedByUs = false;
        m_threadDone = true;

        if (m_dialogBusy) {
            raiseError(QStringLiteral("A connection test is still in progress, please wait for it to finish."));
            return false;
        }

        m_runActive = true;
        m_settingsDlg->setRunActive(true);
        auto cleanupOnFailure = qScopeGuard([this] {
            if (m_sglxRunStartedByUs) {
                LOG_INFO(m_log, "Stopping the SpikeGLX run again after failed preparation");
                if (auto r = m_client.stopRun(); !r)
                    LOG_WARNING(m_log, "Unable to stop SpikeGLX run: {}", r.error());
                m_sglxRunStartedByUs = false;
            }
            m_runActive = false;
            m_settingsDlg->setRunActive(false);
        });

        // settings snapshot
        m_configuredMode = m_settingsDlg->runControlMode();
        m_mode = m_configuredMode;
        m_pushMetadata = m_settingsDlg->pushMetadata();
        m_syncIntervalMs = m_settingsDlg->syncIntervalMs();
        m_fetchEnabled = m_settingsDlg->fetchEnabled();
        m_fetchIntervalMs = m_settingsDlg->fetchIntervalMs();
        m_fetchMaxBlockMs = m_settingsDlg->fetchMaxBlockMs();
        m_abortOnOverrun = m_settingsDlg->overrunPolicy() == SpikeGLXSettingsDialog::AbortRun;
        const auto host = m_settingsDlg->host();
        const auto port = m_settingsDlg->port();

        if (host.isEmpty()) {
            raiseError(QStringLiteral("No SpikeGLX host is set."));
            return false;
        }

        // connect
        setStatusMessage(QStringLiteral("Connecting to %1:%2…").arg(host).arg(port));
        if (auto r = m_client.connect(
                host.toStdString(),
                port,
                std::chrono::milliseconds(m_settingsDlg->connectTimeoutMs()));
            !r) {
            raiseError(QStringLiteral("Unable to connect to SpikeGLX: %1").arg(qstr(r.error())));
            return false;
        }
        LOG_INFO(m_log, "Connected to {} on {}:{}", m_client.version(), host, port);

        auto initialized = m_client.isInitialized();
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

        auto running = m_client.isRunning();
        if (!running) {
            raiseError(qstr(running.error()));
            return false;
        }
        if (m_mode == RunControlMode::Automatic) {
            m_mode = *running ? RunControlMode::GateOnly : RunControlMode::FullControl;
            LOG_INFO(
                m_log,
                "SpikeGLX is {}, using {} mode",
                *running ? "already running" : "idle",
                runControlModeString(m_mode));
        }
        if (m_mode == RunControlMode::FullControl && *running) {
            raiseError(QStringLiteral(
                "SpikeGLX is already running a run. Stop it, or switch this module to 'Gate only' mode."));
            return false;
        }
        if (m_mode != RunControlMode::FullControl && !*running) {
            raiseError(QStringLiteral(
                "SpikeGLX is not running. Start the SpikeGLX run first, or switch this module to 'Full control' "
                "mode."));
            return false;
        }

        // device detection & parameter validation
        if (!*running) {
            if (auto r = ensureDevicesSelected(m_settingsDlg->deviceString()); !r) {
                raiseError(qstr(r.error()));
                return false;
            }
        }

        // stream layout
        QString err;
        if (!enumerateStreams(m_streams, err)) {
            raiseError(QStringLiteral("Unable to query SpikeGLX streams: %1").arg(err));
            return false;
        }
        if (m_streams.empty()) {
            raiseError(QStringLiteral("SpikeGLX reports no enabled data streams."));
            return false;
        }

        // dataset & static attributes
        m_dataset = createDefaultDataset(name());
        if (!m_dataset)
            return false;
        m_dataset->insertAttribute("spikeglx_version", m_client.version());
        m_dataset->insertAttribute("host", host.toStdString());
        m_dataset->insertAttribute("port", static_cast<int64_t>(port));
        m_dataset->insertAttribute("run_control", runControlModeString(m_mode)); // effective control mode
        if (!m_settingsDlg->deviceString().isEmpty())
            m_dataset->insertAttribute("device_string", m_settingsDlg->deviceString().toStdString());
        if (auto addrs = m_client.probeAddrs())
            m_dataset->insertAttribute("probe_addresses", *addrs);
        m_dataset->insertAttribute("timestamp_method", "polled-tcp");
        m_dataset->insertAttribute("live_data_enabled", m_fetchEnabled);
        if (m_fetchEnabled)
            m_dataset->insertAttribute("live_data_overrun_policy", std::string{m_abortOnOverrun ? "abort" : "skip"});
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
            m_dataset->insertAttribute("streams", streamsMeta);
        }
        if (m_settingsDlg->storeParams()) {
            if (auto params = m_client.params()) {
                MetaStringMap pm;
                for (const auto &[k, v] : *params)
                    pm.insert(k, MetaValue(v));
                m_dataset->insertAttribute("spikeglx_params", pm);
            } else {
                LOG_WARNING(m_log, "Unable to fetch SpikeGLX parameters: {}", params.error());
            }
        }

        // live-data ports
        m_portsMeta.clear();
        if (m_fetchEnabled) {
            for (auto &fs : m_fetchStreams) {
                if (!configureFetchStream(fs))
                    return false;
            }
            m_dataset->insertAttribute("live_data_ports", m_portsMeta);
        }

        // sample-count log
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
            ss.writer->setSyncMode(TSyncFileMode::CONTINUOUS);
            ss.writer->setTimeNames("sample-count", "master-time");
            ss.writer->setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
            ss.writer->setTimeDataTypes(TSyncFileDataType::UINT64, TSyncFileDataType::UINT64);
            ss.writer->setChunkSize(120); // new chunk about every 2 min at 1 Hz

            auto fname = m_dataset->addAuxDataFile(
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
            if (!ss.writer->open(name().toStdString(), m_dataset->collectionId(), userData)) {
                raiseError(QStringLiteral("Unable to open time-sync file: %1").arg(qstr(ss.writer->lastError())));
                return false;
            }
            m_syncStreams.push_back(std::move(ss));
        }

        // run name & SpikeGLX run start
        m_runName = makeRunName(info);
        if (m_mode == RunControlMode::FullControl) {
            setStatusMessage(QStringLiteral("Starting SpikeGLX run '%1'…").arg(m_runName));
            if (auto r = m_client.startRun(m_runName.toStdString()); !r) {
                raiseError(QStringLiteral("Unable to start SpikeGLX run '%1': %2").arg(m_runName, qstr(r.error())));
                return false;
            }
            m_sglxRunStartedByUs = true;

            if (!waitForStreams(30000))
                return false;
        } else {
            if (auto rn = m_client.runName())
                m_dataset->insertAttribute("run_name", rn->c_str());
        }

        // Push our identity into the metadata of the next SpikeGLX file-set, i.e. the one
        // created when the gate opens. SpikeGLX attaches pending metadata when a file-set is
        // opened, so this has to happen before SETRECORDENAB 1 - and doing it here keeps the
        // round-trips out of the time-critical path between the start signal and the gate.
        if (m_mode != RunControlMode::Monitor && m_pushMetadata) {
            std::map<std::string, std::string> kv;
            kv["sy_collection_id"] = m_dataset->collectionId().toHex();
            kv["sy_subject_id"] = m_subject.id.toStdString();
            kv["sy_subject_group"] = m_subject.group.toStdString();
            kv["sy_experiment_id"] = m_experimentId.toStdString();
            kv["sy_run_name"] = m_runName.toStdString();
            kv["sy_module_name"] = name().toStdString();
            kv["sy_instance_id"] = m_instanceId.toStdString();
            if (m_isEphemeralRun)
                kv["sy_ephemeral_run"] = "true";
            if (auto r = m_client.setMetadata(kv); !r)
                LOG_WARNING(m_log, "Unable to set SpikeGLX metadata: {}", r.error());
        }

        for (auto &fs : m_fetchStreams) {
            if (m_fetchEnabled)
                fs.stream->start();
        }

        setStatusMessage(QStringLiteral("Ready (%1)").arg(m_runName));
        m_threadDone = false;
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
    bool configureFetchStream(FetchStream &fs)
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
        auto rel = SglxUtils::parseChannelSpec(chanSpec, count);
        if (!rel) {
            raiseError(QStringLiteral("Live-data entry '%1 %2': %3")
                           .arg(fs.streamName, SglxUtils::chanGroupName(fs.group), rel.error()));
            return false;
        }
        fs.relChans = std::move(*rel);
        fs.absChans.clear();
        fs.absChans.reserve(fs.relChans.size());
        for (const auto c : fs.relChans)
            fs.absChans.push_back(offset + c);
        fs.sampleRate = si->sampleRate;
        fs.maxSamps = std::clamp(static_cast<int>(std::lround(fs.sampleRate * m_fetchMaxBlockMs / 1000.0)), 1, 999999);

        // scaling: check the first and last channel of the selection
        double scale = 1.0;
        const bool digital = SglxUtils::isDigitalGroup(fs.group);
        if (!digital) {
            auto first = m_client.i16ToVolts(fs.sid, fs.absChans.front());
            auto last = m_client.i16ToVolts(fs.sid, fs.absChans.back());
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

        MetaArray names;
        const auto groupName = SglxUtils::chanGroupName(fs.group);
        for (const auto c : fs.relChans) {
            if (fs.group == SglxUtils::ChanGroup::ALL)
                names.push_back(QStringLiteral("CH%1").arg(c).toStdString());
            else
                names.push_back(QStringLiteral("%1%2").arg(groupName).arg(c).toStdString());
        }
        MetaArray absChans;
        for (const auto c : fs.absChans)
            absChans.push_back(static_cast<int64_t>(c));

        fs.stream->setMetadataValue("sample_rate", fs.sampleRate);
        fs.stream->setMetadataValue("time_unit", std::string{"index"});
        fs.stream->setMetadataValue("data_unit", std::string{digital ? "raw" : "V"});
        fs.stream->setMetadataValue("data_scale", scale);
        fs.stream->setMetadataValue("data_offset", 0.0);
        fs.stream->setMetadataValue("is_digital", digital);
        fs.stream->setMetadataValue("signal_names", names);
        fs.stream->setMetadataValue("spikeglx_stream", fs.streamName.toStdString());
        fs.stream->setMetadataValue("spikeglx_js", static_cast<int64_t>(fs.sid.js));
        fs.stream->setMetadataValue("spikeglx_ip", static_cast<int64_t>(fs.sid.ip));
        fs.stream->setMetadataValue("spikeglx_group", groupName.toStdString());
        fs.stream->setMetadataValue("spikeglx_channels", absChans);
        if (!si->serial.isEmpty())
            fs.stream->setMetadataValue("spikeglx_serial", si->serial.toStdString());
        fs.stream->setSuggestedDataName(
            QStringLiteral("%1-%2/%3").arg(datasetNameSuggestion(), fs.streamName, groupName.toLower()));

        MetaStringMap portMeta;
        portMeta.insert("stream", fs.streamName.toStdString());
        portMeta.insert("group", groupName.toStdString());
        portMeta.insert("channels", absChans);
        portMeta.insert("sample_rate", fs.sampleRate);
        portMeta.insert("data_unit", std::string{digital ? "raw" : "V"});
        portMeta.insert("data_scale", scale);
        m_portsMeta.insert(fs.portId.toStdString(), portMeta);

        fs.block = SignalBlockI16(1, static_cast<uint>(fs.absChans.size()));
        return true;
    }

    /**
     * After STARTRUN, wait until SpikeGLX reports the run as active and all
     * streams we touch deliver samples.
     */
    bool waitForStreams(int timeoutMs)
    {
        std::vector<Sglx::StreamId> touched;
        for (const auto &fs : m_fetchStreams) {
            if (m_fetchEnabled)
                touched.push_back(fs.sid);
        }
        for (const auto &ss : m_syncStreams)
            touched.push_back(ss.sid);
        if (touched.empty())
            touched.push_back(m_streams.front().sid);

        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            appProcessEvents();
            auto running = m_client.isRunning();
            if (!running) {
                raiseError(qstr(running.error()));
                return false;
            }
            if (*running) {
                bool allUp = true;
                for (const auto &sid : touched) {
                    auto cnt = m_client.sampleCount(sid);
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
     * Fetch everything new on one stream and publish it as signal blocks.
     * Returns false on a fatal error (already reported).
     */
    bool pumpFetchStream(FetchStream &fs)
    {
        for (int iter = 0; iter < 64 && m_running; ++iter) {
            auto res = m_client.fetch(fs.sid, fs.cursor, fs.maxSamps, fs.absChans, fs.buffer);
            const auto recvTs = m_syTimer->timeSinceStartUsec();
            if (!res) {
                if (res.error().find("Too late") != std::string::npos) {
                    if (m_abortOnOverrun) {
                        raiseError(QStringLiteral(
                                       "Syntalos fell behind SpikeGLX: live data of '%1' was overwritten in the "
                                       "SpikeGLX buffer before it could be fetched. The run was aborted because "
                                       "complete live data was requested; the SpikeGLX files on the remote "
                                       "computer are not affected.")
                                       .arg(fs.streamName));
                        return false;
                    }
                    // we fell behind the server's ring buffer, resynchronize
                    auto cnt = m_client.sampleCount(fs.sid);
                    if (!cnt) {
                        raiseError(QStringLiteral("Unable to resynchronize with SpikeGLX stream '%1': %2")
                                       .arg(fs.streamName, qstr(cnt.error())));
                        return false;
                    }
                    const auto newCursor = std::max<uint64_t>(*cnt, 1);
                    fs.droppedSamples += newCursor > fs.cursor ? newCursor - fs.cursor : 0;
                    fs.gapCount++;
                    LOG_WARNING(
                        m_log,
                        "Live data of '{}' fell behind the SpikeGLX buffer, skipping {} samples",
                        fs.streamName,
                        newCursor - fs.cursor);
                    fs.cursor = newCursor;
                    return true;
                }
                if (auto running = m_client.isRunning(); running && !*running)
                    raiseError(QStringLiteral("SpikeGLX stopped running unexpectedly."));
                else
                    raiseError(QStringLiteral("Fetching live data from '%1' failed: %2")
                                   .arg(fs.streamName, qstr(res.error())));
                return false;
            }

            if (res->nSamps <= 0)
                return true; // no new data yet

            if (res->headCt != fs.cursor) {
                // should not happen without a "Too late" error, but keep the counters honest
                if (m_abortOnOverrun && res->headCt > fs.cursor) {
                    raiseError(QStringLiteral(
                                   "Live data of '%1' has a gap of %2 samples; the run was aborted "
                                   "because complete live data was requested.")
                                   .arg(fs.streamName)
                                   .arg(res->headCt - fs.cursor));
                    return false;
                }
                fs.gapCount++;
                if (res->headCt > fs.cursor)
                    fs.droppedSamples += res->headCt - fs.cursor;
            }

            const int n = res->nSamps;
            const int nCh = res->nChans;
            fs.block.data.resize(n, nCh);
            fs.block.timestamps.resize(n);
            // the SDK buffer is sample-major int16, exactly our row-major block layout
            fs.block.data = Eigen::Map<const MatrixXi16>(fs.buffer.data(), n, nCh);
            for (int s = 0; s < n; ++s) {
                const int64_t idx = static_cast<int64_t>(res->headCt + s) - static_cast<int64_t>(fs.refSampleCount)
                                    + fs.startSampleOffset;
                fs.block.timestamps(s) = static_cast<uint64_t>(std::max<int64_t>(idx, 0));
            }
            if (fs.syncer)
                fs.syncer->processTimestamps(recvTs, 0, 1, fs.block.timestamps);

            fs.stream->push(fs.block);
            fs.cursor = res->headCt + n;
            fs.fetchedSamples += n;

            if (n < fs.maxSamps)
                return true; // drained
        }
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        auto markDone = qScopeGuard([this] {
            m_threadDone = true;
        });

        bool failed = false;
        startWaitCondition->wait(this);
        const auto startTime = m_syTimer->startTime();
        const auto startWallUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                     m_syTimer->startWallTime().time_since_epoch())
                                     .count();
        m_dataset->insertAttribute("run_start_wall_time_us", startWallUs);

        // Open the recording gate right away (identity and prep data has been sent in prepare())
        if (m_mode != RunControlMode::Monitor) {
            Sglx::Client::Result<void> r;
            const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = m_client.setRecordingEnable(true));
            if (!r) {
                raiseError(QStringLiteral("Unable to enable recording in SpikeGLX: %1").arg(qstr(r.error())));
                failed = true;
            } else {
                m_dataset->insertAttribute("record_start_master_time_us", static_cast<int64_t>(ts.count()));
                LOG_INFO(m_log, "SpikeGLX recording enabled at {} µs", ts.count());
            }
        }

        // reference points: sample count <-> master time
        if (!failed) {
            MetaStringMap refs;
            auto takeReference = [&](Sglx::StreamId sid, const QString &sname, uint64_t &count, microseconds_t &time) {
                Sglx::Client::Result<uint64_t> cnt;
                time = FUNC_EXEC_TIMESTAMP(startTime, cnt = m_client.sampleCount(sid));
                if (!cnt) {
                    raiseError(QStringLiteral("Unable to read sample count of '%1': %2").arg(sname, qstr(cnt.error())));
                    return false;
                }
                count = *cnt;
                MetaStringMap rm;
                rm.insert("sample_count", static_cast<int64_t>(count));
                rm.insert("master_time_us", static_cast<int64_t>(time.count()));
                refs.insert(sname.toStdString(), rm);
                return true;
            };

            for (auto &fs : m_fetchStreams) {
                if (!m_fetchEnabled)
                    break;
                microseconds_t t;
                if (!takeReference(fs.sid, fs.streamName, fs.refSampleCount, t)) {
                    failed = true;
                    break;
                }
                fs.cursor = std::max<uint64_t>(fs.refSampleCount, 1);
                fs.startSampleOffset = std::llround(static_cast<double>(t.count()) * fs.sampleRate / 1e6);
                fs.syncer = initCounterSynchronizer(fs.sampleRate);
                if (fs.syncer) {
                    fs.syncer->setStrategies(
                        TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);
                    fs.syncer->setTolerance(std::chrono::milliseconds(5));
                    fs.syncer->setCalibrationBlocksCount(std::max(20, 20000 / std::max(m_fetchIntervalMs, 1)));
                    if (!fs.syncer->start()) {
                        raiseError(QStringLiteral("Unable to start time synchronizer for '%1'.").arg(fs.streamName));
                        failed = true;
                        break;
                    }
                }
            }
            for (auto &ss : m_syncStreams) {
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
            m_dataset->insertAttribute("reference_points", refs);
        }

        // main loop
        auto lastSync = m_syTimer->timeSinceStartUsec();
        auto lastHealth = lastSync;
        const auto syncInterval = std::chrono::milliseconds(m_syncIntervalMs);
        const auto healthInterval = std::chrono::milliseconds(2000);
        const int sleepMs = m_fetchEnabled ? m_fetchIntervalMs : std::min(m_syncIntervalMs, 100);

        while (m_running && !failed) {
            if (m_fetchEnabled) {
                for (auto &fs : m_fetchStreams) {
                    if (!pumpFetchStream(fs)) {
                        failed = true;
                        break;
                    }
                }
                if (failed)
                    break;
            }

            const auto now = m_syTimer->timeSinceStartUsec();
            if (!m_syncStreams.empty() && now - lastSync >= syncInterval) {
                lastSync = now;
                for (auto &ss : m_syncStreams) {
                    Sglx::Client::Result<uint64_t> cnt;
                    const auto t = FUNC_EXEC_TIMESTAMP(startTime, cnt = m_client.sampleCount(ss.sid));
                    if (!cnt) {
                        raiseError(QStringLiteral("SpikeGLX stream '%1' stopped delivering samples: %2")
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
                auto running = m_client.isRunning();
                if (!running || !*running) {
                    raiseError(
                        running ? QStringLiteral("SpikeGLX stopped running unexpectedly.")
                                : QStringLiteral("Lost connection to SpikeGLX: %1").arg(qstr(running.error())));
                    failed = true;
                    break;
                }
                if (m_mode != RunControlMode::Monitor) {
                    auto saving = m_client.isSaving();
                    if (saving && !*saving) {
                        raiseError(QStringLiteral(
                            "SpikeGLX stopped writing data while the run was active (was recording disabled "
                            "manually?)."));
                        failed = true;
                        break;
                    }
                }
            }

            // sleep in small slices so we react to stop requests quickly
            for (int slept = 0; slept < sleepMs && m_running; slept += 20)
                std::this_thread::sleep_for(milliseconds_t(std::min(20, sleepMs - slept)));
        }

        // epilogue: this thread is the only user of the client during a run,
        // so all stop-time commands are issued here.
        if (m_mode != RunControlMode::Monitor) {
            Sglx::Client::Result<void> r;
            const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = m_client.setRecordingEnable(false));
            if (r)
                m_dataset->insertAttribute("record_stop_master_time_us", static_cast<int64_t>(ts.count()));
            else
                LOG_WARNING(m_log, "Unable to disable SpikeGLX recording: {}", r.error());
        }
        collectRunInfo();
        if (m_mode == RunControlMode::FullControl) {
            Sglx::Client::Result<void> r;
            const auto ts = FUNC_EXEC_TIMESTAMP(startTime, r = m_client.stopRun());
            if (r) {
                m_dataset->insertAttribute("run_stop_master_time_us", static_cast<int64_t>(ts.count()));
                m_sglxRunStartedByUs = false;
            } else {
                LOG_WARNING(m_log, "Unable to stop SpikeGLX run: {}", r.error());
            }
        }

        MetaStringMap fetchStats;
        for (auto &fs : m_fetchStreams) {
            if (fs.syncer) {
                safeStopSynchronizer(fs.syncer);
                fs.syncer.reset();
            }
            if (!m_fetchEnabled)
                continue;
            MetaStringMap sm;
            sm.insert("fetched_samples", static_cast<int64_t>(fs.fetchedSamples));
            sm.insert("gap_count", static_cast<int64_t>(fs.gapCount));
            sm.insert("dropped_samples", static_cast<int64_t>(fs.droppedSamples));
            fetchStats.insert(fs.portId.toStdString(), sm);
            if (fs.gapCount > 0)
                LOG_WARNING(
                    m_log,
                    "Live data port '{}' had {} gaps ({} samples lost); the SpikeGLX files on disk are unaffected",
                    fs.portId,
                    fs.gapCount,
                    fs.droppedSamples);
        }
        if (m_fetchEnabled)
            m_dataset->insertAttribute("live_data", fetchStats);
    }

    /** Record where SpikeGLX wrote its files. */
    void collectRunInfo()
    {
        auto dir = m_client.dataDir();
        auto rn = m_client.runName();
        auto gt = m_client.lastGT();
        if (dir)
            m_dataset->insertAttribute("remote_data_dir", *dir);
        if (rn)
            m_dataset->insertAttribute("remote_run_name", *rn);
        if (gt) {
            m_dataset->insertAttribute("last_gate_index", static_cast<int64_t>(gt->first));
            m_dataset->insertAttribute("last_trigger_index", static_cast<int64_t>(gt->second));
        }
        if (dir && rn && gt && gt->first >= 0) {
            const auto runDir = QStringLiteral("%1/%2_g%3").arg(qstr(*dir), qstr(*rn)).arg(gt->first);
            m_dataset->insertAttribute("remote_run_dir", runDir.toStdString());
            m_dataset->insertAttribute(
                "remote_file_prefix",
                QStringLiteral("%1_g%2_t%3").arg(qstr(*rn)).arg(gt->first).arg(gt->second).toStdString());

            // list the files SpikeGLX wrote for this run (best effort)
            if (auto files = m_client.enumDataDir()) {
                const auto needle = QStringLiteral("/%1_g%2/").arg(qstr(*rn)).arg(gt->first);
                MetaArray remoteFiles;
                for (const auto &f : *files) {
                    const auto qf = qstr(f);
                    if (qf.contains(needle)
                        && (qf.endsWith(QLatin1String(".bin")) || qf.endsWith(QLatin1String(".meta"))))
                        remoteFiles.push_back(f);
                }
                if (!remoteFiles.empty())
                    m_dataset->insertAttribute("remote_files", remoteFiles);
            }
        }
    }

    void stop() override
    {
        // The engine calls stop() before clearing m_running, so we do it
        // ourselves and wait for the run thread to finish its epilogue.
        m_running = false;
        QElapsedTimer timer;
        timer.start();
        while (!m_threadDone) {
            if (timer.elapsed() > 15000) {
                LOG_CRITICAL(m_log, "SpikeGLX run thread did not finish in time");
                break;
            }
            processUiEvents();
            QThread::msleep(2);
        }

        for (auto &ss : m_syncStreams) {
            if (ss.writer)
                ss.writer->close();
        }
        m_syncStreams.clear();

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
        settings.insert(QStringLiteral("push_metadata"), m_settingsDlg->pushMetadata());
        settings.insert(QStringLiteral("store_params"), m_settingsDlg->storeParams());
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
        m_settingsDlg->setPushMetadata(settings.value(QStringLiteral("push_metadata"), true).toBool());
        m_settingsDlg->setStoreParams(settings.value(QStringLiteral("store_params"), true).toBool());
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
