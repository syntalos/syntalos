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

#include "flowmetermodule.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>

#include <QDialog>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <KCapacityBar>
#include <KLed>

#include "datactl/datatypes.h"
#include "datatypeselector.h"

SYNTALOS_MODULE(FlowMeterModule)

// Display refresh / activity-LED cadence.
static constexpr int kRefreshHz = 10;

// EMA smoothing factor for the displayed rate (0 < α <= 1; higher = more responsive).
static constexpr double kRateEmaAlpha = 0.35;

// Edge length of the square activity LED (also the load-bar height), in pixels.
static constexpr int kLedSize = 24;

/**
 * @brief Settings dialog: pick the data type the meter should count.
 */
class FlowMeterSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FlowMeterSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Flow Meter Settings"));
        setMaximumSize(420, 120);

        auto layout = new QFormLayout(this);
        // the flow meter can count any kind of stream, so it offers every data type
        m_typeSel = new DataTypeSelector(this);
        m_typeSel->addAllDataTypes();
        layout->addRow(QStringLiteral("Data type"), m_typeSel);

        connect(m_typeSel, &DataTypeSelector::selectionChanged, this, &FlowMeterSettingsDialog::settingsChanged);
    }

    int selectedTypeId() const
    {
        return m_typeSel->selectedTypeId();
    }

    QString selectedTypeName() const
    {
        return m_typeSel->selectedTypeName();
    }

    void setSelectedTypeName(const QString &typeName)
    {
        m_typeSel->setSelectedTypeName(typeName);
    }

    void setRunning(bool running)
    {
        // The port topology may not change during a run
        m_typeSel->setEnabled(!running);
    }

Q_SIGNALS:
    void settingsChanged();

private:
    DataTypeSelector *m_typeSel;
};

/**
 * @brief Display: activity LED, load bar and live throughput statistics.
 *
 * The module thread only increments an atomic counter; this widget polls it on
 * the GUI thread via a repaint timer, so no cross-thread locking is required.
 */
class FlowMeterDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit FlowMeterDisplay(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto layout = new QVBoxLayout(this);

        auto topRow = new QHBoxLayout;
        m_led = new KLed(QColor(0x36, 0xd1, 0x4d), this); // green activity LED
        m_led->off();
        // a visibly-sized, square LED sitting right next to the bar and matching its height
        m_led->setFixedSize(kLedSize, kLedSize);
        topRow->addWidget(m_led, 0);
        m_loadBar = new KCapacityBar(KCapacityBar::DrawTextInline, this);
        m_loadBar->setValue(0);
        m_loadBar->setFixedHeight(kLedSize);
        topRow->addWidget(m_loadBar, 1);
        layout->addLayout(topRow);

        auto form = new QFormLayout;
        m_typeLabel = new QLabel(QStringLiteral("—"), this);
        m_totalLabel = new QLabel(this);
        m_rateLabel = new QLabel(this);
        m_meanLabel = new QLabel(this);
        m_peakLabel = new QLabel(this);
        form->addRow(QStringLiteral("Data type"), m_typeLabel);
        form->addRow(QStringLiteral("Total items"), m_totalLabel);
        form->addRow(QStringLiteral("Current rate"), m_rateLabel);
        form->addRow(QStringLiteral("Mean rate"), m_meanLabel);
        form->addRow(QStringLiteral("Peak rate"), m_peakLabel);
        layout->addLayout(form);
        layout->addStretch(1);

        // Keep the window compact; maximizing it just stretches empty space.
        setMaximumSize(480, 320);

        m_timer = new QTimer(this);
        m_timer->setInterval(1000 / kRefreshHz);
        connect(m_timer, &QTimer::timeout, this, &FlowMeterDisplay::refresh);

        resetStats();
        updateLabels();
    }

    void setStatsSource(std::function<uint64_t()> fn)
    {
        m_statsSource = std::move(fn);
    }

    void setTypeName(const QString &name)
    {
        m_typeLabel->setText(name.isEmpty() ? QStringLiteral("—") : name);
    }

    void setRunning(bool running)
    {
        if (running) {
            resetStats();
            updateLabels();
            m_runTimer.restart();
            m_timer->start();
        } else {
            m_timer->stop();
            m_led->off();
        }
    }

private:
    void resetStats()
    {
        m_lastCount = 0;
        m_lastElapsedMs = 0;
        m_emaRate = 0.0;
        m_peakRate = 0.0;
    }

    void refresh()
    {
        const uint64_t count = m_statsSource ? m_statsSource() : 0;
        const qint64 nowMs = m_runTimer.isValid() ? m_runTimer.elapsed() : 0;
        const qint64 dtMs = nowMs - m_lastElapsedMs;

        // Activity LED: lit if anything arrived since the previous tick.
        if (count > m_lastCount)
            m_led->on();
        else
            m_led->off();

        if (dtMs > 0) {
            const double instRate = static_cast<double>(count - m_lastCount) * 1000.0 / dtMs;
            m_emaRate = kRateEmaAlpha * instRate + (1.0 - kRateEmaAlpha) * m_emaRate;
            m_peakRate = std::max(m_peakRate, m_emaRate);
        }

        m_lastCount = count;
        m_lastElapsedMs = nowMs;
        updateLabels();
    }

    void updateLabels()
    {
        const uint64_t count = m_lastCount;
        const double nowSec = m_lastElapsedMs / 1000.0;
        const double meanRate = nowSec > 0.0 ? static_cast<double>(count) / nowSec : 0.0;

        m_totalLabel->setText(QString::number(count));
        m_rateLabel->setText(QStringLiteral("%1 /s").arg(m_emaRate, 0, 'f', 1));
        m_meanLabel->setText(QStringLiteral("%1 /s").arg(meanRate, 0, 'f', 1));
        m_peakLabel->setText(QStringLiteral("%1 /s").arg(m_peakRate, 0, 'f', 1));

        const int loadPct = m_peakRate > 0.0 ? static_cast<int>(std::lround(100.0 * m_emaRate / m_peakRate)) : 0;
        m_loadBar->setValue(std::clamp(loadPct, 0, 100));
        m_loadBar->setText(QStringLiteral("%1 /s").arg(m_emaRate, 0, 'f', 0));
    }

    KLed *m_led;
    KCapacityBar *m_loadBar;
    QLabel *m_typeLabel;
    QLabel *m_totalLabel;
    QLabel *m_rateLabel;
    QLabel *m_meanLabel;
    QLabel *m_peakLabel;

    QTimer *m_timer;
    QElapsedTimer m_runTimer;
    std::function<uint64_t()> m_statsSource;

    uint64_t m_lastCount = 0;
    qint64 m_lastElapsedMs = 0;
    double m_emaRate = 0.0;
    double m_peakRate = 0.0;
};

class FlowMeterModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<VarStreamInputPort> m_inPort;
    std::shared_ptr<VariantStreamSubscription> m_sub;
    std::atomic<uint64_t> m_count{0};

    FlowMeterSettingsDialog *m_settingsDlg;
    FlowMeterDisplay *m_display;

public:
    explicit FlowMeterModule(FlowMeterModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_display = new FlowMeterDisplay;
        m_display->setWindowIcon(modInfo->icon());
        m_display->setStatsSource([this]() {
            return m_count.load(std::memory_order_relaxed);
        });
        addDisplayWindow(m_display);

        m_settingsDlg = new FlowMeterSettingsDialog;
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &FlowMeterSettingsDialog::settingsChanged, this, [this]() {
            updatePortConfiguration();
        });

        // create the initial input port for the default-selected type
        updatePortConfiguration();
    }

    ~FlowMeterModule() override = default;

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS | ModuleFeature::SHOW_DISPLAY;
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_display->setWindowTitle(name);
    }

    void updatePortConfiguration()
    {
        // Rebuild the single input port to match the selected data type.
        // Only safe on the main thread while not running.
        clearInPorts();
        m_inPort = registerInputPortByTypeId(
            m_settingsDlg->selectedTypeId(),
            QStringLiteral("data-in"),
            QStringLiteral("Data"));
    }

    bool prepare(const TestSubject &) override
    {
        m_settingsDlg->setRunning(true);

        m_count.store(0, std::memory_order_relaxed);
        m_sub.reset();
        clearDataReceivedEventRegistrations();

        m_display->setTypeName(m_settingsDlg->selectedTypeName());

        if (!m_inPort || !m_inPort->hasSubscription()) {
            setStateDormant();
            return true;
        }

        m_sub = m_inPort->subscriptionVar();
        registerDataReceivedEvent(
            [this]() {
                onData();
            },
            m_sub);

        setStateReady();
        return true;
    }

    void start() override
    {
        m_display->setRunning(true);
        AbstractModule::start();
    }

    void stop() override
    {
        m_display->setRunning(false);
        m_settingsDlg->setRunning(false);
    }

    void onData()
    {
        // Drain every queued item and count it. We must drain (even though we
        // discard the decoded value) so the meter does not act as a bottleneck.
        while (m_sub->callIfNextVar([](BaseDataType &) {}))
            m_count.fetch_add(1, std::memory_order_relaxed);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("data_type", m_settingsDlg->selectedTypeName());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        const auto typeName = settings.value("data_type").toString();
        if (!typeName.isEmpty())
            m_settingsDlg->setSelectedTypeName(typeName);
        updatePortConfiguration();
        return true;
    }
};

QString FlowMeterModuleInfo::id() const
{
    return QStringLiteral("flowmeter");
}

QString FlowMeterModuleInfo::name() const
{
    return QStringLiteral("Flow Meter");
}

QString FlowMeterModuleInfo::description() const
{
    return QStringLiteral("Count items on any stream and show how fast they arrive.");
}

ModuleCategories FlowMeterModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV | ModuleCategory::DISPLAY;
}

AbstractModule *FlowMeterModuleInfo::createModule(QObject *parent)
{
    return new FlowMeterModule(this, parent);
}

#include "flowmetermodule.moc"
