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
#include "devices/oni/AcqBoardONI.h"
#include "devices/simulated/AcqBoardSim.h"
#include "oeacqimpedancedialog.h"
#include "oeacqsettingsdialog.h"

#include <QByteArray>
#include <QMessageBox>
#include <QSet>
#include <QStringList>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>

using namespace Syntalos;

SYNTALOS_MODULE(OpenEphysAcqModule)

struct GroupStream {
    int groupIndex = -1;
    ChannelKind kind = ChannelKind::Electrode;
    QString portId;
    QString streamPrefix;
    /** Hardware-fixed number of columns the board fills per sample. */
    int rawChannelsPerSample = 0;
    /** Width of the published SignalBlockU16 (mask-filtered). */
    int channelsPerSample = 0;
    /** Map from output column index -> raw column index. */
    std::vector<int> enabledLocalIndices;
    /** Canonical channel ID for each output column (see makeChannelId). */
    QStringList outChannelIds;

    /** Per-output-column zero-fill flag. Written by the GUI thread when the
     *  user toggles a checkbox during a run; read by the run thread on every
     *  sample copy. */
    std::unique_ptr<std::atomic_bool[]> mutedOutputColumns;
    std::shared_ptr<DataStream<SignalBlockU16>> stream;
    std::shared_ptr<SignalBlockU16> block;
    /** Names of enabled channels only. */
    std::vector<std::string> channelNames;
};

/**
 * Build a canonical channel ID stable across rescans.
 *   Electrode -> "<prefix>:E<i>"
 *   Aux       -> "<prefix>:AUX<i>"
 *   ADC       -> "ADC:<i>"
 */
static QString makeChannelId(const QString &portPrefix, ChannelKind kind, int idx)
{
    switch (kind) {
    case ChannelKind::Electrode:
        return QStringLiteral("%1:E%2").arg(portPrefix).arg(idx, 2, 10, QChar(u'0'));
    case ChannelKind::Aux:
        return QStringLiteral("%1:AUX%2").arg(portPrefix).arg(idx, 2, 10, QChar(u'0'));
    case ChannelKind::Adc:
        return QStringLiteral("ADC:%1").arg(idx, 2, 10, QChar(u'0'));
    }
    return {};
}

static QString headstagePortTitle(const std::string &prefix, int chanCount)
{
    return QStringLiteral("Headstage %1 - %2 ch").arg(QString::fromStdString(prefix)).arg(chanCount);
}

static QString auxPortTitle(const std::string &prefix, int auxCount)
{
    return QStringLiteral("Headstage %1 - AUX (%2 ch)").arg(QString::fromStdString(prefix)).arg(auxCount);
}

static QString adcPortTitle(int adcCount)
{
    return QStringLiteral("Board ADC (%1 ch)").arg(adcCount);
}

class OpenEphysAcqModule : public AbstractModule
{
    Q_OBJECT

public:
    explicit OpenEphysAcqModule(OpenEphysAcqModuleInfo *info, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_settingsDlg = new OeAcqSettingsDialog;
        m_settingsDlg->setWindowIcon(info->icon());
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
        // Load the simulation backend initially, so some backend is always available
        setupBackend();

        // Register a TTL trigger input port. Upstream modules can push
        // LineCommand events here to fire one of the board's digital output lines.
        m_ttlIn = registerInputPort<LineCommand>(QStringLiteral("ttl-in"), QStringLiteral("TTL Triggers"));

        // Build the settings dialog once we know what the board can offer.
        m_settingsDlg->setAvailableSampleRates(m_board->getAvailableSampleRates());
        m_settingsDlg->setSampleRateHz(m_sampleRateHz);
        m_settingsDlg->setAcquireAux(m_acquireAux);
        m_settingsDlg->setAcquireAdc(m_acquireAdc);
        m_settingsDlg->setNamingScheme(m_namingScheme);
        m_settingsDlg->setBackendChoice(m_backendChoice);
        m_settingsDlg->setActiveBackendLabel(activeBackendDescription());
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
        connect(m_settingsDlg, &OeAcqSettingsDialog::backendChoiceChanged, this, [this](auto c) {
            m_backendChoice = c;
            setupBackend();
        });
        connect(m_settingsDlg, &OeAcqSettingsDialog::reconnectRequested, this, [this]() {
            setupBackend();
        });

        connect(m_settingsDlg, &OeAcqSettingsDialog::measureImpedancesRequested, this, [this]() {
            if (!m_board || m_running)
                return;
            OeAcqImpedanceDialog dlg(m_board.get(), m_settingsDlg);
            dlg.exec();
            // Refresh the headstage summary — channel impedance values
            // changed on each Headstage, the textual summary doesn't, but
            // a future dialog may want to surface "scan ran at <time>".
            m_settingsDlg->setHeadstageSummary(m_board->getHeadstages());
        });

        connect(
            m_settingsDlg,
            &OeAcqSettingsDialog::channelEnabledChanged,
            this,
            [this](const QString &id, bool enabled) {
                handleChannelToggle(id, enabled);
            });

        // Reflect current mask state in the dialog now that it exists.
        refreshChannelInventoryFromBoard();

        addSettingsWindow(m_settingsDlg);
        return true;
    }

    /**
     * Handle a checkbox toggle from the channel panel. Behavior differs
     * depending on whether a run is active.
     */
    void handleChannelToggle(const QString &id, bool enabled)
    {
        if (enabled)
            m_disabledChannels.remove(id);
        else
            m_disabledChannels.insert(id);

        if (m_running) {
            // The dialog locks unchecked boxes off during a run, so this
            // signal only fires for channels with an output column.
            for (auto &g : m_groups) {
                const int idx = g.outChannelIds.indexOf(id);
                if (idx >= 0) {
                    g.mutedOutputColumns[idx].store(!enabled, std::memory_order_relaxed);
                    break;
                }
            }
            return;
        }

        // if we aren't running, we can truly disable stuff instead of just muting it
        rebuildOutputPorts();
    }

    bool prepare(const TestSubject &) override
    {
        // Lock the settings UI for the duration of the run. Live changes
        // would reconfigure the board behind the run loop and crash.
        setSettingsRunActive(true);
        auto cleanupOnFailure = qScopeGuard([this] {
            setSettingsRunActive(false);
        });

        if (!m_board) {
            raiseError(QStringLiteral("Acquisition board has not been initialized."));
            return false;
        }

        // Refuse to start if the user explicitly asked for hardware but the active backend isn't the ONI board.
        if (m_backendChoice == OeAcqSettingsDialog::BackendDevice
            && m_board->getBoardType() != AcquisitionBoard::BoardType::ONI) {
            raiseError(QStringLiteral(
                "Hardware backend is selected, but no Open Ephys Acquisition Board is currently connected. Refusing to "
                "start a run on the simulated backend."));
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
        m_board->setNamingScheme(m_namingScheme);

        const auto headstages = m_board->getHeadstages();
        if (headstages.empty()) {
            raiseError(QStringLiteral("No connected headstages were detected. Cannot start acquisition."));
            return false;
        }

        // Refresh per-port metadata against the current settings (sample rate,
        // bit volts, channel names).
        refreshGroupMetadata();

        if (m_groups.empty()) {
            raiseError(QStringLiteral("No headstage produced any active channels."));
            return false;
        }

        for (auto &port : outPorts())
            port->startStream();

        // FIXME: Do proper time synchronization!
        // THIS IS A PLACEHOLDER!
        m_clockSync = initCounterSynchronizer(static_cast<double>(m_board->getSampleRate()));
        if (m_clockSync) {
            m_clockSync->setTolerance(std::chrono::microseconds(1400));
            if (!m_clockSync->start()) {
                raiseError(QStringLiteral("Unable to start time synchronizer."));
                return false;
            }
        }

        cleanupOnFailure.dismiss();
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        if (!m_board || m_groups.empty())
            return;

        // Reset mute atomics at run start: every column starts un-muted.
        for (auto &g : m_groups) {
            for (int oc = 0; oc < g.channelsPerSample; ++oc)
                g.mutedOutputColumns[oc].store(false, std::memory_order_relaxed);
        }

        // Build the per-pump chunk vector (one entry per group). The board
        // fills numSamples / samples / sampleIndices in place every call.
        // chunk.channelsPerSample is the HARDWARE width (rawChannelsPerSample),
        // not the masked output width.
        std::vector<AcqSampleChunk> chunks;
        chunks.reserve(m_groups.size());
        for (const auto &g : m_groups) {
            AcqSampleChunk c;
            c.groupIndex = g.groupIndex;
            c.kind = g.kind;
            c.channelsPerSample = g.rawChannelsPerSample;
            chunks.push_back(std::move(c));
        }

        startWaitCondition->wait(this);

        if (!m_board->startAcquisition()) {
            raiseError(QStringLiteral("Failed to start acquisition."));
            return;
        }

        while (m_running) {
            if (!m_board->pumpSamples(std::span<AcqSampleChunk>(chunks.data(), chunks.size()))) {
                raiseError(QStringLiteral(
                    "Open Ephys acquisition read failed. The run has been aborted; "
                    "captured data may be incomplete."));
                break;
            }

            // FIXME: Perform proper time synchronization!
            const auto blockRecvTime = m_syTimer->timeSinceStartUsec();

            for (size_t gi = 0; gi < chunks.size(); ++gi) {
                auto &chunk = chunks[gi];
                auto &g = m_groups[gi];
                if (chunk.numSamples <= 0)
                    continue;

                const int n = chunk.numSamples;
                const int rawCps = chunk.channelsPerSample; // hardware width
                const int outCps = g.channelsPerSample;     // masked width

                g.block->data.resize(n, outCps);
                g.block->timestamps.resize(n);

                // Row-major copy with mask: pick enabledLocalIndices[outCol]
                // from each raw row, copy values, and zero-fill any
                // column the user disabled mid-run.
                for (int s = 0; s < n; ++s) {
                    const uint16_t *row = chunk.samples.data() + static_cast<size_t>(s) * rawCps;
                    for (int oc = 0; oc < outCps; ++oc) {
                        if (g.mutedOutputColumns[oc].load(std::memory_order_relaxed))
                            g.block->data(s, oc) = 0;
                        else
                            g.block->data(s, oc) = row[g.enabledLocalIndices[oc]];
                    }
                    g.block->timestamps(s) = chunk.sampleIndices[s];
                }

                if (m_clockSync) {
                    m_clockSync->processTimestamps(
                        blockRecvTime,
                        0, // blockIndex
                        1, // blockCount
                        g.block->timestamps);
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
     * Translate one line command into a board digital output action.
     * Only WriteDigitalPulse is honored; other kinds are ignored.
     */
    void dispatchTtlEvent(const LineCommand &fc)
    {
        if (!m_board)
            return;

        switch (fc.kind) {
        case LineCommandKind::WRITE_DIGITAL_PULSE: {
            const int line = static_cast<int>(fc.lineId);
            const auto durMs = std::max<int64_t>(1, std::chrono::duration_cast<milliseconds_t>(fc.duration).count());
            if (line < 0 || line >= 16) {
                LOG_WARNING(m_log, "TTL pulse for line {} ignored: only lines 0..15 are valid.", line);
                return;
            }
            m_board->triggerDigitalOutput(line, static_cast<int>(durMs));
            break;
        }
        case LineCommandKind::WRITE_DIGITAL:
            LOG_INFO(
                m_log,
                "TTL line-level command on line {} ignored: only WriteDigitalPulse is "
                "supported by the Open Ephys AcqBoard module.",
                static_cast<int>(fc.lineId));
            break;
        default:
            // SetMode, analog writes, etc. are not relevant to a
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
        setSettingsRunActive(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("sample_rate"), m_sampleRateHz);
        settings.insert(QStringLiteral("acquire_aux"), m_acquireAux);
        settings.insert(QStringLiteral("acquire_adc"), m_acquireAdc);
        settings.insert(QStringLiteral("naming_scheme"), static_cast<int>(m_namingScheme));
        settings.insert(QStringLiteral("backend"), m_backendChoice);

        QStringList disabled;
        disabled.reserve(m_disabledChannels.size());
        for (const auto &id : m_disabledChannels)
            disabled << id;
        disabled.sort();
        settings.insert(QStringLiteral("disabled_channels"), disabled);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_sampleRateHz = settings.value(QStringLiteral("sample_rate"), 30000).toInt();
        m_acquireAux = settings.value(QStringLiteral("acquire_aux"), false).toBool();
        m_acquireAdc = settings.value(QStringLiteral("acquire_adc"), false).toBool();
        m_namingScheme = static_cast<ChannelNamingScheme>(
            settings.value(QStringLiteral("naming_scheme"), static_cast<int>(GLOBAL_INDEX)).toInt());
        m_backendChoice = static_cast<OeAcqSettingsDialog::BackendChoice>(
            settings.value(QStringLiteral("backend"), OeAcqSettingsDialog::BackendAuto).toInt());

        const auto savedList = settings.value(QStringLiteral("disabled_channels")).toStringList();
        m_disabledChannels = QSet<QString>(savedList.cbegin(), savedList.cend());

        // initialize() set up a backend before settings were loaded, so
        // m_board may not match the saved choice. setBackendChoice() on
        // the dialog blocks signals, so updating the combo below will not
        // trigger a re-setup either. Drive it directly here.
        const auto currentType = m_board ? m_board->getBoardType() : AcquisitionBoard::BoardType::None;
        bool choiceSatisfied = false;
        switch (m_backendChoice) {
        case OeAcqSettingsDialog::BackendAuto:
            choiceSatisfied =
                (currentType == AcquisitionBoard::BoardType::ONI
                 || currentType == AcquisitionBoard::BoardType::Simulated);
            break;
        case OeAcqSettingsDialog::BackendDevice:
            choiceSatisfied = (currentType == AcquisitionBoard::BoardType::ONI);
            break;
        case OeAcqSettingsDialog::BackendSimulated:
            choiceSatisfied = (currentType == AcquisitionBoard::BoardType::Simulated);
            break;
        }
        if (!choiceSatisfied)
            setupBackend();
        else
            rebuildOutputPorts();

        if (m_settingsDlg) {
            m_settingsDlg->setSampleRateHz(m_sampleRateHz);
            m_settingsDlg->setAcquireAux(m_acquireAux);
            m_settingsDlg->setAcquireAdc(m_acquireAdc);
            m_settingsDlg->setNamingScheme(m_namingScheme);
            m_settingsDlg->setBackendChoice(m_backendChoice);
        }

        return true;
    }

private:
    /**
     * Bring up the acquisition board backend per @c m_backendChoice. Tears
     * down any previously-active board, instantiates the requested one (or
     * auto-detects), runs detect/initialize/scanPorts, rebuilds the output
     * port set, and updates the settings-dialog status. Refuses to do any of
     * this while a recording is running.
     *
     * @return true on success, false if even the simulated fallback fails.
     */
    bool setupBackend()
    {
        if (m_running) {
            // Should not be reachable — settings dialog disables these
            // controls during a run — but defend anyway.
            LOG_WARNING(m_log, "Refusing to switch backend while a run is active.");
            return m_board != nullptr;
        }

        const int choice = m_backendChoice;
        const bool
            wantDevice = (choice == OeAcqSettingsDialog::BackendAuto || choice == OeAcqSettingsDialog::BackendDevice);
        const bool deviceMandatory = (choice == OeAcqSettingsDialog::BackendDevice);

        // Drop the previous board (if any) so its FT600/USB resources are
        // released before we open a new one.
        m_board.reset();

        std::unique_ptr<AcquisitionBoard> next;

        if (wantDevice) {
            auto oni = std::make_unique<AcqBoardONI>();
            oni->setMessageCallback([this](AcquisitionBoard::MessageSeverity sev, const std::string &msg) {
                handleBoardMessage(sev, msg);
            });
            if (oni->detectBoard()) {
                next = std::move(oni);
                LOG_INFO(m_log, "Using ONI Acquisition Board backend.");
            } else if (deviceMandatory) {
                raiseError(QStringLiteral(
                    "Acquisition from hardware was requested but no Open Ephys "
                    "Acquisition Board was detected. Connect the board and press "
                    "\"Reconnect\", or switch the backend choice."));
                if (m_settingsDlg) {
                    m_settingsDlg->setActiveBackendLabel(QStringLiteral("(no device detected)"));
                    m_settingsDlg->setHeadstageSummary({});
                }
                rebuildOutputPorts();
                return false;
            } else {
                LOG_INFO(m_log, "No ONI Acquisition Board detected; using simulated backend.");
            }
        }

        if (!next) {
            // Either user explicitly chose Simulated, or device probe failed.
            auto sim = std::make_unique<AcqBoardSim>();
            sim->setMessageCallback([this](AcquisitionBoard::MessageSeverity sev, const std::string &msg) {
                handleBoardMessage(sev, msg);
            });
            if (!sim->detectBoard()) {
                raiseError(QStringLiteral(
                    "No Open Ephys Acquisition Board detected and the simulated "
                    "backend failed to initialise."));
                return false;
            }
            next = std::move(sim);
        }

        if (!next->initializeBoard()) {
            raiseError(QStringLiteral("Failed to initialize the Open Ephys Acquisition Board."));
            return false;
        }
        next->scanPorts();
        m_board = std::move(next);

        LOG_INFO(
            m_log,
            "Open Ephys Acquisition Board ready (board type: {}, headstages: {}).",
            static_cast<int>(m_board->getBoardType()),
            m_board->getHeadstages().size());

        // Rebuild ports against the new backend. Surviving ids keep their
        // subscriptions; the rest are pruned.
        rebuildOutputPorts();

        if (m_settingsDlg) {
            // Sample rate set may differ between backends.
            m_settingsDlg->setAvailableSampleRates(m_board->getAvailableSampleRates());
            m_settingsDlg->setSampleRateHz(m_sampleRateHz);
            m_settingsDlg->setActiveBackendLabel(activeBackendDescription());
            m_settingsDlg->setHeadstageSummary(m_board->getHeadstages());
        }

        return true;
    }

    QString activeBackendDescription() const
    {
        if (!m_board)
            return QStringLiteral("(not initialised)");
        switch (m_board->getBoardType()) {
        case AcquisitionBoard::BoardType::ONI:
            return QStringLiteral("Open Ephys Acquisition Board (ONI)");
        case AcquisitionBoard::BoardType::Simulated:
            return QStringLiteral("Simulated");
        case AcquisitionBoard::BoardType::None:
            break;
        }
        return QStringLiteral("(unknown)");
    }

    /** Snapshot the current output-port id set; used to diff across reconnects. */
    QSet<QString> collectCurrentPortIds() const
    {
        QSet<QString> out;
        out.reserve(static_cast<int>(m_groups.size()));
        for (const auto &g : m_groups)
            out.insert(g.portId);
        return out;
    }

    /**
     * Surface a message coming from the device layer to the user. Info
     * messages of the form "buffer:NN.N%" become live status; warnings open a
     * QMessageBox; errors raise the module error state.
     *
     * Always dispatched onto the GUI thread - the device layer can call
     * this from acquisition threads.
     */
    void handleBoardMessage(AcquisitionBoard::MessageSeverity sev, const std::string &msgIn)
    {
        const QString msg = QString::fromStdString(msgIn);
        QMetaObject::invokeMethod(
            this,
            [this, sev, msg]() {
                if (sev == AcquisitionBoard::MessageSeverity::Info) {
                    if (msg.startsWith(QStringLiteral("buffer:"))) {
                        bool ok = false;
                        const float pct = msg.mid(7).chopped(1).toFloat(&ok);
                        if (ok)
                            updateBufferStatus(pct);
                    } else {
                        setStatusMessage(msg);
                    }
                    return;
                }

                if (sev == AcquisitionBoard::MessageSeverity::Error) {
                    raiseError(msg);
                    return;
                }

                QMessageBox box(QMessageBox::Warning, name(), msg, QMessageBox::Ok, m_settingsDlg);
                box.setTextFormat(Qt::RichText);
                box.setTextInteractionFlags(Qt::TextBrowserInteraction);
                box.exec();
            },
            Qt::QueuedConnection);
    }

    /** Reflect the current ONI on-board buffer fill % in the module status line. */
    void updateBufferStatus(float pct)
    {
        // Soft / hard thresholds: above 50% the buffer is filling; above 75%
        // the host can't keep up and data loss is imminent.
        const QString fmt = pct < 50.0f   ? QStringLiteral("Buffer: %1%")
                            : pct < 75.0f ? QStringLiteral("<html><font color=\"#aa6500\">Buffer:</font> %1%")
                                          : QStringLiteral(
                                                "<html><font color=\"red\">Buffer:</font> %1% <small>"
                                                "<font color=\"red\">— host falling behind</font></small>");
        setStatusMessage(fmt.arg(pct, 0, 'f', 1));
    }

    /**
     * Like registerOutputPort but silently returns the existing port's stream
     * if one was already registered with that ID.
     */
    std::shared_ptr<DataStream<SignalBlockU16>> getOrRegisterOutPort(const QString &id, const QString &title)
    {
        if (auto existing = outPortById(id))
            return existing->stream<SignalBlockU16>();
        return registerOutputPort<SignalBlockU16>(id, title);
    }

    /**
     * Build the per-headstage stream layout for the currently-connected
     * headstages and (re)register an output port for each.
     */
    void rebuildOutputPorts()
    {
        if (!m_board) {
            // No board - drop everything we registered earlier.
            for (const auto &g : m_groups)
                removeOutPortById(g.portId);
            m_groups.clear();
            refreshChannelInventoryFromBoard();
            return;
        }

        // m_disabledChannels may contain ids for channels that aren't currently
        // detected (headstage unplugged, etc.). We deliberately keep those
        // entries so the user's preference survives a replug; they're harmless
        // here because we only consult the set by id.

        const int auxPerHs = m_board->getAuxChannelsPerHeadstage();
        const int adcCount = m_board->getNumAdcChannels();

        // Snapshot the previous port-id set so we can prune ones that
        // disappeared in this rescan. registerOutputPort() already returns
        // the existing port if the id is reused, so live subscriptions on
        // ports that survive don't break.
        QSet<QString> previousIds;
        previousIds.reserve(static_cast<int>(m_groups.size()));
        for (const auto &g : m_groups)
            previousIds.insert(g.portId);

        m_groups.clear();
        QSet<QString> currentIds;

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
                g.rawChannelsPerSample = chanCount;
                g.portId = QStringLiteral("hs-%1").arg(prefixLower);

                for (int c = 0; c < chanCount; ++c) {
                    const auto id = makeChannelId(prefix, ChannelKind::Electrode, c);
                    if (!m_disabledChannels.contains(id)) {
                        g.enabledLocalIndices.push_back(c);
                        g.outChannelIds.append(id);
                        g.channelNames.push_back(hs->getChannelName(c));
                    }
                }
                g.channelsPerSample = static_cast<int>(g.enabledLocalIndices.size());
                if (g.channelsPerSample <= 0)
                    continue; // skip empty port
                g.mutedOutputColumns = std::make_unique<std::atomic_bool[]>(g.channelsPerSample);

                g.stream = getOrRegisterOutPort(
                    g.portId,
                    headstagePortTitle(hs->getStreamPrefix(), g.channelsPerSample));
                g.block = std::make_shared<SignalBlockU16>(0, g.channelsPerSample);
                currentIds.insert(g.portId);
                m_groups.push_back(std::move(g));
            }

            // ---- aux group ----
            {
                GroupStream g;
                g.groupIndex = static_cast<int>(i);
                g.kind = ChannelKind::Aux;
                g.streamPrefix = prefix;
                g.rawChannelsPerSample = auxPerHs;
                g.portId = QStringLiteral("hs-%1-aux").arg(prefixLower);

                for (int c = 0; c < auxPerHs; ++c) {
                    const auto id = makeChannelId(prefix, ChannelKind::Aux, c);
                    if (!m_disabledChannels.contains(id)) {
                        g.enabledLocalIndices.push_back(c);
                        g.outChannelIds.append(id);
                        g.channelNames.push_back(hs->getStreamPrefix() + "_AUX" + std::to_string(c + 1));
                    }
                }
                g.channelsPerSample = static_cast<int>(g.enabledLocalIndices.size());
                if (g.channelsPerSample <= 0)
                    continue;
                g.mutedOutputColumns = std::make_unique<std::atomic_bool[]>(g.channelsPerSample);

                g.stream = getOrRegisterOutPort(g.portId, auxPortTitle(hs->getStreamPrefix(), auxPerHs));
                g.block = std::make_shared<SignalBlockU16>(0, g.channelsPerSample);
                currentIds.insert(g.portId);
                m_groups.push_back(std::move(g));
            }
        }

        // ---- board ADC ----
        {
            GroupStream g;
            g.groupIndex = -1;
            g.kind = ChannelKind::Adc;
            g.rawChannelsPerSample = adcCount;
            g.portId = QStringLiteral("adc");

            for (int c = 0; c < adcCount; ++c) {
                const auto id = makeChannelId(QStringLiteral("ADC"), ChannelKind::Adc, c);
                if (!m_disabledChannels.contains(id)) {
                    g.enabledLocalIndices.push_back(c);
                    g.outChannelIds.append(id);
                    g.channelNames.push_back("ADC" + std::to_string(c + 1));
                }
            }
            g.channelsPerSample = static_cast<int>(g.enabledLocalIndices.size());
            if (g.channelsPerSample > 0) {
                g.mutedOutputColumns = std::make_unique<std::atomic_bool[]>(g.channelsPerSample);
                g.stream = getOrRegisterOutPort(g.portId, adcPortTitle(adcCount));
                g.block = std::make_shared<SignalBlockU16>(0, g.channelsPerSample);
                currentIds.insert(g.portId);
                m_groups.push_back(std::move(g));
            }
        }

        // Drop any ports that existed before this rescan but weren't
        // re-registered in the current pass. Surviving ports kept their
        // identity (registerOutputPort recycles by id), so any subscriptions
        // wired to them are unaffected.
        for (const auto &oldId : previousIds) {
            if (!currentIds.contains(oldId))
                removeOutPortById(oldId);
        }

        refreshChannelInventoryFromBoard();
    }

    /** Push the current detected-channel set into the settings dialog. */
    void refreshChannelInventoryFromBoard()
    {
        if (!m_settingsDlg)
            return;

        std::vector<OeAcqSettingsDialog::ChannelEntry> entries;
        if (!m_board) {
            m_settingsDlg->setChannelInventory(entries);
            return;
        }

        const int auxPerHs = m_board->getAuxChannelsPerHeadstage();
        const int adcCount = m_board->getNumAdcChannels();
        const auto headstages = m_board->getHeadstages();
        for (const auto *hs : headstages) {
            if (!hs->isConnected())
                continue;
            const auto prefix = QString::fromStdString(hs->getStreamPrefix());
            const auto group = tr("Headstage %1").arg(prefix);
            for (int c = 0; c < hs->getNumActiveChannels(); ++c) {
                OeAcqSettingsDialog::ChannelEntry e;
                e.id = makeChannelId(prefix, ChannelKind::Electrode, c);
                e.label = QString::fromStdString(hs->getChannelName(c));
                if (e.label.isEmpty())
                    e.label = QStringLiteral("CH%1").arg(c + 1);
                e.group = group;
                e.enabled = !m_disabledChannels.contains(e.id);
                entries.push_back(std::move(e));
            }
            for (int c = 0; c < auxPerHs; ++c) {
                OeAcqSettingsDialog::ChannelEntry e;
                e.id = makeChannelId(prefix, ChannelKind::Aux, c);
                e.label = QStringLiteral("AUX%1").arg(c + 1);
                e.group = group;
                e.enabled = !m_disabledChannels.contains(e.id);
                entries.push_back(std::move(e));
            }
        }
        const auto adcGroup = tr("Board ADC");
        for (int c = 0; c < adcCount; ++c) {
            OeAcqSettingsDialog::ChannelEntry e;
            e.id = makeChannelId(QStringLiteral("ADC"), ChannelKind::Adc, c);
            e.label = QStringLiteral("ADC%1").arg(c + 1);
            e.group = adcGroup;
            e.enabled = !m_disabledChannels.contains(e.id);
            entries.push_back(std::move(e));
        }
        m_settingsDlg->setChannelInventory(entries);
    }

    /**
     * Toggle the settings dialog's run-locked state. Safe to call from any
     * thread - the actual widget update is dispatched onto the GUI thread.
     */
    void setSettingsRunActive(bool active)
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
            g.stream->setMetadataValue("data_unit", std::string{"µV"});
            g.stream->setMetadataValue("data_scale", static_cast<double>(m_board->getBitVolts(g.kind)));
            g.stream->setMetadataValue("data_offset", 0.0);
            g.stream->setMetadataValue("signal_names", namesMeta);
        }
    }

    std::unique_ptr<AcquisitionBoard> m_board;
    std::vector<GroupStream> m_groups;
    std::unique_ptr<FreqCounterSynchronizer> m_clockSync;
    OeAcqSettingsDialog *m_settingsDlg = nullptr;

    std::shared_ptr<StreamInputPort<LineCommand>> m_ttlIn;
    std::shared_ptr<StreamSubscription<LineCommand>> m_ttlSub;

    int m_sampleRateHz = 30000;
    bool m_acquireAux = false;
    bool m_acquireAdc = false;
    ChannelNamingScheme m_namingScheme{GLOBAL_INDEX};
    OeAcqSettingsDialog::BackendChoice m_backendChoice{OeAcqSettingsDialog::BackendAuto};

    // Channels the user has explicitly turned off.
    // Entries for currently-unplugged channels are kept on purpose:
    // the preference persists across replug.
    QSet<QString> m_disabledChannels;
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
    return new OpenEphysAcqModule(this, parent);
}

#include "oeacqmodule.moc"
