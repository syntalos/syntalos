/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "oeacqmodule.h"

#include "datactl/datatypes.h"
#include "datactl/streammeta.h"
#include "datactl/syclock.h"
#include "datactl/timesync.h"

#include "devices/AcquisitionBoard.h"
#include "devices/simulated/AcqBoardSim.h"
#include "oeacqsettingsdialog.h"

#include <QStringList>
#include <chrono>
#include <memory>
#include <vector>

using namespace Syntalos;

SYNTALOS_MODULE(OpenEphysAcqModule)

namespace
{
struct GroupStream {
    int groupIndex = -1;
    ChannelKind kind = ChannelKind::Electrode;
    QString portId;
    QString streamPrefix;
    int channelsPerSample = 0;
    std::shared_ptr<DataStream<IntSignalBlock>> stream;
    std::shared_ptr<IntSignalBlock> block;
    std::vector<std::string> channelNames;
};

constexpr int kBoardAdcChannels = 8;
constexpr int kHeadstageAuxChannels = 3;

QString headstagePortTitle(const std::string &prefix, int chanCount)
{
    return QStringLiteral("Headstage %1 - %2 ch")
        .arg(QString::fromStdString(prefix))
        .arg(chanCount);
}

QString auxPortTitle(const std::string &prefix)
{
    return QStringLiteral("Headstage %1 - AUX (3 ch)").arg(QString::fromStdString(prefix));
}

QString adcPortTitle()
{
    return QStringLiteral("Board ADC (8 ch)");
}
} // namespace

class OpenEphysAcqModule : public AbstractModule
{
    Q_OBJECT

public:
    explicit OpenEphysAcqModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_log(Syntalos::getLogger("oeacq.module"))
    {
    }

    ~OpenEphysAcqModule() override = default;

    ModuleFeatures features() const override
    {
        return ModuleFeature::REALTIME | ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool initialize() override
    {
        // For now we only have a working simulated backend. The ONI backend
        // will replace / be selected ahead of this once its .cpp port lands.
        m_board = std::make_unique<AcqBoardSim>();

        if (!m_board->detectBoard()) {
            raiseError(QStringLiteral("No Open Ephys Acquisition Board detected."));
            return false;
        }
        if (!m_board->initializeBoard()) {
            raiseError(QStringLiteral("Failed to initialize the Open Ephys Acquisition Board."));
            return false;
        }

        // Synchronously scan the ports so we know which headstages are
        // attached before prepare() runs.
        m_board->scanPorts();
        LOG_INFO(
            m_log,
            "Open Ephys Acquisition Board ready (board type: {}, headstages: {}).",
            static_cast<int>(m_board->getBoardType()),
            m_board->getHeadstages().size());

        // Register one output port per detected headstage so the user can
        // wire connections at design time. The actual signal-block size is
        // (re)assigned per run inside prepare().
        rebuildOutputPorts();

        // Register a TTL trigger input port. Upstream modules can push
        // FirmataControl::WRITE_DIGITAL_PULSE events here to fire one of the
        // board's digital output lines for `value` ms. Other Firmata commands
        // are accepted but only pulses are honored against the board.
        m_ttlIn = registerInputPort<FirmataControl>(
            QStringLiteral("ttl-in"), QStringLiteral("TTL Triggers"));

        // Build the settings dialog once we know what the board can offer.
        m_settingsDlg = new OeAcqSettingsDialog;
        m_settingsDlg->setAvailableSampleRates(m_board->getAvailableSampleRates());
        m_settingsDlg->setSampleRateHz(m_sampleRateHz);
        m_settingsDlg->setAcquireAux(m_acquireAux);
        m_settingsDlg->setAcquireAdc(m_acquireAdc);
        m_settingsDlg->setNamingScheme(m_namingScheme);
        m_settingsDlg->setHeadstageSummary(m_board->getHeadstages());

        connect(m_settingsDlg, &OeAcqSettingsDialog::sampleRateChanged, this, [this](int hz) {
            m_sampleRateHz = hz;
            if (m_board)
                m_board->setSampleRate(hz);
        });
        connect(m_settingsDlg, &OeAcqSettingsDialog::acquireAuxChanged, this, [this](bool on) {
            m_acquireAux = on;
            if (m_board)
                m_board->enableAuxChannels(on);
        });
        connect(m_settingsDlg, &OeAcqSettingsDialog::acquireAdcChanged, this, [this](bool on) {
            m_acquireAdc = on;
            if (m_board)
                m_board->enableAdcChannels(on);
        });
        connect(m_settingsDlg, &OeAcqSettingsDialog::namingSchemeChanged, this, [this](int s) {
            m_namingScheme = static_cast<ChannelNamingScheme>(s);
            if (m_board)
                m_board->setNamingScheme(m_namingScheme);
            // Channel names changed - rebuild ports so port labels match.
            rebuildOutputPorts();
            m_settingsDlg->setHeadstageSummary(m_board->getHeadstages());
        });
        connect(m_settingsDlg, &OeAcqSettingsDialog::rescanRequested, this, [this]() {
            if (!m_board)
                return;
            m_board->scanPorts();
            rebuildOutputPorts();
            m_settingsDlg->setHeadstageSummary(m_board->getHeadstages());
        });

        addSettingsWindow(m_settingsDlg);
        return true;
    }

    bool prepare(const TestSubject &) override
    {
        if (!m_board) {
            raiseError(QStringLiteral("Acquisition board has not been initialized."));
            return false;
        }

        // Pick up the TTL subscription if anything is wired into us.
        if (m_ttlIn && m_ttlIn->hasSubscription())
            m_ttlSub = m_ttlIn->subscription();
        else
            m_ttlSub.reset();

        // Apply current settings to the board.
        m_board->setSampleRate(m_sampleRateHz);
        m_board->enableAuxChannels(m_acquireAux);
        m_board->enableAdcChannels(m_acquireAdc);
        m_board->setNamingScheme(GLOBAL_INDEX);

        const auto headstages = m_board->getHeadstages();
        if (headstages.empty()) {
            raiseError(QStringLiteral(
                "No connected headstages were detected. Cannot start acquisition."));
            return false;
        }

        // Refresh per-port metadata against the current settings (sample rate,
        // bit volts, channel names). The port set itself is owned by
        // initialize() / rebuildOutputPorts().
        refreshGroupMetadata();

        if (m_groups.empty()) {
            raiseError(QStringLiteral("No headstage produced any active channels."));
            return false;
        }

        for (auto &port : outPorts())
            port->startStream();

        // Time synchronization: align device sample-counter timestamps to the
        // master clock. WRITE_TSYNCFILE intentionally not enabled here - the
        // settings dialog (Task 7) will hook a dataset basename in once it
        // exists.
        m_clockSync = initCounterSynchronizer(static_cast<double>(m_board->getSampleRate()));
        if (m_clockSync) {
            m_clockSync->setTolerance(std::chrono::microseconds(1400));
            if (!m_clockSync->start()) {
                raiseError(QStringLiteral("Unable to start time synchronizer."));
                return false;
            }
        }

        // Lock the settings UI for the duration of the run. Live changes
        // would reconfigure the board behind the run loop and crash.
        setSettingsRunLock(true);

        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        if (!m_board || m_groups.empty())
            return;

        // Build the per-pump chunk vector (one entry per group). The board
        // fills numSamples / samples / sampleIndices in place every call.
        std::vector<AcqSampleChunk> chunks;
        chunks.reserve(m_groups.size());
        for (const auto &g : m_groups) {
            AcqSampleChunk c;
            c.groupIndex = g.groupIndex;
            c.kind = g.kind;
            c.channelsPerSample = g.channelsPerSample;
            chunks.push_back(std::move(c));
        }

        startWaitCondition->wait(this);

        if (!m_board->startAcquisition()) {
            raiseError(QStringLiteral("Failed to start acquisition."));
            return;
        }

        while (m_running) {
            if (!m_board->pumpSamples(std::span<AcqSampleChunk>(chunks.data(), chunks.size())))
                break;

            const auto blockRecvTime = m_syTimer->timeSinceStartUsec();

            for (size_t gi = 0; gi < chunks.size(); ++gi) {
                auto &chunk = chunks[gi];
                auto &g = m_groups[gi];
                if (chunk.numSamples <= 0)
                    continue;

                const int n = chunk.numSamples;
                const int cps = chunk.channelsPerSample;

                g.block->data.resize(n, cps);
                g.block->timestamps.resize(n);

                // Row-major copy: chunk.samples and IntSignalBlock::data are
                // both row-major (samples × channels), so this is an int32
                // widen on every cell.
                for (int s = 0; s < n; ++s) {
                    const uint16_t *row =
                        chunk.samples.data() + static_cast<size_t>(s) * cps;
                    for (int c = 0; c < cps; ++c)
                        g.block->data(s, c) = static_cast<int32_t>(row[c]);
                    g.block->timestamps(s) = chunk.sampleIndices[s];
                }

                if (m_clockSync) {
                    m_clockSync->processTimestamps(
                        blockRecvTime, /* blockIndex */ 0, /* blockCount */ 1, g.block->timestamps);
                }

                g.stream->push(*g.block);
            }

            // Dispatch any incoming TTL trigger commands onto the board.
            if (m_ttlSub) {
                while (auto evt = m_ttlSub->peekNext()) {
                    dispatchTtlEvent(*evt);
                }
            }

            m_board->drainElapsedDigitalDeadlines();
        }

        m_board->stopAcquisition();
    }

    /**
     * Translate one Firmata control command into a board digital output
     * action. Only WRITE_DIGITAL_PULSE is supported today; other variants
     * log a warning and are ignored.
     */
    void dispatchTtlEvent(const FirmataControl &fc)
    {
        if (!m_board)
            return;

        switch (fc.command) {
        case FirmataCommandKind::WRITE_DIGITAL_PULSE: {
            const int line = static_cast<int>(fc.pinId);
            const int durMs = std::max(1, static_cast<int>(fc.value));
            if (line < 0 || line >= 16) {
                LOG_WARNING(
                    m_log,
                    "TTL pulse for line {} ignored: only lines 0..15 are valid.",
                    line);
                return;
            }
            m_board->triggerDigitalOutput(line, durMs);
            break;
        }
        case FirmataCommandKind::WRITE_DIGITAL:
            LOG_WARNING(
                m_log,
                "TTL line-level command on pin {} ignored: only WRITE_DIGITAL_PULSE is "
                "supported by the Open Ephys AcqBoard module.",
                static_cast<int>(fc.pinId));
            break;
        default:
            // Pin-mode setup, analog writes, etc. are not relevant to a
            // digital trigger output. Silently skip.
            break;
        }
    }

    void stop() override
    {
        if (m_clockSync) {
            const auto last = m_clockSync->lastMasterAssumedAcqTS();
            safeStopSynchronizer(m_clockSync, last);
        }
        AbstractModule::stop();
        setSettingsRunLock(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("sample_rate"), m_sampleRateHz);
        settings.insert(QStringLiteral("acquire_aux"), m_acquireAux);
        settings.insert(QStringLiteral("acquire_adc"), m_acquireAdc);
        settings.insert(QStringLiteral("naming_scheme"), static_cast<int>(m_namingScheme));
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_sampleRateHz = settings.value(QStringLiteral("sample_rate"), 30000).toInt();
        m_acquireAux = settings.value(QStringLiteral("acquire_aux"), false).toBool();
        m_acquireAdc = settings.value(QStringLiteral("acquire_adc"), false).toBool();
        m_namingScheme = static_cast<ChannelNamingScheme>(
            settings.value(QStringLiteral("naming_scheme"), static_cast<int>(GLOBAL_INDEX)).toInt());

        if (m_settingsDlg) {
            m_settingsDlg->setSampleRateHz(m_sampleRateHz);
            m_settingsDlg->setAcquireAux(m_acquireAux);
            m_settingsDlg->setAcquireAdc(m_acquireAdc);
            m_settingsDlg->setNamingScheme(m_namingScheme);
        }
        return true;
    }

private:
    /**
     * Build the per-headstage stream layout for the currently-connected
     * headstages and (re)register an output port for each. Called from
     * @ref initialize() so the canvas sees ports at design time and saved
     * subscriptions can re-attach.
     */
    void rebuildOutputPorts()
    {
        m_groups.clear();
        if (!m_board)
            return;

        const auto headstages = m_board->getHeadstages();
        m_groups.reserve(headstages.size() * 2 + 1);

        for (size_t i = 0; i < headstages.size(); ++i) {
            const Headstage *hs = headstages[i];
            if (!hs->isConnected())
                continue;

            const int chanCount = hs->getNumActiveChannels();
            if (chanCount <= 0)
                continue;

            const auto prefix = QString::fromStdString(hs->getStreamPrefix());
            const auto prefixLower = prefix.toLower();

            // ---- electrode group ----
            {
                GroupStream g;
                g.groupIndex = static_cast<int>(i);
                g.kind = ChannelKind::Electrode;
                g.streamPrefix = prefix;
                g.channelsPerSample = chanCount;
                g.portId = QStringLiteral("hs-%1").arg(prefixLower);
                g.stream = registerOutputPort<IntSignalBlock>(
                    g.portId, headstagePortTitle(hs->getStreamPrefix(), chanCount));

                g.channelNames.reserve(chanCount);
                for (int c = 0; c < chanCount; ++c)
                    g.channelNames.push_back(hs->getChannelName(c));

                g.block = std::make_shared<IntSignalBlock>(0, chanCount);
                m_groups.push_back(std::move(g));
            }

            // ---- aux group (always registered; fed only when AUX enabled) ----
            {
                GroupStream g;
                g.groupIndex = static_cast<int>(i);
                g.kind = ChannelKind::Aux;
                g.streamPrefix = prefix;
                g.channelsPerSample = kHeadstageAuxChannels;
                g.portId = QStringLiteral("hs-%1-aux").arg(prefixLower);
                g.stream = registerOutputPort<IntSignalBlock>(
                    g.portId, auxPortTitle(hs->getStreamPrefix()));

                g.channelNames = {
                    hs->getStreamPrefix() + "_AUX1",
                    hs->getStreamPrefix() + "_AUX2",
                    hs->getStreamPrefix() + "_AUX3",
                };

                g.block = std::make_shared<IntSignalBlock>(0, kHeadstageAuxChannels);
                m_groups.push_back(std::move(g));
            }
        }

        // ---- board ADC (always registered; fed only when ADC enabled) ----
        {
            GroupStream g;
            g.groupIndex = -1;
            g.kind = ChannelKind::Adc;
            g.channelsPerSample = kBoardAdcChannels;
            g.portId = QStringLiteral("adc");
            g.stream = registerOutputPort<IntSignalBlock>(g.portId, adcPortTitle());

            g.channelNames.reserve(kBoardAdcChannels);
            for (int c = 0; c < kBoardAdcChannels; ++c)
                g.channelNames.push_back("ADC" + std::to_string(c + 1));

            g.block = std::make_shared<IntSignalBlock>(0, kBoardAdcChannels);
            m_groups.push_back(std::move(g));
        }
    }

    /**
     * Toggle the settings dialog's run-locked state. Safe to call from any
     * thread - the actual widget update is dispatched onto the GUI thread.
     */
    void setSettingsRunLock(bool active)
    {
        if (!m_settingsDlg)
            return;
        QMetaObject::invokeMethod(
            m_settingsDlg,
            [dlg = m_settingsDlg, active]() {
                dlg->setRunActive(active);
            },
            Qt::QueuedConnection);
    }

    /** Push current sample rate / bit-volts / channel names onto each port. */
    void refreshGroupMetadata()
    {
        if (!m_board)
            return;

        const float sampleRate = m_board->getSampleRate();

        for (auto &g : m_groups) {
            if (!g.stream)
                continue;

            MetaArray namesMeta;
            namesMeta.reserve(g.channelNames.size());
            for (const auto &n : g.channelNames)
                namesMeta.push_back(MetaValue{n});

            g.stream->setMetadataValue("sample_rate", static_cast<double>(sampleRate));
            g.stream->setMetadataValue("time_unit", std::string{"index"});
            g.stream->setMetadataValue("data_unit", std::string{"raw_uint16"});
            g.stream->setMetadataValue(
                "bit_volts", static_cast<double>(m_board->getBitVolts(g.kind)));
            g.stream->setMetadataValue("signal_names", namesMeta);
        }
    }

    std::unique_ptr<AcquisitionBoard> m_board;
    std::vector<GroupStream> m_groups;
    std::unique_ptr<FreqCounterSynchronizer> m_clockSync;
    Syntalos::QuillLogger *m_log;
    OeAcqSettingsDialog *m_settingsDlg = nullptr;

    std::shared_ptr<StreamInputPort<FirmataControl>> m_ttlIn;
    std::shared_ptr<StreamSubscription<FirmataControl>> m_ttlSub;

    int m_sampleRateHz = 30000;
    bool m_acquireAux = false;
    bool m_acquireAdc = false;
    ChannelNamingScheme m_namingScheme = GLOBAL_INDEX;
};

QString OpenEphysAcqModuleInfo::id() const
{
    return QStringLiteral("open-ephys-acq");
}

QString OpenEphysAcqModuleInfo::name() const
{
    return QStringLiteral("Open Ephys AcqBoard");
}

QString OpenEphysAcqModuleInfo::description() const
{
    return QStringLiteral("Streams data from any generation of the Open Ephys Acquisition Board.");
}

QString OpenEphysAcqModuleInfo::authors() const
{
    return QStringLiteral(
        "Josh Siegle\n"
        "Aarón Cuevas López\n"
        "Brandon Parks\n"
        "Matthias Klumpp");
}

ModuleCategories OpenEphysAcqModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *OpenEphysAcqModuleInfo::createModule(QObject *parent)
{
    return new OpenEphysAcqModule(parent);
}

#include "oeacqmodule.moc"
