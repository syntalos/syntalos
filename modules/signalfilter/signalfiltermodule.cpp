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

#include "signalfiltermodule.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <type_traits>

#include "datactl/datatypes.h"
#include "filterpipeline.h"
#include "signalfiltersettingsdialog.h"

SYNTALOS_MODULE(SignalFilterModule)

/**
 * @brief Short sample-type tag ("F32", "I16", ...) for a signal block type ID, empty if not a signal block.
 */
static QString signalTagForTypeId(int typeId)
{
    QString tag;
    withSignalBlockType(typeId, [&](auto t) {
        using T = typename decltype(t)::type;
        tag = QString::fromStdString(scalarTypeName<signal_block_scalar_t<T>>(true)).toUpper();
    });
    return tag;
}

/**
 * @brief Parse a channel-range string like "0-15, 20, 24-31" into indices.
 * @return the set of zero-based channel indices; malformed tokens are skipped.
 */
static std::set<int> parseChannelRanges(const QString &text)
{
    std::set<int> indices;
    const auto tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto &tokRaw : tokens) {
        const QString tok = tokRaw.trimmed();
        if (tok.isEmpty())
            continue;
        const int dash = tok.indexOf(QLatin1Char('-'));
        if (dash > 0) {
            bool ok1 = false, ok2 = false;
            const int lo = tok.left(dash).trimmed().toInt(&ok1);
            const int hi = tok.mid(dash + 1).trimmed().toInt(&ok2);
            if (ok1 && ok2 && lo >= 0 && hi >= lo)
                for (int i = lo; i <= hi; ++i)
                    indices.insert(i);
        } else {
            bool ok = false;
            const int v = tok.toInt(&ok);
            if (ok && v >= 0)
                indices.insert(v);
        }
    }

    return indices;
}

/**
 * Validate a stage list against the running input: SOS coefficients, sample
 * rate, and frequency ranges. Rejecting out-of-range frequencies here is what
 * lets transient values (e.g. a momentary 0 Hz while the user edits a field)
 * be ignored instead of crashing filter construction and stopping the run.
 */
static bool validateStagesLive(const std::vector<FilterStage> &stages, double sampleRate, QString *err)
{
    const auto fail = [err](const QString &msg) {
        if (err)
            *err = msg;
        return false;
    };

    for (size_t i = 0; i < stages.size(); ++i) {
        const auto &s = stages[i];
        const int n = static_cast<int>(i + 1);

        if (s.family == FilterFamily::CustomSOS) {
            if (!s.sosValid)
                return fail(QStringLiteral("stage %1 has invalid Custom (SOS) coefficients").arg(n));
            continue;
        }

        // every other family is frequency-based and needs a valid sample rate
        if (!(sampleRate > 0.0))
            return fail(QStringLiteral("stage %1 needs a sample rate the input does not provide").arg(n));

        const double nyquist = sampleRate / 2.0;

        // cutoff / centre must sit strictly inside (0, Nyquist)
        if (!(s.freq1 > 0.0 && s.freq1 < nyquist))
            return fail(
                QStringLiteral("stage %1 frequency must be between 0 and %2 Hz").arg(n).arg(nyquist, 0, 'g', 4));

        // band filters additionally need a positive width that keeps the band in range
        const bool isPole = s.family == FilterFamily::Butterworth || s.family == FilterFamily::ChebyshevI
                            || s.family == FilterFamily::ChebyshevII;
        const bool isBand = isPole
                            && (s.response == FilterResponse::BandPass || s.response == FilterResponse::BandStop);
        if (isBand && !(s.freq2 > 0.0 && s.freq1 - s.freq2 / 2.0 > 0.0 && s.freq1 + s.freq2 / 2.0 < nyquist))
            return fail(QStringLiteral("stage %1 band (centre %2 Hz, width %3 Hz) is out of range")
                            .arg(n)
                            .arg(s.freq1)
                            .arg(s.freq2));
    }
    return true;
}

/**
 * Settings that can be changed while a run is in progress.
 * The revisions tell the worker which part of the settings was changed since it last looked.
 */
struct LiveFilterSettings {
    std::vector<FilterStage> stages;
    uint64_t stagesRev = 0;

    bool useAllChannels = true;
    std::set<int> channels;
    uint64_t maskRev = 0;
};

class SignalFilterModule : public AbstractModule
{
    Q_OBJECT

private:
    // One input/output port pair of the signal block type the user selected;
    // the concrete type is only dispatched on when data arrives.
    std::shared_ptr<VarStreamInputPort> m_inPort;
    std::shared_ptr<VariantDataStream> m_out;

    SignalFilterSettingsDialog *m_settingsDlg;

    // Live reconfiguration. The GUI thread deposits the desired channel/stage
    // config here; the worker applies it at a block boundary.
    LiveValue<LiveFilterSettings> m_liveSettings;

public:
    explicit SignalFilterModule(SignalFilterModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_settingsDlg = new SignalFilterSettingsDialog();
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &SignalFilterSettingsDialog::settingsChanged, this, [this]() {
            updatePortConfiguration();
        });

        // Live changes during a run: deposit the new config; the worker
        // picks it up at the next block boundary.
        connect(m_settingsDlg, &SignalFilterSettingsDialog::channelsChanged, this, [this]() {
            queueLiveChannelUpdate();
        });
        connect(m_settingsDlg, &SignalFilterSettingsDialog::stagesChanged, this, [this]() {
            queueLiveStageUpdate();
        });
    }

    ~SignalFilterModule() override = default;

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    int eventsMaxModulesPerThread() const override
    {
        return 8;
    }

    std::expected<void, QString> initialize() override
    {
        updatePortConfiguration();
        return {};
    }

    void updatePortConfiguration()
    {
        // Rebuild the single input/output port pair to match the selected type.
        // Only safe on the main thread while not running.
        clearInPorts();
        clearOutPorts();
        m_inPort.reset();
        m_out.reset();

        setStatusMessage({});
        const int typeId = m_settingsDlg->selectedTypeId();
        const auto tag = signalTagForTypeId(typeId);
        if (tag.isEmpty()) {
            setStatusMessage(QStringLiteral("No input signal type selected!"));
            return;
        }
        m_inPort = registerInputPortByTypeId(typeId, QStringLiteral("signals-in"), tag + QStringLiteral(" Source"));
        m_out = registerOutputPortByTypeId(typeId, QStringLiteral("signals-out"), tag + QStringLiteral(" Filtered"));
    }

    bool prepare(const RunInfo &) override
    {
        m_settingsDlg->setRunning(true);

        // configure the pipeline from the current settings; channel count is
        // discovered from the first data block, so the actual build is lazy
        const auto stages = m_settingsDlg->stages();
        FilterPipeline pipeline;
        pipeline.setStages(stages);
        const bool useAllChannels = m_settingsDlg->useAllChannels();
        const auto selectedChannels = useAllChannels ? std::set<int>{}
                                                     : parseChannelRanges(m_settingsDlg->channelSelectionText());

        if (!useAllChannels && selectedChannels.empty())
            LOG_WARNING(m_log, "Channel selection is empty; all channels will pass through unfiltered");

        if (!m_inPort || !m_out || !m_inPort->hasSubscription()) {
            // nothing connected: nothing to do this run
            setStateDormant();
            return true;
        }

        const auto sub = m_inPort->subscriptionVar();
        const double sampleRate = sub->metadataValue("sample_rate", -1.0);
        if (!resolveSampleRate(pipeline, sampleRate))
            return false;
        m_out->setMetadata(updateOutputMetadata(sub->metadata(), useAllChannels, selectedChannels));
        m_out->start();

        // now that the sample rate is known, validate the whole filter design
        QString verr;
        if (!validateStagesLive(stages, sampleRate, &verr)) {
            raiseError(QStringLiteral("Filter configuration is invalid: %1.").arg(verr));
            return false;
        }

        // start from the current settings, discarding any live updates queued before this run started
        m_liveSettings = LiveValue<LiveFilterSettings>(LiveFilterSettings{
            .stages = stages,
            .useAllChannels = useAllChannels,
            .channels = selectedChannels,
        });

        setWorker(
            Worker{
                .sub = sub,
                .out = m_out,
                .pipeline = std::move(pipeline),
                .sampleRate = sampleRate,
                .useAllChannels = useAllChannels,
                .selectedChannels = selectedChannels,
                .liveSettings = m_liveSettings,
            });

        setStateReady();
        return true;
    }

    /**
     * Filters the incoming signal blocks, in the event loop the module is assigned to.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<VariantStreamSubscription> sub;
        std::shared_ptr<VariantDataStream> out;

        FilterPipeline pipeline;
        double sampleRate;
        bool useAllChannels;
        std::set<int> selectedChannels;

        // Live reconfiguration. The GUI thread deposits the desired channel/stage
        // config here; we apply it at a block boundary. The per-sample hot path
        // never locks — it only reads the already-applied pipeline state.
        LiveValue<LiveFilterSettings> liveSettings;
        uint64_t appliedStagesRev = 0;
        uint64_t appliedMaskRev = 0;

        void setup(WorkerEvents &ev)
        {
            ev.onData(sub, [this] {
                onDataReceived();
            });
        }

        void onDataReceived()
        {
            // Apply any GUI-driven channel/filter changes at this block boundary.
            applyLiveUpdates();

            // Drain everything queued to keep the subscription from backing up.
            const ProcessVarFn processItem = [this](BaseDataType &data) {
                visitSignalBlock(data, [this](auto &block) {
                    using T = std::remove_cvref_t<decltype(block)>;
                    filterBlock(block);
                    // the output port was registered with the very same type as the input
                    std::static_pointer_cast<DataStream<T>>(out)->push(std::move(block));
                });
            };
            while (sub->callIfNextVar(processItem)) {
            }
        }

        void applyLiveUpdates()
        {
            const auto maybeSettings = liveSettings.takeIfChanged();
            if (!maybeSettings.has_value())
                return;
            const auto &ls = *maybeSettings;

            if (ls.stagesRev != appliedStagesRev) {
                appliedStagesRev = ls.stagesRev;

                QString err;
                if (validateStagesLive(ls.stages, sampleRate, &err)) {
                    pipeline.setStages(ls.stages); // forces a rebuild on the next block
                } else {
                    // Keep the previous (valid) filter running; the dialog already
                    // flags the problem inline, so just note it in the log.
                    LOG_WARNING(mod.log, "Ignoring live filter change: {}", err.toStdString());
                }
            }

            if (ls.maskRev != appliedMaskRev) {
                appliedMaskRev = ls.maskRev;

                useAllChannels = ls.useAllChannels;
                selectedChannels = ls.channels;
                const int nc = pipeline.channelCount();
                if (nc > 0)
                    pipeline.setChannelMask(maskFor(nc));
            }
        }

        /**
         * Build a channel mask for @p nCols from the current selection (empty = all).
         */
        std::vector<bool> maskFor(int nCols) const
        {
            std::vector<bool> mask;
            if (!useAllChannels) {
                mask.assign(static_cast<size_t>(nCols), false);
                for (int ch : selectedChannels)
                    if (ch < nCols)
                        mask[static_cast<size_t>(ch)] = true;
            }
            return mask;
        }

        template<SignalBlockType BlockT>
        void filterBlock(BlockT &block)
        {
            using Scalar = signal_block_scalar_t<BlockT>;

            const int nRows = static_cast<int>(block.data.rows());
            const int nCols = static_cast<int>(block.data.cols());
            if (nCols <= 0 || nRows <= 0)
                return;

            // (Re)build per-channel filters when the channel count changes.
            if (!pipeline.isBuiltFor(nCols)) {
                std::string err;
                if (!pipeline.build(nCols, maskFor(nCols), &err)) {
                    mod.raiseError(std::format("Failed to construct filter: {}", err));
                    return;
                }
            }

            constexpr bool isFloat = std::is_floating_point_v<Scalar>;
            constexpr double scalarMin = static_cast<double>(std::numeric_limits<Scalar>::lowest());
            constexpr double scalarMax = static_cast<double>(std::numeric_limits<Scalar>::max());

            // rows = samples, cols = channels (row-major). Filter each channel
            // through its own state, advancing sample by sample (row by row).
            for (int c = 0; c < nCols; ++c) {
                for (int r = 0; r < nRows; ++r) {
                    const double y = pipeline.processSample(c, static_cast<double>(block.data(r, c)));
                    if constexpr (isFloat) {
                        block.data(r, c) = static_cast<Scalar>(y);
                    } else {
                        // Frequency filters remove the DC component, so the output swings around
                        // zero. On unsigned types (U16) the negative half clamps to the type
                        // minimum: filtering raw integer/unsigned DAQ samples is inherently lossy
                        // and callers should prefer F32. The clamp keeps it safe (no wraparound).
                        const double clamped = std::clamp(std::nearbyint(y), scalarMin, scalarMax);
                        block.data(r, c) = static_cast<Scalar>(clamped);
                    }
                }
            }
        }
    };

    void stop() override
    {
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("input_type", m_settingsDlg->selectedTypeName());
        settings.insert("use_all_channels", m_settingsDlg->useAllChannels());
        settings.insert("channel_selection", m_settingsDlg->channelSelectionText());

        QVariantList stagesList;
        for (const auto &st : m_settingsDlg->stages())
            stagesList.append(stageToVariant(st));
        settings.insert("stages", stagesList);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setSelectedTypeName(settings.value("input_type", QStringLiteral("SignalBlockF32")).toString());
        updatePortConfiguration();

        m_settingsDlg->setChannelSelection(
            settings.value("use_all_channels", true).toBool(),
            settings.value("channel_selection").toString());

        std::vector<FilterStage> stages;
        for (const auto &v : settings.value("stages").toList())
            stages.push_back(stageFromVariant(v.toHash()));
        m_settingsDlg->setStages(stages);

        return true;
    }

private:
    bool resolveSampleRate(FilterPipeline &pipeline, double sampleRate)
    {
        pipeline.setSampleRate(sampleRate);

        if (pipeline.needsSampleRate() && !(sampleRate > 0.0)) {
            raiseError(QStringLiteral(
                "The input signal stream does not advertise a \"sample_rate\", which is required "
                "for the configured frequency-based filters. Either connect a source that provides "
                "it, or use only Custom (SOS) filter stages."));
            return false;
        }

        return true;
    }

    /**
     * Derive the outgoing stream metadata from the source metadata, marking what
     * the module filtered: channels selected for filtering at the start of the
     * run get their "signal_names" entry suffixed with "_flt", and the dataset
     * name proposal gets a "-filtered" suffix. The metadata is fixed when the
     * stream starts, so live filter/channel changes during a run are not
     * reflected here.
     */
    MetaStringMap updateOutputMetadata(
        const MetaStringMap &srcMeta,
        bool useAllChannels,
        const std::set<int> &selectedChannels) const
    {
        MetaStringMap meta = srcMeta;

        // suffix the names of channels that are being filtered
        if (const auto namesV = meta.value("signal_names"); namesV.has_value()) {
            if (const auto arr = namesV->get<MetaArray>()) {
                MetaArray names = *arr;
                for (size_t ch = 0; ch < names.size(); ++ch) {
                    const bool filtered = useAllChannels || selectedChannels.contains(static_cast<int>(ch));
                    if (!filtered)
                        continue;
                    if (const auto name = names[ch].get<std::string>())
                        names[ch] = *name + "_flt";
                }
                meta.insert("signal_names", names);
            }
        }

        // Mark the recorded dataset as filtered.
        const auto ProposedDataNameKey = CommonMetadataKeyMap->value(CommonMetadataKey::DataNameProposal);
        std::string proposal = meta.valueOr<std::string>(ProposedDataNameKey, std::string{});
        if (proposal.empty()) {
            const auto srcName = meta.valueOr<std::string>(
                CommonMetadataKeyMap->value(CommonMetadataKey::SrcModName),
                std::string{});
            if (!srcName.empty())
                proposal = srcName + "/data";
        }
        if (!proposal.empty()) {
            const auto slash = proposal.find('/');
            if (slash == std::string::npos) {
                proposal += "-flt";
            } else {
                proposal.insert(slash, "-flt");
                proposal += "-filtered";
            }
            meta[ProposedDataNameKey] = proposal;
        }

        return meta;
    }

    // --- live reconfiguration: GUI thread deposits, worker applies ---

    void queueLiveChannelUpdate()
    {
        if (!m_running)
            return; // not running: prepare() reads the dialog directly
        auto ls = m_liveSettings.get();
        ls.useAllChannels = m_settingsDlg->useAllChannels();
        ls.channels = parseChannelRanges(m_settingsDlg->channelSelectionText());
        ls.maskRev++;
        m_liveSettings.set(std::move(ls));
    }

    void queueLiveStageUpdate()
    {
        if (!m_running)
            return;
        auto ls = m_liveSettings.get();
        ls.stages = m_settingsDlg->stages();
        ls.stagesRev++;
        m_liveSettings.set(std::move(ls));
    }

    static QVariantHash stageToVariant(const FilterStage &st)
    {
        QVariantHash h;
        h.insert("family", static_cast<int>(st.family));
        h.insert("response", static_cast<int>(st.response));
        h.insert("order", st.order);
        h.insert("freq1", st.freq1);
        h.insert("freq2", st.freq2);
        h.insert("ripple_db", st.rippleDb);
        h.insert("stopband_db", st.stopbandDb);
        h.insert("q_factor", st.qFactor);

        // Store the second-order-sections as a flat list of doubles (6 per
        // section). A nested array-of-arrays does not survive the settings
        // round-trip: QVariantList::append(QVariantList) hits QList's list
        // concatenation overload and silently flattens it anyway, after which
        // it can no longer be read back.
        QVariantList sosFlat;
        for (const auto &row : st.sos)
            for (double v : row)
                sosFlat.append(v);
        h.insert("sos", sosFlat);
        return h;
    }

    static FilterStage stageFromVariant(const QVariantHash &h)
    {
        FilterStage st;
        st.family = static_cast<FilterFamily>(h.value("family", 0).toInt());
        st.response = static_cast<FilterResponse>(h.value("response", 1).toInt());
        st.order = h.value("order", 2).toInt();
        st.freq1 = h.value("freq1", 300.0).toDouble();
        st.freq2 = h.value("freq2", 10.0).toDouble();
        st.rippleDb = h.value("ripple_db", 1.0).toDouble();
        st.stopbandDb = h.value("stopband_db", 60.0).toDouble();
        st.qFactor = h.value("q_factor", 20.0).toDouble();

        st.sos.clear();
        const auto sosList = h.value("sos").toList();
        // Flat layout: consecutive groups of 6 doubles, one per section.
        for (int i = 0; i + 6 <= sosList.size(); i += 6) {
            std::array<double, 6> row{};
            for (int j = 0; j < 6; ++j)
                row[static_cast<size_t>(j)] = sosList[i + j].toDouble();
            st.sos.push_back(row);
        }

        // A loaded Custom (SOS) stage is valid if it carries coefficients; only
        // valid coefficients are ever persisted, so this can't resurrect garbage.
        st.sosValid = (st.family != FilterFamily::CustomSOS) || !st.sos.empty();
        return st;
    }
};

QString SignalFilterModuleInfo::id() const
{
    return QStringLiteral("signalfilter");
}

QString SignalFilterModuleInfo::name() const
{
    return QStringLiteral("Signal Filter");
}

QString SignalFilterModuleInfo::description() const
{
    return QStringLiteral("Apply a chain of IIR filters (high-/low-pass, band-pass, notch, custom) to signal channels");
}

ModuleCategories SignalFilterModuleInfo::categories() const
{
    return ModuleCategory::PROCESSING;
}

AbstractModule *SignalFilterModuleInfo::createModule(QObject *parent)
{
    return new SignalFilterModule(this, parent);
}

#include "signalfiltermodule.moc"
