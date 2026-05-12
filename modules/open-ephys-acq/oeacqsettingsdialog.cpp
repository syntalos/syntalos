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

#include "oeacqsettingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

OeAcqSettingsDialog::OeAcqSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Open Ephys AcqBoard — Settings"));
    buildUi();
}

void OeAcqSettingsDialog::buildUi()
{
    auto root = new QVBoxLayout(this);

    // ---------- backend group ----------
    auto backendGroup = new QGroupBox(QStringLiteral("Backend"), this);
    auto backendForm = new QFormLayout(backendGroup);

    m_backendCombo = new QComboBox(backendGroup);
    m_backendCombo->addItem(QStringLiteral("Auto-detect (use device if connected)"), static_cast<int>(BackendAuto));
    m_backendCombo->addItem(QStringLiteral("Open Ephys Acquisition Board"), static_cast<int>(BackendDevice));
    m_backendCombo->addItem(QStringLiteral("Simulated"), static_cast<int>(BackendSimulated));
    backendForm->addRow(QStringLiteral("Backend:"), m_backendCombo);

    auto activeRow = new QHBoxLayout;
    m_activeBackendLabel = new QLabel(QStringLiteral("(not yet initialised)"), backendGroup);
    m_activeBackendLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    activeRow->addWidget(m_activeBackendLabel, 1);
    m_reconnectButton = new QPushButton(QStringLiteral("Reconnect"), backendGroup);
    activeRow->addWidget(m_reconnectButton);
    backendForm->addRow("Active:", activeRow);

    root->addWidget(backendGroup);

    // ---------- acquisition group ----------
    auto acqGroup = new QGroupBox("Acquisition", this);
    auto acqForm = new QFormLayout(acqGroup);

    m_sampleRateCombo = new QComboBox(acqGroup);
    acqForm->addRow("Sample rate:", m_sampleRateCombo);

    m_acquireAuxCheck = new QCheckBox(QStringLiteral("Stream AUX channels (3 per headstage)"), acqGroup);
    acqForm->addRow(QString(), m_acquireAuxCheck);

    m_acquireAdcCheck = new QCheckBox(QStringLiteral("Stream board ADC channels (8)"), acqGroup);
    acqForm->addRow(QString(), m_acquireAdcCheck);

    m_namingCombo = new QComboBox(acqGroup);
    m_namingCombo->addItem(QStringLiteral("Global index (CH1, CH2, …)"), static_cast<int>(GLOBAL_INDEX));
    m_namingCombo->addItem(QStringLiteral("Stream-relative (A1_CH1, A1_CH2, …)"), static_cast<int>(STREAM_INDEX));
    acqForm->addRow(QStringLiteral("Channel naming:"), m_namingCombo);

    root->addWidget(acqGroup);

    // ---------- headstage panel ----------
    auto hsGroup = new QGroupBox(QStringLiteral("Detected headstages"), this);
    auto hsLayout = new QVBoxLayout(hsGroup);
    m_headstageLabel = new QLabel(QStringLiteral("No scan performed yet."), hsGroup);
    m_headstageLabel->setWordWrap(true);
    m_headstageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hsLayout->addWidget(m_headstageLabel);

    auto hsBtnRow = new QHBoxLayout;
    m_scanButton = new QPushButton(QStringLiteral("Rescan headstages"), hsGroup);
    hsBtnRow->addWidget(m_scanButton);
    m_impedanceButton = new QPushButton(QStringLiteral("Measure impedances…"), hsGroup);
    hsBtnRow->addWidget(m_impedanceButton);
    hsBtnRow->addStretch(1);
    hsLayout->addLayout(hsBtnRow);

    root->addWidget(hsGroup);

    // ---------- channel selection group ----------
    m_channelsGroup = new QGroupBox("Channels", this);
    auto channelsOuter = new QVBoxLayout(m_channelsGroup);

    auto channelsToolbar = new QHBoxLayout;
    m_enableAllButton = new QPushButton("Enable all", m_channelsGroup);
    m_disableAllButton = new QPushButton("Disable all", m_channelsGroup);
    channelsToolbar->addWidget(m_enableAllButton);
    channelsToolbar->addWidget(m_disableAllButton);
    channelsToolbar->addStretch(1);
    channelsOuter->addLayout(channelsToolbar);

    auto channelsScroll = new QScrollArea(m_channelsGroup);
    channelsScroll->setWidgetResizable(true);
    channelsScroll->setMinimumHeight(240);
    m_channelsHost = new QWidget(channelsScroll);
    m_channelsLayout = new QVBoxLayout(m_channelsHost);
    m_channelsLayout->setContentsMargins(4, 4, 4, 4);
    m_channelsLayout->addStretch(1);
    channelsScroll->setWidget(m_channelsHost);
    channelsOuter->addWidget(channelsScroll, 1);

    root->addWidget(m_channelsGroup, 1);

    connect(m_enableAllButton, &QPushButton::clicked, this, [this]() {
        for (const auto &e : m_channelEntries) {
            auto cb = m_channelChecks.value(e.id);
            if (cb && !cb->isChecked() && cb->isEnabled())
                cb->setChecked(true);
        }
    });
    connect(m_disableAllButton, &QPushButton::clicked, this, [this]() {
        for (const auto &e : m_channelEntries) {
            auto *cb = m_channelChecks.value(e.id);
            if (cb && cb->isChecked() && cb->isEnabled())
                cb->setChecked(false);
        }
    });
    // ---------- close button row ----------
    auto btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    auto closeBtn = new QPushButton(QStringLiteral("Close"), this);
    closeBtn->setDefault(true);
    btnRow->addWidget(closeBtn);
    root->addLayout(btnRow);

    // ---------- wiring ----------
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_sampleRateCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_emitSignals)
            return;
        const auto v = m_sampleRateCombo->currentData().toInt();
        if (v > 0)
            emit sampleRateChanged(v);
    });
    connect(m_acquireAuxCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_emitSignals)
            emit acquireAuxChanged(on);
    });
    connect(m_acquireAdcCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_emitSignals)
            emit acquireAdcChanged(on);
    });
    connect(m_namingCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_emitSignals)
            return;
        emit namingSchemeChanged(m_namingCombo->currentData().toInt());
    });
    connect(m_scanButton, &QPushButton::clicked, this, &OeAcqSettingsDialog::rescanRequested);
    connect(m_impedanceButton, &QPushButton::clicked, this, &OeAcqSettingsDialog::measureImpedancesRequested);

    connect(m_backendCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_emitSignals)
            return;
        emit backendChoiceChanged(static_cast<BackendChoice>(m_backendCombo->currentData().toInt()));
    });
    connect(m_reconnectButton, &QPushButton::clicked, this, &OeAcqSettingsDialog::reconnectRequested);
}

void OeAcqSettingsDialog::setAvailableSampleRates(const std::vector<int> &ratesHz)
{
    const QSignalBlocker block(m_sampleRateCombo);
    const int previous = m_sampleRateCombo->currentData().toInt();
    m_sampleRateCombo->clear();
    for (int r : ratesHz)
        m_sampleRateCombo->addItem(QStringLiteral("%1 Hz").arg(r), r);

    // Preserve previous selection if it's still available.
    int idx = m_sampleRateCombo->findData(previous);
    if (idx < 0)
        idx = 0;
    m_sampleRateCombo->setCurrentIndex(idx);
}

void OeAcqSettingsDialog::setHeadstageSummary(const std::vector<const Headstage *> &headstages)
{
    if (headstages.empty()) {
        m_headstageLabel->setText(QStringLiteral("No connected headstages."));
        return;
    }
    QStringList lines;
    lines.reserve(static_cast<int>(headstages.size()));
    for (const auto hs : headstages) {
        if (hs == nullptr || !hs->isConnected())
            continue;
        lines << QStringLiteral("• %1 – %2 channels")
                     .arg(QString::fromStdString(hs->getStreamPrefix()))
                     .arg(hs->getNumActiveChannels());
    }
    if (lines.isEmpty())
        m_headstageLabel->setText(QStringLiteral("No connected headstages."));
    else
        m_headstageLabel->setText(lines.join(QStringLiteral("\n")));
}

int OeAcqSettingsDialog::sampleRateHz() const
{
    return m_sampleRateCombo->currentData().toInt();
}

void OeAcqSettingsDialog::setSampleRateHz(int rate)
{
    const QSignalBlocker block(m_sampleRateCombo);
    int idx = m_sampleRateCombo->findData(rate);
    if (idx < 0) {
        // Append the foreign rate so the saved value isn't silently lost.
        m_sampleRateCombo->addItem(QStringLiteral("%1 Hz").arg(rate), rate);
        idx = m_sampleRateCombo->count() - 1;
    }
    m_sampleRateCombo->setCurrentIndex(idx);
}

bool OeAcqSettingsDialog::acquireAux() const
{
    return m_acquireAuxCheck->isChecked();
}

void OeAcqSettingsDialog::setAcquireAux(bool enabled)
{
    const QSignalBlocker block(m_acquireAuxCheck);
    m_acquireAuxCheck->setChecked(enabled);
}

bool OeAcqSettingsDialog::acquireAdc() const
{
    return m_acquireAdcCheck->isChecked();
}

void OeAcqSettingsDialog::setAcquireAdc(bool enabled)
{
    const QSignalBlocker block(m_acquireAdcCheck);
    m_acquireAdcCheck->setChecked(enabled);
}

ChannelNamingScheme OeAcqSettingsDialog::namingScheme() const
{
    return static_cast<ChannelNamingScheme>(m_namingCombo->currentData().toInt());
}

void OeAcqSettingsDialog::setNamingScheme(ChannelNamingScheme scheme)
{
    const QSignalBlocker block(m_namingCombo);
    int idx = m_namingCombo->findData(static_cast<int>(scheme));
    if (idx >= 0)
        m_namingCombo->setCurrentIndex(idx);
}

void OeAcqSettingsDialog::setRunActive(bool active)
{
    // Sample rate / aux / adc require board reconfiguration that we don't
    // do live during a run. Naming and rescan stay locked too - the port
    // set is frozen during runThread().
    m_sampleRateCombo->setEnabled(!active);
    m_acquireAuxCheck->setEnabled(!active);
    m_acquireAdcCheck->setEnabled(!active);
    m_namingCombo->setEnabled(!active);
    m_scanButton->setEnabled(!active);
    m_impedanceButton->setEnabled(!active);
    m_backendCombo->setEnabled(!active);
    m_reconnectButton->setEnabled(!active);

    // Bulk-toggle buttons are dangerous during a run (would flip many
    // currently-checked boxes off, which we don't want as a single click).
    m_enableAllButton->setEnabled(!active);
    m_disableAllButton->setEnabled(!active);
    // Run rules: channels that started this run enabled stay clickable —
    // unchecking mid-run zero-fills the column, re-checking resumes samples.
    // Channels that started the run disabled have no column in the matrix,
    // so we lock their checkboxes off until the run stops.
    for (auto it = m_channelChecks.cbegin(); it != m_channelChecks.cend(); ++it) {
        QCheckBox *cb = it.value();
        if (!cb)
            continue;
        if (active)
            cb->setEnabled(cb->isChecked());
        else
            cb->setEnabled(true);
    }
}

void OeAcqSettingsDialog::setChannelInventory(const std::vector<ChannelEntry> &entries)
{
    m_channelEntries = entries;
    rebuildChannelPanelControls();
}

void OeAcqSettingsDialog::rebuildChannelPanelControls()
{
    // Tear down existing per-group children of m_channelsHost.
    QLayoutItem *item;
    while ((item = m_channelsLayout->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    m_channelChecks.clear();

    if (m_channelEntries.empty()) {
        auto placeholder = new QLabel(
            "No channels available. Connect a headstage or enable Board ADC and rescan.",
            m_channelsHost);
        placeholder->setWordWrap(true);
        m_channelsLayout->addWidget(placeholder);
        m_channelsLayout->addStretch(1);
        return;
    }

    // Preserve insertion order of groups while bucketing entries.
    QStringList groupOrder;
    QMap<QString, std::vector<const ChannelEntry *>> buckets;
    for (const auto &e : m_channelEntries) {
        if (!buckets.contains(e.group)) {
            buckets[e.group] = {};
            groupOrder << e.group;
        }
        buckets[e.group].push_back(&e);
    }

    for (const auto &groupName : groupOrder) {
        auto gb = new QGroupBox(groupName, m_channelsHost);
        auto grid = new QGridLayout(gb);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(4);

        const auto &bucket = buckets[groupName];
        constexpr int kCols = 8;
        int col = 0;
        int row = 0;
        for (const auto ePtr : bucket) {
            const ChannelEntry &e = *ePtr;
            auto *cb = new QCheckBox(e.label, gb);
            cb->setChecked(e.enabled);
            m_channelChecks.insert(e.id, cb);

            const QString id = e.id;
            connect(cb, &QCheckBox::toggled, this, [this, id](bool on) {
                if (!m_emitSignals)
                    return;
                Q_EMIT channelEnabledChanged(id, on);
            });

            grid->addWidget(cb, row, col);
            ++col;
            if (col >= kCols) {
                col = 0;
                ++row;
            }
        }
        m_channelsLayout->addWidget(gb);
    }
    m_channelsLayout->addStretch(1);
}

void OeAcqSettingsDialog::setActiveBackendLabel(const QString &text)
{
    m_activeBackendLabel->setText(text);
}

OeAcqSettingsDialog::BackendChoice OeAcqSettingsDialog::backendChoice() const
{
    return static_cast<BackendChoice>(m_backendCombo->currentData().toInt());
}

void OeAcqSettingsDialog::setBackendChoice(BackendChoice choice)
{
    const QSignalBlocker block(m_backendCombo);
    int idx = m_backendCombo->findData(static_cast<int>(choice));
    if (idx < 0)
        idx = 0;
    m_backendCombo->setCurrentIndex(idx);
}
