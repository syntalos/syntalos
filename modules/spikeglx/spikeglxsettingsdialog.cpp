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

#include "spikeglxsettingsdialog.h"
#include "ui_spikeglxsettingsdialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>

#include "sglxutils.h"

SpikeGLXSettingsDialog::SpikeGLXSettingsDialog(ModuleInfo *modInfo, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SpikeGLXSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(modInfo->icon());

    ui->modeComboBox->addItem(
        QStringLiteral("Automatic (start a run if SpikeGLX is idle, else gate recording)"),
        Automatic);
    ui->modeComboBox->addItem(QStringLiteral("Full control (start & stop the SpikeGLX run)"), FullControl);
    ui->modeComboBox->addItem(QStringLiteral("Gate only (SpikeGLX is already running)"), GateOnly);
    ui->modeComboBox->addItem(QStringLiteral("Monitor (never change SpikeGLX state)"), Monitor);

    ui->overrunComboBox->addItem(QStringLiteral("Abort the run (data must be complete)"), AbortRun);
    ui->overrunComboBox->addItem(QStringLiteral("Skip ahead and log the gap (display use)"), SkipAhead);

    ui->fetchTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->fetchTable->verticalHeader()->setVisible(false);

    // plain settings
    connect(ui->hostEdit, &QLineEdit::editingFinished, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->portSpinBox, &QSpinBox::valueChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->timeoutSpinBox, &QSpinBox::valueChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->modeComboBox, &QComboBox::currentIndexChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->deviceStringEdit, &QLineEdit::editingFinished, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->runNameExtraEdit, &QLineEdit::editingFinished, this, &SpikeGLXSettingsDialog::settingsChanged);

    // info buttons
    connect(ui->modeInfoButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this,
            QStringLiteral("Run control modes"),
            QStringLiteral("<html><b>%1</b><br/><br/>%2</html>")
                .arg(
                    ui->modeComboBox->currentText().toHtmlEscaped(),
                    modeDescription(runControlMode()).toHtmlEscaped()));
    });
    connect(ui->deviceInfoButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this,
            QStringLiteral("Device selection"),
            QStringLiteral(
                "<html>SpikeGLX has to validate its run parameters once after it was launched (the "
                "<i>Detect</i> and <i>Verify | Save</i> steps in its acquisition configuration). "
                "If the devices to select are entered here, the module performs these steps remotely whenever "
                "SpikeGLX reports unvalidated parameters.<br/><br/>"
                "Format, several entries may be combined:<br/>"
                "<tt>(slot,port,dock)</tt> for a probe, e.g. <tt>(40,1,1)</tt><br/>"
                "<tt>(slot,obx)</tt> for a OneBox, e.g. <tt>(21,obx)</tt><br/>"
                "<tt>(nidq)</tt> for NI-DAQ<br/><br/>"
                "Leave the field empty to use whatever was last verified in SpikeGLX.</html>"));
    });
    connect(ui->runNameInfoButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this,
            QStringLiteral("SpikeGLX run name"),
            QStringLiteral(
                "<html>The SpikeGLX run (file-set) name is generated from the Syntalos recording as "
                "<tt>&lt;date&gt;_&lt;subject&gt;_&lt;experiment&gt;_&lt;collection tag&gt;</tt>, e.g. "
                "<tt>20260830_TestSubject_MM-1_de5b737f</tt>; parts that are not available are left out.<br/><br/>"
                "The optional extra name is appended to it, which is useful when one Syntalos instance "
                "controls more than one SpikeGLX rig. Characters SpikeGLX does not permit are replaced.</html>"));
    });
    connect(ui->syncIntervalSpinBox, &QSpinBox::valueChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->syncStreamsEdit, &QLineEdit::editingFinished, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->fetchGroup, &QGroupBox::toggled, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->fetchIntervalSpinBox, &QSpinBox::valueChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->fetchBlockSpinBox, &QSpinBox::valueChanged, this, &SpikeGLXSettingsDialog::settingsChanged);
    connect(ui->overrunComboBox, &QComboBox::currentIndexChanged, this, &SpikeGLXSettingsDialog::settingsChanged);

    // actions
    connect(ui->testConnectionButton, &QPushButton::clicked, this, &SpikeGLXSettingsDialog::testConnectionRequested);
    connect(ui->queryStreamsButton, &QPushButton::clicked, this, &SpikeGLXSettingsDialog::queryStreamsRequested);

    // live-data entries
    connect(ui->fetchAddButton, &QPushButton::clicked, this, [this] {
        FetchEntry entry;
        entry.stream = m_knownStreams.isEmpty() ? QStringLiteral("imec0") : m_knownStreams.first();
        entry.group = entry.stream.startsWith(QLatin1String("imec")) ? QStringLiteral("AP") : QStringLiteral("XA");
        appendFetchRow(entry);
        emit fetchEntriesChanged();
    });
    connect(ui->fetchRemoveButton, &QPushButton::clicked, this, [this] {
        const auto rows = ui->fetchTable->selectionModel()->selectedRows();
        if (rows.isEmpty())
            return;
        ui->fetchTable->removeRow(rows.first().row());
        emit fetchEntriesChanged();
    });
    connect(ui->fetchTable, &QTableWidget::cellChanged, this, [this] {
        if (!m_updating)
            emit fetchEntriesChanged();
    });
}

SpikeGLXSettingsDialog::~SpikeGLXSettingsDialog()
{
    delete ui;
}

QString SpikeGLXSettingsDialog::host() const
{
    return ui->hostEdit->text().trimmed();
}

void SpikeGLXSettingsDialog::setHost(const QString &host)
{
    ui->hostEdit->setText(host);
}

int SpikeGLXSettingsDialog::port() const
{
    return ui->portSpinBox->value();
}

void SpikeGLXSettingsDialog::setPort(int port)
{
    ui->portSpinBox->setValue(port);
}

int SpikeGLXSettingsDialog::connectTimeoutMs() const
{
    return ui->timeoutSpinBox->value();
}

void SpikeGLXSettingsDialog::setConnectTimeoutMs(int msec)
{
    ui->timeoutSpinBox->setValue(msec);
}

SpikeGLXSettingsDialog::RunControlMode SpikeGLXSettingsDialog::runControlMode() const
{
    return static_cast<RunControlMode>(ui->modeComboBox->currentData().toInt());
}

void SpikeGLXSettingsDialog::setRunControlMode(RunControlMode mode)
{
    const int idx = ui->modeComboBox->findData(static_cast<int>(mode));
    ui->modeComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
}

QString SpikeGLXSettingsDialog::modeDescription(RunControlMode mode) const
{
    switch (mode) {
    case Automatic:
        return QStringLiteral(
            "If SpikeGLX is idle, acquisition is started while Syntalos prepares the run, recording is enabled "
            "when the run starts and the SpikeGLX run is stopped afterwards. If SpikeGLX is already running, "
            "Syntalos only enables and disables recording and leaves the run going.\n\n"
            "In the SpikeGLX Gates tab, keep the \"Show enable/disable recording button\" option enabled with "
            "\"initially disabled\" (the SpikeGLX default), so files are only written while Syntalos records.");
    case FullControl:
        return QStringLiteral(
            "SpikeGLX must be idle. Acquisition is started while Syntalos prepares the run, recording is enabled "
            "when the run starts and the SpikeGLX run is stopped afterwards.\n\n"
            "In the SpikeGLX Gates tab, keep the \"Show enable/disable recording button\" option enabled with "
            "\"initially disabled\" (the SpikeGLX default).");
    case GateOnly:
        return QStringLiteral(
            "Start the SpikeGLX run yourself with recording disabled. Syntalos enables recording when its run "
            "starts and disables it again when the run ends; the SpikeGLX run keeps going.");
    case Monitor:
        return QStringLiteral(
            "SpikeGLX must already be running. Syntalos only logs sample counts and optionally fetches live "
            "data, without changing any SpikeGLX state.");
    }
    return {};
}

QString SpikeGLXSettingsDialog::deviceString() const
{
    return ui->deviceStringEdit->text().simplified();
}

void SpikeGLXSettingsDialog::setDeviceString(const QString &devString)
{
    ui->deviceStringEdit->setText(devString);
}

QString SpikeGLXSettingsDialog::runNameExtra() const
{
    return ui->runNameExtraEdit->text().trimmed();
}

void SpikeGLXSettingsDialog::setRunNameExtra(const QString &extra)
{
    ui->runNameExtraEdit->setText(extra);
}

int SpikeGLXSettingsDialog::syncIntervalMs() const
{
    return ui->syncIntervalSpinBox->value();
}

void SpikeGLXSettingsDialog::setSyncIntervalMs(int msec)
{
    ui->syncIntervalSpinBox->setValue(msec);
}

QStringList SpikeGLXSettingsDialog::syncStreams() const
{
    QStringList result;
    const auto parts = ui->syncStreamsEdit->text().split(',', Qt::SkipEmptyParts);
    for (const auto &p : parts) {
        const auto s = p.trimmed().toLower();
        if (!s.isEmpty() && !result.contains(s))
            result << s;
    }
    return result;
}

void SpikeGLXSettingsDialog::setSyncStreams(const QStringList &streams)
{
    ui->syncStreamsEdit->setText(streams.join(QStringLiteral(", ")));
}

bool SpikeGLXSettingsDialog::fetchEnabled() const
{
    return ui->fetchGroup->isChecked();
}

void SpikeGLXSettingsDialog::setFetchEnabled(bool enabled)
{
    ui->fetchGroup->setChecked(enabled);
}

int SpikeGLXSettingsDialog::fetchIntervalMs() const
{
    return ui->fetchIntervalSpinBox->value();
}

void SpikeGLXSettingsDialog::setFetchIntervalMs(int msec)
{
    ui->fetchIntervalSpinBox->setValue(msec);
}

int SpikeGLXSettingsDialog::fetchMaxBlockMs() const
{
    return ui->fetchBlockSpinBox->value();
}

void SpikeGLXSettingsDialog::setFetchMaxBlockMs(int msec)
{
    ui->fetchBlockSpinBox->setValue(msec);
}

SpikeGLXSettingsDialog::OverrunPolicy SpikeGLXSettingsDialog::overrunPolicy() const
{
    return static_cast<OverrunPolicy>(ui->overrunComboBox->currentData().toInt());
}

void SpikeGLXSettingsDialog::setOverrunPolicy(OverrunPolicy policy)
{
    const int idx = ui->overrunComboBox->findData(static_cast<int>(policy));
    ui->overrunComboBox->setCurrentIndex(idx < 0 ? 0 : idx);
}

SpikeGLXSettingsDialog::FetchEntry SpikeGLXSettingsDialog::fetchEntryAt(int row) const
{
    FetchEntry entry;
    if (auto *combo = qobject_cast<QComboBox *>(ui->fetchTable->cellWidget(row, 0)))
        entry.stream = combo->currentText().trimmed().toLower();
    if (auto *combo = qobject_cast<QComboBox *>(ui->fetchTable->cellWidget(row, 1)))
        entry.group = combo->currentText().trimmed().toUpper();
    if (auto *item = ui->fetchTable->item(row, 2))
        entry.channels = item->text().simplified().remove(' ');
    return entry;
}

QList<SpikeGLXSettingsDialog::FetchEntry> SpikeGLXSettingsDialog::fetchEntries() const
{
    QList<FetchEntry> entries;
    for (int row = 0; row < ui->fetchTable->rowCount(); ++row) {
        const auto e = fetchEntryAt(row);
        if (!e.stream.isEmpty())
            entries << e;
    }
    return entries;
}

void SpikeGLXSettingsDialog::appendFetchRow(const FetchEntry &entry)
{
    m_updating = true;
    const int row = ui->fetchTable->rowCount();
    ui->fetchTable->insertRow(row);

    auto *streamCombo = new QComboBox(ui->fetchTable);
    streamCombo->setEditable(true);
    streamCombo->addItems(m_knownStreams);
    streamCombo->setCurrentText(entry.stream);
    connect(streamCombo, &QComboBox::currentTextChanged, this, [this] {
        if (!m_updating)
            emit fetchEntriesChanged();
    });
    ui->fetchTable->setCellWidget(row, 0, streamCombo);

    auto *groupCombo = new QComboBox(ui->fetchTable);
    for (const auto g :
         {SglxUtils::ChanGroup::AP,
          SglxUtils::ChanGroup::LF,
          SglxUtils::ChanGroup::SY,
          SglxUtils::ChanGroup::XA,
          SglxUtils::ChanGroup::DW,
          SglxUtils::ChanGroup::MN,
          SglxUtils::ChanGroup::MA,
          SglxUtils::ChanGroup::ALL})
        groupCombo->addItem(SglxUtils::chanGroupName(g));
    const int gidx = groupCombo->findText(entry.group.toUpper());
    groupCombo->setCurrentIndex(gidx < 0 ? 0 : gidx);
    connect(groupCombo, &QComboBox::currentTextChanged, this, [this] {
        if (!m_updating)
            emit fetchEntriesChanged();
    });
    ui->fetchTable->setCellWidget(row, 1, groupCombo);

    ui->fetchTable->setItem(row, 2, new QTableWidgetItem(entry.channels));
    m_updating = false;
}

void SpikeGLXSettingsDialog::setFetchEntries(const QList<FetchEntry> &entries)
{
    m_updating = true;
    ui->fetchTable->setRowCount(0);
    m_updating = false;
    for (const auto &e : entries)
        appendFetchRow(e);
}

void SpikeGLXSettingsDialog::setRunActive(bool active)
{
    ui->connectionGroup->setEnabled(!active);
    ui->runControlGroup->setEnabled(!active);
    ui->syncGroup->setEnabled(!active);
    ui->fetchGroup->setEnabled(!active);
}

void SpikeGLXSettingsDialog::setConnectionStatus(const QString &text, bool ok)
{
    ui->connectionStatusLabel->setText(text);
    ui->connectionStatusLabel->setStyleSheet(ok ? QString() : QStringLiteral("color: #c0392b;"));
}

void SpikeGLXSettingsDialog::setStreamsInfo(const QString &text, const QStringList &streamNames)
{
    ui->streamsInfoLabel->setText(text);
    m_knownStreams = streamNames;

    // refresh the stream selectors of existing rows without losing their values
    m_updating = true;
    for (int row = 0; row < ui->fetchTable->rowCount(); ++row) {
        auto *combo = qobject_cast<QComboBox *>(ui->fetchTable->cellWidget(row, 0));
        if (!combo)
            continue;
        const auto current = combo->currentText();
        combo->clear();
        combo->addItems(m_knownStreams);
        combo->setCurrentText(current);
    }
    m_updating = false;
}
