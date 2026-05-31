/*
 * Copyright (C) 2023-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "latencycheckmodule.h"

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QWidget>
#include <unordered_map>

#include "latencycanvas.h"

SYNTALOS_MODULE(LatencyCheckModule)

/**
 * @brief Measurement mode
 */
enum class LatencyMode {
    DualLine = 0,  /// latency between a pulse on line A and the following pulse on line B
    SingleLine = 1 /// interval between consecutive pulses on line A only
};

/**
 * @brief The line transition to trigger a measurement
 */
enum class TriggerEdge {
    Rising = 0,  /// 0 -> non-zero
    Falling = 1, /// non-zero -> 0
    Both = 2     /// any change
};

static bool isQualifyingEdge(uint32_t last, uint32_t cur, TriggerEdge edge)
{
    switch (edge) {
    case TriggerEdge::Rising:
        return last == 0 && cur != 0;
    case TriggerEdge::Falling:
        return last != 0 && cur == 0;
    case TriggerEdge::Both:
        return last != cur;
    }
    return false;
}

/**
 * @brief Settings UI for the latency-check module.
 */
class LCSettingsDialog : public QDialog
{
public:
    explicit LCSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Latency Check Settings"));

        auto layout = new QFormLayout(this);

        m_modeCombo = new QComboBox(this);
        m_modeCombo->addItem(QStringLiteral("Dual line (latency between A → B)"));
        m_modeCombo->addItem(QStringLiteral("Single line (interval between pulses on A)"));
        layout->addRow(QStringLiteral("Mode"), m_modeCombo);

        m_edgeCombo = new QComboBox(this);
        m_edgeCombo->addItem(QStringLiteral("Rising (0 → 1)"));
        m_edgeCombo->addItem(QStringLiteral("Falling (1 → 0)"));
        m_edgeCombo->addItem(QStringLiteral("Both edges"));
        layout->addRow(QStringLiteral("Trigger edge"), m_edgeCombo);

        m_ackLineSpin = new QSpinBox(this);
        m_ackLineSpin->setRange(0, 65535);
        m_ackLineSpin->setValue(8);
        layout->addRow(QStringLiteral("Acknowledgement line"), m_ackLineSpin);

        m_ackPulseSpin = new QSpinBox(this);
        m_ackPulseSpin->setRange(1, 10000);
        m_ackPulseSpin->setValue(25);
        m_ackPulseSpin->setSuffix(QStringLiteral(" ms"));
        layout->addRow(QStringLiteral("Acknowledgement pulse duration"), m_ackPulseSpin);
    }

    LatencyMode mode() const
    {
        return static_cast<LatencyMode>(m_modeCombo->currentIndex());
    }

    void setMode(LatencyMode mode)
    {
        m_modeCombo->setCurrentIndex(static_cast<int>(mode));
    }

    TriggerEdge edge() const
    {
        return static_cast<TriggerEdge>(m_edgeCombo->currentIndex());
    }

    void setEdge(TriggerEdge edge)
    {
        m_edgeCombo->setCurrentIndex(static_cast<int>(edge));
    }

    uint16_t ackLine() const
    {
        return static_cast<uint16_t>(m_ackLineSpin->value());
    }

    void setAckLine(uint16_t line)
    {
        m_ackLineSpin->setValue(line);
    }

    int ackPulseMs() const
    {
        return m_ackPulseSpin->value();
    }
    void setAckPulseMs(int ms)
    {
        m_ackPulseSpin->setValue(ms);
    }

private:
    QComboBox *m_modeCombo;
    QComboBox *m_edgeCombo;
    QSpinBox *m_ackLineSpin;
    QSpinBox *m_ackPulseSpin;
};

class LatencyCheckModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<LineReading>> m_lrAInPort;
    std::shared_ptr<StreamInputPort<LineReading>> m_lrBInPort;
    std::shared_ptr<StreamSubscription<LineReading>> m_subA;
    std::shared_ptr<StreamSubscription<LineReading>> m_subB;

    std::shared_ptr<DataStream<LineCommand>> m_lcStream;
    std::shared_ptr<DataStream<SignalBlockI32>> m_latStream;

    LCSettingsDialog *m_settingsDlg;
    LatencyCanvas *m_canvas;

    // Runtime configuration, captured in prepare() so the event handlers
    // (which may run on a different thread) read stable values.
    LatencyMode m_mode;
    TriggerEdge m_edge;
    uint16_t m_ackLine;
    int m_ackPulseMs;

    std::unordered_map<uint16_t, uint32_t> m_lastValueA;
    std::unordered_map<uint16_t, uint32_t> m_lastValueB;
    microseconds_t m_pendingTimeA{}; // last A edge awaiting a B edge (dual mode)
    bool m_havePendingA;
    microseconds_t m_prevTimeA{}; // previous A edge (single-line interval mode)
    bool m_havePrevA;

public:
    explicit LatencyCheckModule(const ModuleInfo *info = nullptr, QObject *parent = nullptr)
        : AbstractModule(info, parent)
    {
        m_lrAInPort = registerInputPort<LineReading>(QStringLiteral("line-a-in"), QStringLiteral("Line A"));
        m_lrBInPort = registerInputPort<LineReading>(QStringLiteral("line-b-in"), QStringLiteral("Line B"));
        m_lcStream = registerOutputPort<LineCommand>(QStringLiteral("line-out"), QStringLiteral("Line Control"));
        m_latStream = registerOutputPort<SignalBlockI32>(QStringLiteral("latencies-out"), QStringLiteral("Latencies"));

        m_canvas = new LatencyCanvas;
        m_canvas->setWindowIcon(info->icon());
        addDisplayWindow(m_canvas);

        m_settingsDlg = new LCSettingsDialog;
        m_settingsDlg->setWindowIcon(info->icon());
        addSettingsWindow(m_settingsDlg);
    }

    ~LatencyCheckModule() override {}

    ModuleDriverKind driver() const final
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    ModuleFeatures features() const final
    {
        return ModuleFeature::SHOW_DISPLAY | ModuleFeature::SHOW_SETTINGS;
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_canvas->setWindowTitle(name);
    }

    bool prepare(const TestSubject &) final
    {
        // Capture settings for the duration of this run
        m_mode = m_settingsDlg->mode();
        m_edge = m_settingsDlg->edge();
        m_ackLine = m_settingsDlg->ackLine();
        m_ackPulseMs = m_settingsDlg->ackPulseMs();

        // Reset measurement state
        m_subA.reset();
        m_subB.reset();
        m_lastValueA.clear();
        m_lastValueB.clear();
        m_havePendingA = false;
        m_havePrevA = false;
        m_canvas->clearRuntimeData();

        // Configure the latency output stream
        m_latStream->setMetadataValue("signal_names", MetaArray{"Latency"});
        m_latStream->setMetadataValue("time_unit", "microseconds");
        m_latStream->setMetadataValue("data_unit", "µs");
        m_latStream->setSuggestedDataName(QStringLiteral("%1/values").arg(datasetNameSuggestion()));
        m_latStream->start();

        m_lcStream->start();

        // Line A is required in both modes
        if (!m_lrAInPort->hasSubscription()) {
            setStateDormant();
            return true;
        }
        m_subA = m_lrAInPort->subscription();
        registerDataReceivedEvent(
            [this]() {
                processLineA();
            },
            m_lrAInPort->subscriptionVar());

        if (m_mode == LatencyMode::DualLine) {
            if (!m_lrBInPort->hasSubscription()) {
                raiseError(QStringLiteral(
                    "Dual-line mode is selected, but the \"Line B\" input is not connected. "
                    "Connect a second line or switch to single-line mode in the settings."));
                return false;
            }
            m_subB = m_lrBInPort->subscription();
            registerDataReceivedEvent(
                [this]() {
                    processLineB();
                },
                m_lrBInPort->subscriptionVar());
        } else if (m_lrBInPort->hasSubscription()) {
            raiseError(QStringLiteral(
                "Single-line mode is selected, but the \"Line B\" input is connected. "
                "Disconnect Line B or switch to dual-line mode in the settings."));
            return false;
        }

        setStateReady();
        return true;
    }

    void start() final
    {
        m_canvas->setRunning(true);

        // configure the acknowledgement line as a digital output
        LineCommand outSetup(LineCommandKind::SET_MODE, m_ackLine);
        outSetup.flags = LineModeFlag::IS_OUTPUT;
        m_lcStream->push(outSetup);

        AbstractModule::start();
    }

    void stop() final
    {
        m_canvas->setRunning(false);
    }

    void processLineA()
    {
        while (true) {
            const auto maybe = m_subA->peekNext();
            if (!maybe.has_value())
                break;
            const auto &r = *maybe;

            auto &last = m_lastValueA[r.lineId];
            const bool edge = isQualifyingEdge(last, r.value, m_edge);
            last = r.value;
            if (!edge)
                continue;

            if (m_mode == LatencyMode::SingleLine) {
                // Interval between consecutive qualifying pulses on line A.
                if (m_havePrevA) {
                    const auto latency = r.time - m_prevTimeA;
                    emitLatency(r.time, latency);
                }
                m_prevTimeA = r.time;
                m_havePrevA = true;
                fireAck();
            } else {
                // Dual-line: remember the A timestamp; B completes the pair.
                m_pendingTimeA = r.time;
                m_havePendingA = true;
            }
        }
    }

    void processLineB()
    {
        while (true) {
            const auto maybe = m_subB->peekNext();
            if (!maybe.has_value())
                break;
            const auto &r = *maybe;

            auto &last = m_lastValueB[r.lineId];
            const bool edge = isQualifyingEdge(last, r.value, m_edge);
            last = r.value;
            if (!edge)
                continue;

            if (m_havePendingA) {
                const auto latency = r.time - m_pendingTimeA;
                emitLatency(r.time, latency);
                m_havePendingA = false;
                fireAck();
            }
        }
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("mode", static_cast<int>(m_settingsDlg->mode()));
        settings.insert("trigger_edge", static_cast<int>(m_settingsDlg->edge()));
        settings.insert("ack_line", m_settingsDlg->ackLine());
        settings.insert("ack_pulse_ms", m_settingsDlg->ackPulseMs());
        settings.insert("histogram_bins", m_canvas->binCount());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setMode(static_cast<LatencyMode>(settings.value("mode", 0).toInt()));
        m_settingsDlg->setEdge(static_cast<TriggerEdge>(settings.value("trigger_edge", 0).toInt()));
        m_settingsDlg->setAckLine(static_cast<uint16_t>(settings.value("ack_line", 8).toUInt()));
        m_settingsDlg->setAckPulseMs(settings.value("ack_pulse_ms", 30).toInt());
        m_canvas->setBinCount(settings.value("histogram_bins", 30).toInt());

        return true;
    }

private:
    void emitLatency(microseconds_t measTime, microseconds_t latency)
    {
        SignalBlockI32 sb(1, 1);
        sb.timestamps[0] = static_cast<uint64_t>(measTime.count());
        sb.data(0, 0) = latency.count();
        m_latStream->push(sb);

        m_canvas->addValue(static_cast<float>(latency.count() / (float)US_PER_MS));
    }

    void fireAck()
    {
        LineCommand ctl(LineCommandKind::WRITE_DIGITAL_PULSE, m_ackLine, 1);
        ctl.duration = std::chrono::duration_cast<microseconds_t>(milliseconds_t(m_ackPulseMs));
        m_lcStream->push(ctl);
    }
};

QString LatencyCheckModuleInfo::id() const
{
    return QStringLiteral("latencycheck");
}

QString LatencyCheckModuleInfo::name() const
{
    return QStringLiteral("Latency Check");
}

QString LatencyCheckModuleInfo::description() const
{
    return QStringLiteral("Measure and visualize TTL line latencies.");
}

ModuleCategories LatencyCheckModuleInfo::categories() const
{
    return ModuleCategory::DISPLAY | ModuleCategory::PROCESSING;
}

AbstractModule *LatencyCheckModuleInfo::createModule(QObject *parent)
{
    return new LatencyCheckModule(this, parent);
}

#include "latencycheckmodule.moc"
