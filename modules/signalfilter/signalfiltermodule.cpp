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

enum class SignalKind {
    None,
    Float,
    Int,
    UInt16
};

static SignalKind signalKindFromTypeId(int typeId)
{
    if (typeId == SignalBlockF32::staticTypeId())
        return SignalKind::Float;
    if (typeId == SignalBlockI32::staticTypeId())
        return SignalKind::Int;
    if (typeId == SignalBlockU16::staticTypeId())
        return SignalKind::UInt16;
    return SignalKind::None;
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

class SignalFilterModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<StreamInputPort<SignalBlockF32>> m_floatIn;
    std::shared_ptr<StreamInputPort<SignalBlockI32>> m_intIn;
    std::shared_ptr<StreamInputPort<SignalBlockU16>> m_uint16In;

    std::shared_ptr<DataStream<SignalBlockF32>> m_floatOut;
    std::shared_ptr<DataStream<SignalBlockI32>> m_intOut;
    std::shared_ptr<DataStream<SignalBlockU16>> m_uint16Out;

    std::shared_ptr<StreamSubscription<SignalBlockF32>> m_floatSub;
    std::shared_ptr<StreamSubscription<SignalBlockI32>> m_intSub;
    std::shared_ptr<StreamSubscription<SignalBlockU16>> m_uint16Sub;

    SignalFilterSettingsDialog *m_settingsDlg;
    FilterPipeline m_pipeline;

    SignalKind m_kind;
    double m_sampleRate;
    bool m_useAllChannels;
    std::set<int> m_selectedChannels;

public:
    explicit SignalFilterModule(SignalFilterModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_kind(SignalKind::None),
          m_sampleRate(-1.0),
          m_useAllChannels(true)
    {
        m_settingsDlg = new SignalFilterSettingsDialog();
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &SignalFilterSettingsDialog::settingsChanged, this, [this]() {
            updatePortConfiguration();
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

    bool initialize() override
    {
        updatePortConfiguration();
        return true;
    }

    void updatePortConfiguration()
    {
        // Rebuild the single input/output port pair to match the selected type.
        // Only safe on the main thread while not running.
        clearInPorts();
        clearOutPorts();
        m_floatIn.reset();
        m_intIn.reset();
        m_uint16In.reset();
        m_floatOut.reset();
        m_intOut.reset();
        m_uint16Out.reset();

        setStatusMessage({});
        switch (signalKindFromTypeId(m_settingsDlg->selectedTypeId())) {
        case SignalKind::Float:
            m_floatIn = registerInputPort<SignalBlockF32>(QStringLiteral("signals-in"), QStringLiteral("F32 Source"));
            m_floatOut = registerOutputPort<SignalBlockF32>(
                QStringLiteral("signals-out"),
                QStringLiteral("F32 Filtered"));
            break;
        case SignalKind::Int:
            m_intIn = registerInputPort<SignalBlockI32>(QStringLiteral("signals-in"), QStringLiteral("I32 Source"));
            m_intOut = registerOutputPort<SignalBlockI32>(
                QStringLiteral("signals-out"),
                QStringLiteral("I32 Filtered"));
            break;
        case SignalKind::UInt16:
            m_uint16In = registerInputPort<SignalBlockU16>(QStringLiteral("signals-in"), QStringLiteral("U16 Source"));
            m_uint16Out = registerOutputPort<SignalBlockU16>(
                QStringLiteral("signals-out"),
                QStringLiteral("U16 Filtered"));
            break;
        case SignalKind::None:
            setStatusMessage(QStringLiteral("No input signal type selected!"));
            break;
        }
    }

    bool prepare(const TestSubject &) override
    {
        clearDataReceivedEventRegistrations();
        m_kind = SignalKind::None;
        m_settingsDlg->setRunning(true);

        // configure the pipeline from the current settings; channel count is
        // discovered from the first data block, so the actual build is lazy
        const auto stages = m_settingsDlg->stages();

        // validate the filter design up front so a misconfiguration is reported
        // immediately instead of silently producing a pass-through run
        for (size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].family == FilterFamily::CustomSOS && !stages[i].sosValid) {
                raiseError(QStringLiteral(
                               "Filter stage %1 is a Custom (SOS) filter with invalid or "
                               "missing coefficients. Enter second-order-sections (6 numbers "
                               "per line: b0 b1 b2 a0 a1 a2) in the settings, or remove the stage.")
                               .arg(i + 1));
                return false;
            }
        }

        m_pipeline.setStages(stages);
        m_useAllChannels = m_settingsDlg->useAllChannels();
        m_selectedChannels = m_useAllChannels ? std::set<int>{}
                                              : parseChannelRanges(m_settingsDlg->channelSelectionText());

        if (!m_useAllChannels && m_selectedChannels.empty())
            LOG_WARNING(m_log, "Channel selection is empty; all channels will pass through unfiltered");

        m_floatSub.reset();
        m_intSub.reset();
        m_uint16Sub.reset();

        if (m_floatIn && m_floatIn->hasSubscription()) {
            m_floatSub = m_floatIn->subscription();
            m_kind = SignalKind::Float;
            if (!resolveSampleRate(m_floatSub->metadataValue("sample_rate", -1.0)))
                return false;
            m_floatOut->setMetadata(m_floatSub->metadata());
            m_floatOut->start();
            registerDataReceivedEvent(&SignalFilterModule::onFloatReceived, m_floatSub);
        } else if (m_intIn && m_intIn->hasSubscription()) {
            m_intSub = m_intIn->subscription();
            m_kind = SignalKind::Int;
            if (!resolveSampleRate(m_intSub->metadataValue("sample_rate", -1.0)))
                return false;
            m_intOut->setMetadata(m_intSub->metadata());
            m_intOut->start();
            registerDataReceivedEvent(&SignalFilterModule::onIntReceived, m_intSub);
        } else if (m_uint16In && m_uint16In->hasSubscription()) {
            m_uint16Sub = m_uint16In->subscription();
            m_kind = SignalKind::UInt16;
            if (!resolveSampleRate(m_uint16Sub->metadataValue("sample_rate", -1.0)))
                return false;
            m_uint16Out->setMetadata(m_uint16Sub->metadata());
            m_uint16Out->start();
            registerDataReceivedEvent(&SignalFilterModule::onUInt16Received, m_uint16Sub);
        }

        if (m_kind == SignalKind::None) {
            // nothing connected: nothing to do this run
            setStateDormant();
            return true;
        }

        setStateReady();
        return true;
    }

    void stop() override
    {
        m_settingsDlg->setRunning(false);
    }

    void onFloatReceived()
    {
        processBlocks(m_floatSub, m_floatOut);
    }

    void onIntReceived()
    {
        processBlocks(m_intSub, m_intOut);
    }

    void onUInt16Received()
    {
        processBlocks(m_uint16Sub, m_uint16Out);
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
    bool resolveSampleRate(double sampleRate)
    {
        m_sampleRate = sampleRate;
        m_pipeline.setSampleRate(m_sampleRate);

        if (m_pipeline.needsSampleRate() && !(m_sampleRate > 0.0)) {
            raiseError(QStringLiteral(
                "The input signal stream does not advertise a \"sample_rate\", which is required "
                "for the configured frequency-based filters. Either connect a source that provides "
                "it, or use only Custom (SOS) filter stages."));
            return false;
        }

        return true;
    }

    template<typename SubT, typename OutT>
    void processBlocks(SubT &sub, OutT &out)
    {
        if (!sub || !out)
            return;

        // Drain everything queued to keep the subscription from backing up.
        while (true) {
            auto maybe = sub->peekNext();
            if (!maybe.has_value())
                break;
            auto block = std::move(*maybe);
            filterBlock(block);
            out->push(std::move(block));
        }
    }

    template<typename BlockT>
    void filterBlock(BlockT &block)
    {
        using Scalar = typename std::remove_reference_t<decltype(block.data)>::Scalar;

        const int nRows = static_cast<int>(block.data.rows());
        const int nCols = static_cast<int>(block.data.cols());
        if (nCols <= 0 || nRows <= 0)
            return;

        // (Re)build per-channel filters when the channel count changes.
        if (!m_pipeline.isBuiltFor(nCols)) {
            std::vector<bool> mask;
            if (!m_useAllChannels) {
                mask.assign(static_cast<size_t>(nCols), false);
                for (int ch : m_selectedChannels)
                    if (ch < nCols)
                        mask[static_cast<size_t>(ch)] = true;
            }
            std::string err;
            if (!m_pipeline.build(nCols, mask, &err)) {
                raiseError(QStringLiteral("Failed to construct filter: %1").arg(QString::fromStdString(err)));
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
                const double y = m_pipeline.processSample(c, static_cast<double>(block.data(r, c)));
                if constexpr (isFloat) {
                    block.data(r, c) = static_cast<Scalar>(y);
                } else {
                    const double clamped = std::clamp(std::nearbyint(y), scalarMin, scalarMax);
                    block.data(r, c) = static_cast<Scalar>(clamped);
                }
            }
        }
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
