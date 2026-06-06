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

#include "signalfiltersettingsdialog.h"
#include "ui_signalfiltersettingsdialog.h"

#include <QSignalBlocker>

#include "datactl/datatypes.h"

using namespace Syntalos;

SignalFilterSettingsDialog::SignalFilterSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SignalFilterSettingsDialog),
      m_currentIndex(-1),
      m_loading(false)
{
    ui->setupUi(this);

    // input type selector (only the signal block types make sense here)
    ui->inputTypeSel->addDataType(SignalBlockF32::staticTypeId(), QStringLiteral("Float32 Signals"));
    ui->inputTypeSel->addDataType(SignalBlockI32::staticTypeId(), QStringLiteral("Int32 Signals"));
    ui->inputTypeSel->addDataType(SignalBlockU16::staticTypeId(), QStringLiteral("UInt16 Signals"));
    ui->inputTypeSel->setToolTip(QStringLiteral(
        "Frequency filters remove the DC offset, so filtering raw integer/unsigned signals is "
        "lossy (negative excursions clamp to the type minimum).\nPrefer Float32 where possible."));

    // family / response options; the combo indices match the enum values
    ui->cbFamily->addItems(
        {QStringLiteral("Butterworth"),
         QStringLiteral("Chebyshev I"),
         QStringLiteral("Chebyshev II"),
         QStringLiteral("RBJ Notch"),
         QStringLiteral("Custom (SOS)")});
    ui->cbResponse->addItems(
        {QStringLiteral("Low-pass"),
         QStringLiteral("High-pass"),
         QStringLiteral("Band-pass"),
         QStringLiteral("Band-stop")});

    connect(ui->inputTypeSel, &DataTypeSelector::selectionChanged, this, [this]() {
        Q_EMIT settingsChanged();
    });

    connect(ui->rbSelectedChannels, &QRadioButton::toggled, ui->leChannels, &QLineEdit::setEnabled);

    // channel selection can be changed live during a run
    connect(ui->rbAllChannels, &QRadioButton::toggled, this, [this]() {
        Q_EMIT channelsChanged();
    });
    connect(ui->leChannels, &QLineEdit::textChanged, this, [this]() {
        Q_EMIT channelsChanged();
    });

    connect(ui->lwStages, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_loading)
            return;
        m_currentIndex = row;
        const bool valid = row >= 0 && row < static_cast<int>(m_stages.size());
        ui->gbStageEditor->setEnabled(valid);
        if (valid)
            populateEditorFromStage(row);
    });

    connect(ui->tbAddStage, &QToolButton::clicked, this, [this]() {
        m_stages.emplace_back();
        rebuildStageList();
        ui->lwStages->setCurrentRow(static_cast<int>(m_stages.size()) - 1);
        Q_EMIT stagesChanged();
    });
    connect(ui->tbRemoveStage, &QToolButton::clicked, this, [this]() {
        if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_stages.size()))
            return;
        const int removed = m_currentIndex;
        m_stages.erase(m_stages.begin() + removed);
        rebuildStageList();
        if (!m_stages.empty())
            ui->lwStages->setCurrentRow(std::min(removed, static_cast<int>(m_stages.size()) - 1));
        else {
            m_currentIndex = -1;
            ui->gbStageEditor->setEnabled(false);
        }
        Q_EMIT stagesChanged();
    });
    connect(ui->tbStageUp, &QToolButton::clicked, this, [this]() {
        if (m_currentIndex <= 0)
            return;
        std::swap(m_stages[m_currentIndex], m_stages[m_currentIndex - 1]);
        const int target = m_currentIndex - 1;
        rebuildStageList();
        ui->lwStages->setCurrentRow(target);
        Q_EMIT stagesChanged();
    });
    connect(ui->tbStageDown, &QToolButton::clicked, this, [this]() {
        if (m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_stages.size()) - 1)
            return;
        std::swap(m_stages[m_currentIndex], m_stages[m_currentIndex + 1]);
        const int target = m_currentIndex + 1;
        rebuildStageList();
        ui->lwStages->setCurrentRow(target);
        Q_EMIT stagesChanged();
    });

    // editor widget changes write back into the current stage
    const auto onEditChanged = [this]() {
        writeEditorToStage();
    };
    connect(ui->cbFamily, &QComboBox::currentIndexChanged, this, [this](int) {
        writeEditorToStage();
        updateEditorVisibility();
    });
    connect(ui->cbResponse, &QComboBox::currentIndexChanged, this, [this](int) {
        writeEditorToStage();
        updateEditorVisibility();
    });
    connect(ui->sbOrder, &QSpinBox::valueChanged, this, onEditChanged);
    connect(ui->sbFreq1, &QDoubleSpinBox::valueChanged, this, onEditChanged);
    connect(ui->sbFreq2, &QDoubleSpinBox::valueChanged, this, onEditChanged);
    connect(ui->sbRipple, &QDoubleSpinBox::valueChanged, this, onEditChanged);
    connect(ui->sbStopband, &QDoubleSpinBox::valueChanged, this, onEditChanged);
    connect(ui->sbQ, &QDoubleSpinBox::valueChanged, this, onEditChanged);
    connect(ui->teSos, &QPlainTextEdit::textChanged, this, onEditChanged);

    ui->gbStageEditor->setEnabled(false);
}

SignalFilterSettingsDialog::~SignalFilterSettingsDialog()
{
    delete ui;
}

int SignalFilterSettingsDialog::selectedTypeId() const
{
    return ui->inputTypeSel->selectedTypeId();
}

QString SignalFilterSettingsDialog::selectedTypeName() const
{
    return ui->inputTypeSel->selectedTypeName();
}

void SignalFilterSettingsDialog::setSelectedTypeName(const QString &typeName)
{
    ui->inputTypeSel->setSelectedTypeName(typeName);
}

bool SignalFilterSettingsDialog::useAllChannels() const
{
    return ui->rbAllChannels->isChecked();
}

QString SignalFilterSettingsDialog::channelSelectionText() const
{
    return ui->leChannels->text().trimmed();
}

void SignalFilterSettingsDialog::setChannelSelection(bool useAll, const QString &ranges)
{
    ui->rbAllChannels->setChecked(useAll);
    ui->rbSelectedChannels->setChecked(!useAll);
    ui->leChannels->setEnabled(!useAll);
    ui->leChannels->setText(ranges);
}

std::vector<FilterStage> SignalFilterSettingsDialog::stages() const
{
    return m_stages;
}

void SignalFilterSettingsDialog::setStages(const std::vector<FilterStage> &stages)
{
    m_stages = stages;
    rebuildStageList();
    if (!m_stages.empty())
        ui->lwStages->setCurrentRow(0);
    else {
        m_currentIndex = -1;
        ui->gbStageEditor->setEnabled(false);
    }
}

void SignalFilterSettingsDialog::setRunning(bool running)
{
    // Only the input type changes the port topology, so it must stay locked
    // during a run. Channel selection and the filter design can be tuned live.
    ui->inputTypeSel->setEnabled(!running);
}

static QString sosToText(const std::vector<std::array<double, 6>> &sos)
{
    QStringList lines;
    for (const auto &row : sos) {
        QStringList nums;
        for (double v : row)
            nums << QString::number(v, 'g', 12);
        lines << nums.join(QLatin1Char(' '));
    }
    return lines.join(QLatin1Char('\n'));
}

struct SosParseResult {
    std::vector<std::array<double, 6>> rows;
    QString error; ///< first problem found, empty if every non-blank line is valid
};

/**
 * Render the parsed-SOS status into @p label (red on error/empty, green with a
 * section count).
 */
static void renderSosStatus(QLabel *label, const SosParseResult &res)
{
    if (!res.error.isEmpty()) {
        label->setText(res.error);
        label->setStyleSheet(QStringLiteral("color: #c0392b;")); // red
    } else if (res.rows.empty()) {
        label->setText(QStringLiteral("No sections entered — paste at least one line of 6 coefficients."));
        label->setStyleSheet(QStringLiteral("color: #c0392b;"));
    } else {
        label->setText(QStringLiteral("%1 section(s) parsed.").arg(res.rows.size()));
        label->setStyleSheet(QStringLiteral("color: #27ae60;")); // green
    }
}

/**
 * Parse the SOS text box. Each non-blank line must hold exactly 6 numbers
 * (b0 b1 b2 a0 a1 a2), whitespace- or comma-separated. Reports the first
 * malformed line so the user gets actionable feedback.
 */
static SosParseResult parseSos(const QString &text)
{
    SosParseResult res;
    const auto lines = text.split(QLatin1Char('\n'));
    int lineNo = 0;
    for (const auto &rawLine : lines) {
        ++lineNo;
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        QString norm = line;
        norm.replace(QLatin1Char(','), QLatin1Char(' '));
        const auto tokens = norm.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() != 6) {
            if (res.error.isEmpty())
                res.error = QStringLiteral("Line %1: expected 6 values (b0 b1 b2 a0 a1 a2), got %2.")
                                .arg(lineNo)
                                .arg(tokens.size());
            continue;
        }
        std::array<double, 6> row{};
        bool ok = true;
        for (int i = 0; i < 6 && ok; ++i) {
            row[static_cast<size_t>(i)] = tokens[i].toDouble(&ok);
            if (!ok && res.error.isEmpty())
                res.error = QStringLiteral("Line %1: \"%2\" is not a number.").arg(lineNo).arg(tokens[i]);
        }
        if (ok)
            res.rows.push_back(row);
    }

    return res;
}

QString SignalFilterSettingsDialog::stageLabel(const FilterStage &stage) const
{
    const QString family = ui->cbFamily->itemText(static_cast<int>(stage.family));
    switch (stage.family) {
    case FilterFamily::RbjNotch:
        return QStringLiteral("%1 @ %2 Hz").arg(family).arg(stage.freq1);
    case FilterFamily::CustomSOS:
        return QStringLiteral("%1 (%2 sections)").arg(family).arg(stage.sos.size());
    default: {
        const QString resp = ui->cbResponse->itemText(static_cast<int>(stage.response));
        return QStringLiteral("%1 %2 (ord %3)").arg(family, resp).arg(stage.order);
    }
    }
}

void SignalFilterSettingsDialog::rebuildStageList()
{
    QSignalBlocker blocker(ui->lwStages);
    ui->lwStages->clear();
    for (const auto &stage : m_stages)
        ui->lwStages->addItem(stageLabel(stage));
}

void SignalFilterSettingsDialog::populateEditorFromStage(int index)
{
    if (index < 0 || index >= static_cast<int>(m_stages.size()))
        return;
    const auto &st = m_stages[static_cast<size_t>(index)];

    m_loading = true;
    ui->cbFamily->setCurrentIndex(static_cast<int>(st.family));
    ui->cbResponse->setCurrentIndex(static_cast<int>(st.response));
    ui->sbOrder->setValue(st.order);
    ui->sbFreq1->setValue(st.freq1);
    ui->sbFreq2->setValue(st.freq2);
    ui->sbRipple->setValue(st.rippleDb);
    ui->sbStopband->setValue(st.stopbandDb);
    ui->sbQ->setValue(st.qFactor);
    ui->teSos->setPlainText(sosToText(st.sos));
    m_loading = false;

    updateEditorVisibility();
}

void SignalFilterSettingsDialog::writeEditorToStage()
{
    if (m_loading || m_currentIndex < 0 || m_currentIndex >= static_cast<int>(m_stages.size()))
        return;
    auto &st = m_stages[static_cast<size_t>(m_currentIndex)];

    st.family = static_cast<FilterFamily>(ui->cbFamily->currentIndex());
    st.response = static_cast<FilterResponse>(ui->cbResponse->currentIndex());
    st.order = ui->sbOrder->value();
    st.freq1 = ui->sbFreq1->value();
    st.freq2 = ui->sbFreq2->value();
    st.rippleDb = ui->sbRipple->value();
    st.stopbandDb = ui->sbStopband->value();
    st.qFactor = ui->sbQ->value();
    const auto sosRes = parseSos(ui->teSos->toPlainText());
    st.sos = sosRes.rows;
    // A Custom (SOS) stage is only valid if every non-blank line parsed and at
    // least one section was entered; other families don't use the SOS box.
    st.sosValid = (st.family != FilterFamily::CustomSOS) || (sosRes.error.isEmpty() && !sosRes.rows.empty());

    if (st.family == FilterFamily::CustomSOS)
        renderSosStatus(ui->lblSosStatus, sosRes);
    else
        ui->lblSosStatus->clear();

    if (auto *item = ui->lwStages->item(m_currentIndex))
        item->setText(stageLabel(st));

    Q_EMIT stagesChanged();
}

void SignalFilterSettingsDialog::updateSosStatus()
{
    if (ui->cbFamily->currentIndex() != static_cast<int>(FilterFamily::CustomSOS)) {
        ui->lblSosStatus->clear();
        return;
    }

    renderSosStatus(ui->lblSosStatus, parseSos(ui->teSos->toPlainText()));
}

void SignalFilterSettingsDialog::updateEditorVisibility()
{
    const auto family = static_cast<FilterFamily>(ui->cbFamily->currentIndex());
    const auto response = static_cast<FilterResponse>(ui->cbResponse->currentIndex());

    const bool isPole = family == FilterFamily::Butterworth || family == FilterFamily::ChebyshevI
                        || family == FilterFamily::ChebyshevII;
    const bool isBand = isPole && (response == FilterResponse::BandPass || response == FilterResponse::BandStop);
    const bool isNotch = family == FilterFamily::RbjNotch;
    const bool isCustom = family == FilterFamily::CustomSOS;

    ui->formStage->setRowVisible(ui->cbResponse, isPole);
    ui->formStage->setRowVisible(ui->sbOrder, isPole);
    ui->formStage->setRowVisible(ui->sbFreq1, isPole || isNotch);
    ui->formStage->setRowVisible(ui->sbFreq2, isBand);
    ui->formStage->setRowVisible(ui->sbRipple, family == FilterFamily::ChebyshevI);
    ui->formStage->setRowVisible(ui->sbStopband, family == FilterFamily::ChebyshevII);
    ui->formStage->setRowVisible(ui->sbQ, isNotch);
    ui->formStage->setRowVisible(ui->teSos, isCustom);
    ui->lblSosHint->setVisible(isCustom);
    ui->formStage->setRowVisible(ui->lblSosStatus, isCustom);
    if (isCustom)
        updateSosStatus();

    ui->lblFreq1->setText((isBand || isNotch) ? QStringLiteral("Center (Hz):") : QStringLiteral("Cutoff (Hz):"));
}
