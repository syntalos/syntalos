/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "globalconfigdialog.h"
#include "ui_globalconfigdialog.h"

#include <array>
#include <tuple>
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QToolButton>

#include "appstyle.h"
#include "datactl/priv/rtkit.h"
#include "soundcueplayer.h"

using namespace Syntalos;

GlobalConfigDialog::GlobalConfigDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::GlobalConfigDialog),
      m_acceptChanges(false)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Syntalos Settings"));
    setWindowModality(Qt::WindowModal);

    m_gc = new GlobalConfig(this);

    RtKit rtkit;

    setupPageStyles();

    // ensure we always show the first page when opening
    connect(ui->pageList, &QListWidget::currentRowChanged, ui->pageStack, &QStackedWidget::setCurrentIndex);
    ui->pageList->setCurrentRow(0);

    // general section
    ui->colorModeComboBox->clear();
    ui->colorModeComboBox->addItem("System Default", Syntalos::colorModeToString(ColorMode::SYSTEM));
    if (darkColorSchemeAvailable()) {
        ui->colorModeComboBox->addItem("Bright Colors", Syntalos::colorModeToString(ColorMode::BRIGHT));
        ui->colorModeComboBox->addItem("Dark Colors", Syntalos::colorModeToString(ColorMode::DARK));
        ui->colorModeComboBox->setCurrentIndex(static_cast<int>(m_gc->appColorMode()));
    }
    ui->cbEmergencyOOMStop->setChecked(m_gc->emergencyOOMStop());
    ui->sbMinFreeDiskSpace->setValue(m_gc->minFreeDiskSpaceGB());
    ui->sbDiskSpaceWarnMinutes->setValue(m_gc->diskSpaceWarnMinutes());
    ui->cbNetEnabled->setChecked(m_gc->netControlEnabled());
    ui->sbNetControlPort->setValue(m_gc->netControlPort());
    ui->sbNetFeedbackPort->setValue(m_gc->netFeedbackPort());
    ui->leNetHost->setText(m_gc->netControlHost());
    ui->leNetInstanceId->setText(m_gc->instanceId());
    updateNetControlWidgetsState();

    // sounds section
    setupSoundCueControls();

    // advanced section
    ui->defaultNicenessSpinBox->setMaximum(20);
    ui->defaultNicenessSpinBox->setMinimum(rtkit.queryMinNiceLevel());
    ui->defaultNicenessSpinBox->setValue(m_gc->defaultThreadNice());

    ui->defaultRTPrioSpinBox->setMaximum(rtkit.queryMaxRealtimePriority());
    ui->defaultRTPrioSpinBox->setMinimum(1);
    ui->defaultRTPrioSpinBox->setValue(m_gc->defaultRTThreadPriority());

    connect(ui->lblCaptionCpuAffinityWarning, &QLabel::linkActivated, this, &GlobalConfigDialog::showCpuAffinityInfo);
    ui->explicitCoreAffinitiesCheckBox->setChecked(m_gc->explicitCoreAffinities());

    // devel section
    ui->cbDisplayDevModules->setChecked(m_gc->showDevelModules());
    ui->cbSaveDiagnostic->setChecked(m_gc->saveExperimentDiagnostics());
    ui->cbPythonVenvForScripts->setChecked(m_gc->useVenvForPyScript());
    updateCreateDevDirButtonState();

    // we can accept user changes now!
    m_acceptChanges = true;
}

GlobalConfigDialog::~GlobalConfigDialog()
{
    delete ui;
}

void GlobalConfigDialog::setupPageStyles()
{
    // give the sidebar entries some room, similar to system settings panels
    const auto itemHeight = qMax(ui->pageList->iconSize().height(), ui->pageList->fontMetrics().height()) + 14;
    for (int i = 0; i < ui->pageList->count(); ++i)
        ui->pageList->item(i)->setSizeHint(QSize(0, itemHeight));

    // derive sizes from the dialog font, so font sizes from the .ui file are not scaled
    // a second time and the labels still follow the user's system font size
    const auto baseFont = font();
    for (auto label : findChildren<QLabel *>()) {
        const auto name = label->objectName();
        auto font = baseFont;
        if (name.startsWith(QStringLiteral("lblPageTitle"))) {
            font.setPointSizeF(font.pointSizeF() * 1.4);
            font.setBold(true);
        } else if (name.startsWith(QStringLiteral("lblSection"))) {
            font.setBold(true);
        } else if (name.startsWith(QStringLiteral("lblCaption"))) {
            font.setPointSizeF(font.pointSizeF() * 0.9);
        } else {
            continue;
        }
        label->setFont(font);
    }

    updateCaptionColors();
}

void GlobalConfigDialog::updateCaptionColors()
{
    // captions use a dimmed variant of the regular text color, which we derive from the current
    // palette so it stays readable when the color scheme is changed while the dialog is open
    const auto &dlgPal = palette();
    for (auto label : findChildren<QLabel *>()) {
        if (!label->objectName().startsWith(QStringLiteral("lblCaption")))
            continue;
        auto pal = label->palette();
        for (const auto group : {QPalette::Active, QPalette::Inactive}) {
            auto dimColor = dlgPal.color(group, QPalette::WindowText);
            dimColor.setAlphaF(0.7);
            pal.setColor(group, QPalette::WindowText, dimColor);
        }
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dlgPal.color(QPalette::Disabled, QPalette::WindowText));
        label->setPalette(pal);
    }
}

void GlobalConfigDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        updateCaptionColors();
}

void GlobalConfigDialog::updateNetControlWidgetsState()
{
    const auto enabled = ui->cbNetEnabled->isChecked();
    const std::array<QWidget *, 12> netWidgets = {
        ui->portLabel,
        ui->sbNetControlPort,
        ui->lblCaptionNetControlPort,
        ui->lblNetFeedbackPort,
        ui->sbNetFeedbackPort,
        ui->lblCaptionNetFeedbackPort,
        ui->lblNetHost,
        ui->leNetHost,
        ui->lblCaptionNetHost,
        ui->lblNetInstanceId,
        ui->leNetInstanceId,
        ui->lblCaptionNetInstanceId,
    };
    for (auto widget : netWidgets)
        widget->setEnabled(enabled);
}

void GlobalConfigDialog::on_colorModeComboBox_currentIndexChanged(int index)
{
    if (!m_acceptChanges)
        return;

    if (index == 0 && m_gc->appColorMode() != Syntalos::ColorMode::SYSTEM)
        QMessageBox::information(
            this,
            "Restart may be required",
            "You may need to restart Syntalos for this style change to take effect.");

    m_gc->setAppColorMode(Syntalos::colorModeFromString(ui->colorModeComboBox->currentData().toString()));
    Q_EMIT defaultColorSchemeChanged();
}

void GlobalConfigDialog::on_cbEmergencyOOMStop_toggled(bool checked)
{
    if (m_acceptChanges)
        m_gc->setEmergencyOOMStop(checked);
}

void GlobalConfigDialog::on_sbMinFreeDiskSpace_valueChanged(int value)
{
    if (m_acceptChanges)
        m_gc->setMinFreeDiskSpaceGB(value);
}

void GlobalConfigDialog::on_sbDiskSpaceWarnMinutes_valueChanged(int value)
{
    if (m_acceptChanges)
        m_gc->setDiskSpaceWarnMinutes(value);
}

void GlobalConfigDialog::on_cbNetEnabled_toggled(bool checked)
{
    updateNetControlWidgetsState();
    if (m_acceptChanges)
        m_gc->setNetControlEnabled(checked);
}

void GlobalConfigDialog::on_sbNetControlPort_valueChanged(int arg1)
{
    if (m_acceptChanges)
        m_gc->setNetControlPort(arg1);
}

void GlobalConfigDialog::on_sbNetFeedbackPort_valueChanged(int arg1)
{
    if (m_acceptChanges)
        m_gc->setNetFeedbackPort(arg1);
}

void GlobalConfigDialog::on_leNetHost_textEdited(const QString &text)
{
    if (m_acceptChanges)
        m_gc->setNetControlHost(text);
}

void GlobalConfigDialog::on_leNetInstanceId_textEdited(const QString &text)
{
    if (m_acceptChanges)
        m_gc->setInstanceId(text);
}

void GlobalConfigDialog::setupSoundCueControls()
{
    m_soundPlayer = new SoundCuePlayer(this);
    connect(m_soundPlayer, &SoundCuePlayer::outputDevicesChanged, this, &GlobalConfigDialog::refreshSoundDeviceList);

    refreshSoundDeviceList();
    ui->slSoundVolume->setValue(m_gc->soundVolumePercent());
    ui->lblSoundVolume->setText(QStringLiteral("%1 %").arg(m_gc->soundVolumePercent()));

    const std::array<std::tuple<SoundCue, QCheckBox *, QToolButton *>, 5> cueWidgets = {
        std::make_tuple(SoundCue::RunStarted, ui->cbCueRunStarted, ui->btnCueRunStarted),
        std::make_tuple(SoundCue::RunFinishedSuccess, ui->cbCueRunSuccess, ui->btnCueRunSuccess),
        std::make_tuple(SoundCue::RunFinishedFailure, ui->cbCueRunFailure, ui->btnCueRunFailure),
        std::make_tuple(SoundCue::ResourceWarning, ui->cbCueResourceWarning, ui->btnCueResourceWarning),
        std::make_tuple(SoundCue::ModuleFailed, ui->cbCueModuleFailed, ui->btnCueModuleFailed),
    };
    for (const auto &[cue, checkBox, previewBtn] : cueWidgets) {
        checkBox->setChecked(m_gc->soundCueEnabled(cue));
        connect(checkBox, &QCheckBox::toggled, this, [this, cue](bool checked) {
            if (m_acceptChanges)
                m_gc->setSoundCueEnabled(cue, checked);
        });
        previewBtn->setToolTip(QStringLiteral("Preview \"%1\"").arg(soundCueDisplayName(cue)));
        connect(previewBtn, &QToolButton::clicked, this, [this, cue]() {
            m_soundPlayer->playPreview(cue);
        });
    }
}

void GlobalConfigDialog::refreshSoundDeviceList()
{
    // we are only refreshing the view here, don't write anything back
    const QSignalBlocker blocker(ui->cbSoundDevice);
    ui->cbSoundDevice->clear();

    // the plain device name is kept in a separate role, so we can store it for display
    // even if the device is not connected the next time the dialog is opened
    constexpr int NameRole = Qt::UserRole + 1;
    ui->cbSoundDevice->addItem(QStringLiteral("System Default"), QByteArray());
    ui->cbSoundDevice->setItemData(0, QString(), NameRole);

    const auto wantedId = m_gc->soundOutputDeviceId();
    int selectedIdx = 0;
    for (const auto &dev : m_soundPlayer->availableOutputDevices()) {
        ui->cbSoundDevice->addItem(dev.description(), dev.id());
        ui->cbSoundDevice->setItemData(ui->cbSoundDevice->count() - 1, dev.description(), NameRole);
        if (!wantedId.isEmpty() && dev.id() == wantedId)
            selectedIdx = ui->cbSoundDevice->count() - 1;
    }

    // keep the user's choice visible (and selectable) even if the device is currently absent
    if (!wantedId.isEmpty() && selectedIdx == 0) {
        const auto name = m_gc->soundOutputDeviceName();
        ui->cbSoundDevice->addItem(QStringLiteral("%1 (not connected)").arg(name), wantedId);
        ui->cbSoundDevice->setItemData(ui->cbSoundDevice->count() - 1, name, NameRole);
        selectedIdx = ui->cbSoundDevice->count() - 1;
    }

    ui->cbSoundDevice->setCurrentIndex(selectedIdx);
}

void GlobalConfigDialog::on_cbSoundDevice_currentIndexChanged(int index)
{
    if (!m_acceptChanges || index < 0)
        return;

    m_gc->setSoundOutputDevice(
        ui->cbSoundDevice->itemData(index).toByteArray(),
        ui->cbSoundDevice->itemData(index, Qt::UserRole + 1).toString());
    m_soundPlayer->reloadSettings();
}

void GlobalConfigDialog::on_slSoundVolume_valueChanged(int value)
{
    ui->lblSoundVolume->setText(QStringLiteral("%1 %").arg(value));
    if (!m_acceptChanges)
        return;

    m_gc->setSoundVolumePercent(value);
    m_soundPlayer->reloadSettings();
}

void GlobalConfigDialog::on_defaultNicenessSpinBox_valueChanged(int arg1)
{
    if (m_acceptChanges)
        m_gc->setDefaultThreadNice(arg1);
}

void GlobalConfigDialog::on_defaultRTPrioSpinBox_valueChanged(int arg1)
{
    if (m_acceptChanges)
        m_gc->setDefaultRTThreadPriority(arg1);
}

void GlobalConfigDialog::on_explicitCoreAffinitiesCheckBox_toggled(bool checked)
{
    if (m_acceptChanges)
        m_gc->setExplicitCoreAffinities(checked);
}

void GlobalConfigDialog::showCpuAffinityInfo()
{
    QMessageBox::information(
        this,
        QStringLiteral("Information on explicit CPU affinity"),
        QStringLiteral(
            "<html>"
            "By selecting the explicit CPU affinity option, threads of individual modules are bound to "
            "specific CPU cores by the operating system. This can greatly improve latency in some scenarios, "
            "and may occasionally even improve performance due to better CPU cache coherency.<br/><br/>"
            "<b>However</b> using this option can also massively degrade performance, as module threads and "
            "their descendants can not be moved freely between otherwise idle CPU cores anymore. So, while getting "
            "more consistent latencies, you may get much reduced performance.<br/><br/>"
            "The affinity selector does not know about individual module's runtime CPU utilization (yet...), so its "
            "guesses may be wrong and lead to suboptimal results.<br/><br/>"
            "Therefore, using this option is not recommended for most users - you can safely give it a try though and "
            "see if it helps your individual setup's performance or latency."));
}

void GlobalConfigDialog::on_cbDisplayDevModules_toggled(bool checked)
{
    if (m_acceptChanges)
        m_gc->setShowDevelModules(checked);
}

void GlobalConfigDialog::on_cbSaveDiagnostic_toggled(bool checked)
{
    if (m_acceptChanges)
        m_gc->setSaveExperimentDiagnostics(checked);
}

void GlobalConfigDialog::updateCreateDevDirButtonState()
{
    QDir homeDevDir(m_gc->homeDevelDir());
    auto venvLinkPath = homeDevDir.filePath("venv");
    QFileInfo fiVenvLink(venvLinkPath);

    ui->btnCreateDevDir->setChecked(fiVenvLink.exists());
    if (fiVenvLink.exists())
        ui->btnCreateDevDir->setText("Directory exists (click to update)");
    else
        ui->btnCreateDevDir->setText("Create directory in home directory");
    ui->lblCaptionCreateDevDir->setText(
        QStringLiteral("Links to the Python virtual environments and user modules in <code>%1</code>.")
            .arg(m_gc->homeDevelDir().toHtmlEscaped()));
}

void GlobalConfigDialog::on_btnCreateDevDir_clicked()
{
    QDir homeDevDir(m_gc->homeDevelDir());
    QDir().mkdir(homeDevDir.absolutePath());

    auto logger = getLogger("main");

    // create link to venvs directory
    auto venvLinkPath = homeDevDir.filePath("venv");
    QFileInfo fiVenvLink(venvLinkPath);
    if (!fiVenvLink.exists() || fiVenvLink.isSymLink()) {
        QFile::remove(venvLinkPath);
        QDir().mkpath(m_gc->virtualEnvDir());
        if (!QFile::link(m_gc->virtualEnvDir(), venvLinkPath))
            LOG_WARNING(logger, "Failed to create symlink from {} to {}", m_gc->virtualEnvDir(), venvLinkPath);
    }

    // create link to user modules directory
    auto modsLinkPath = homeDevDir.filePath("modules");
    QFileInfo fiModsLink(modsLinkPath);
    if (!fiModsLink.exists() || fiModsLink.isSymLink()) {
        QFile::remove(modsLinkPath);
        QDir().mkpath(m_gc->userModulesDir());
        if (!QFile::link(m_gc->userModulesDir(), modsLinkPath))
            LOG_WARNING(logger, "Failed to create symlink from {} to {}", m_gc->userModulesDir(), modsLinkPath);
    }

    updateCreateDevDirButtonState();
}

void GlobalConfigDialog::on_cbPythonVenvForScripts_toggled(bool checked)
{
    if (m_acceptChanges)
        m_gc->setUseVenvForPyScript(checked);
}
