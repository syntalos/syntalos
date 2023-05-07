/*
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMessageBox>

#include "rtkit.h"
#include "appstyle.h"

using namespace Syntalos;

GlobalConfigDialog::GlobalConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GlobalConfigDialog),
    m_acceptChanges(false)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Syntalos Settings"));
    setWindowModality(Qt::WindowModal);

    m_gc = new GlobalConfig(this);

    RtKit rtkit;

    // ensure we always show the first page when opening
    ui->tabWidget->setCurrentIndex(0);

    // general section
    ui->colorModeComboBox->clear();
    ui->colorModeComboBox->addItem("System Default", Syntalos::colorModeToString(ColorMode::SYSTEM));
    if (darkColorSchemeAvailable()) {
        ui->colorModeComboBox->addItem("Bright Colors", Syntalos::colorModeToString(ColorMode::BRIGHT));
        ui->colorModeComboBox->addItem("Dark Colors", Syntalos::colorModeToString(ColorMode::DARK));
        ui->colorModeComboBox->setCurrentIndex(static_cast<int>(m_gc->appColorMode()));
    }
    ui->cbEmergencyOOMStop->setChecked(m_gc->emergencyOOMStop());

    // advanced section
    ui->defaultNicenessSpinBox->setMaximum(20);
    ui->defaultNicenessSpinBox->setMinimum(rtkit.queryMinNiceLevel());
    ui->defaultNicenessSpinBox->setValue(m_gc->defaultThreadNice());

    ui->defaultRTPrioSpinBox->setMaximum(rtkit.queryMaxRealtimePriority());
    ui->defaultRTPrioSpinBox->setMinimum(1);
    ui->defaultRTPrioSpinBox->setValue(m_gc->defaultRTThreadPriority());

    ui->cpuAffinityWarnButton->setVisible(false);
    ui->explicitCoreAffinitiesCheckBox->setChecked(m_gc->explicitCoreAffinities());

    // devel section
    ui->cbDisplayDevModules->setChecked(m_gc->showDevelModules());
    ui->cbSaveDiagnostic->setChecked(m_gc->saveExperimentDiagnostics());
    ui->cbPythonCreateVEnvLink->setChecked(m_gc->createVEnvUserLink());

    // we can accept user changes now!
    m_acceptChanges = true;
}

GlobalConfigDialog::~GlobalConfigDialog()
{
    delete ui;
}

void GlobalConfigDialog::on_colorModeComboBox_currentIndexChanged(int index)
{
    if (!m_acceptChanges)
        return;

    if (index == 0 && m_gc->appColorMode() != Syntalos::ColorMode::SYSTEM)
        QMessageBox::information(this, "Restart may be required",
                                 "You may need to restart Syntalos for this style change to take effect.");

    m_gc->setAppColorMode(Syntalos::colorModeFromString(ui->colorModeComboBox->currentData().toString()));
    Q_EMIT defaultColorSchemeChanged();
}

void GlobalConfigDialog::on_emergencyOOMStopCheckBox_toggled(bool checked)
{
    if (m_acceptChanges) m_gc->setEmergencyOOMStop(checked);
}

void GlobalConfigDialog::on_defaultNicenessSpinBox_valueChanged(int arg1)
{
    if (m_acceptChanges) m_gc->setDefaultThreadNice(arg1);
}

void GlobalConfigDialog::on_defaultRTPrioSpinBox_valueChanged(int arg1)
{
    if (m_acceptChanges) m_gc->setDefaultRTThreadPriority(arg1);
}

void GlobalConfigDialog::on_explicitCoreAffinitiesCheckBox_toggled(bool checked)
{
    if (m_acceptChanges) m_gc->setExplicitCoreAffinities(checked);
    ui->cpuAffinityWarnButton->setVisible(checked);
}

void GlobalConfigDialog::on_cpuAffinityWarnButton_clicked()
{
    QMessageBox::information(this,
                             QStringLiteral("Information on explicit CPU affinity"),
                             QStringLiteral("<html>"
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
    if (m_acceptChanges) m_gc->setShowDevelModules(checked);
}

void GlobalConfigDialog::on_cbSaveDiagnostic_toggled(bool checked)
{
    if (m_acceptChanges) m_gc->setSaveExperimentDiagnostics(checked);
}

void GlobalConfigDialog::on_cbPythonCreateVEnvLink_toggled(bool checked)
{
    if (m_acceptChanges) m_gc->setCreateVEnvUserLink(checked);
}
