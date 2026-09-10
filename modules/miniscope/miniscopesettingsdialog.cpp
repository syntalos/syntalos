/**
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "miniscopesettingsdialog.h"
#include "ui_miniscopesettingsdialog.h"

#include <QMessageBox>
#include <QVariant>
#include <miniscope/miniscope.h>

#include "mscontrolwidget.h"
#include "utils/misc.h"

using namespace Syntalos;
using namespace Miniscope;

MiniscopeSettingsDialog::MiniscopeSettingsDialog(Miniscope::Miniscope *mscope, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::MiniscopeSettingsDialog),
      m_initDone(false),
      m_mscope(mscope)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // Set up layout for Miniscope controls
    m_controlsLayout = new QVBoxLayout(ui->gbDeviceCtls);
    m_controlsLayout->setContentsMargins(2, 2, 2, 2);
    m_controlsLayout->setSpacing(4);
    ui->gbDeviceCtls->setLayout(m_controlsLayout);
    m_controlsLayout->addStretch();

    // register available Miniscope types
    for (const auto &s : m_mscope->availableDeviceTypes())
        ui->deviceTypeCB->addItem(qstr(s));

    // register available view modes
    ui->viewModeCB->addItem(QStringLiteral("Raw Data"), QVariant::fromValue(DisplayMode::RawFrames));
    ui->viewModeCB->addItem(QStringLiteral("F - F₀"), QVariant::fromValue(DisplayMode::BackgroundDiff));

    // all settings options are loaded now
    m_initDone = true;

    // display default values
    readCurrentValues();
}

MiniscopeSettingsDialog::~MiniscopeSettingsDialog()
{
    delete ui;
}

void MiniscopeSettingsDialog::readCurrentValues()
{
    ui->sbCamId->setValue(m_mscope->scopeCamId());
    updateCurrentDeviceName();
    ui->accAlphaSpinBox->setValue(m_mscope->bgAccumulateAlpha());
    setDeviceType(qstr(m_mscope->deviceType()));

    for (const auto &w : m_controls)
        w->setValue(m_mscope->controlValue(w->controlId()));
}

void MiniscopeSettingsDialog::applyValues()
{
    for (const auto &w : m_controls) {
        if (w->controlId() == "frameRate")
            continue;
        m_mscope->setControlValue(w->controlId(), w->value());
    }
}

void MiniscopeSettingsDialog::setRunning(bool running)
{
    m_mscope->setBgAccumulateAlpha(ui->accAlphaSpinBox->value());
    for (const auto &w : m_controls) {
        if (running)
            m_mscope->setControlValue(w->controlId(), w->value());
        if (w->controlId() == "frameRate")
            w->setEnabled(!running);
    }
}

void MiniscopeSettingsDialog::setDeviceType(const QString &devType)
{
    auto selectedDevice = devType.toLower();
    if (selectedDevice.isEmpty())
        selectedDevice = QStringLiteral("Miniscope_V4_BNO").toLower();

    for (int i = 0; i < ui->deviceTypeCB->count(); ++i) {
        if (ui->deviceTypeCB->itemText(i).toLower() == selectedDevice) {
            ui->deviceTypeCB->setCurrentIndex(i);
            break;
        }
    }
}

void MiniscopeSettingsDialog::setCurrentPixRangeValues(int min, int max)
{
    ui->labelScopeMin->setText(QString::number(min).rightJustified(3, '0'));
    ui->labelScopeMax->setText(QString::number(max).rightJustified(3, '0'));
}

void MiniscopeSettingsDialog::setOrientationIndicatorVisible(bool visible)
{
    ui->orientationIndicatorCheckBox->setChecked(visible);
    on_orientationIndicatorCheckBox_toggled(visible);
}

void MiniscopeSettingsDialog::updateCurrentDeviceName()
{
    on_sbCamId_valueChanged(ui->sbCamId->value());
}

void MiniscopeSettingsDialog::on_deviceTypeCB_currentIndexChanged(int index)
{
    if (!m_initDone)
        return;

    // clear previous controls
    for (const auto &control : m_controls)
        delete control;
    m_controls.clear();

    // load new controls
    if (const auto res = m_mscope->loadDeviceConfig(ui->deviceTypeCB->currentText().toStdString()); !res) {
        QMessageBox::critical(this, "Error", QString("Unable to load device configuration: %1").arg(qstr(res.error())));
        return;
    }

    // display widgets for new controls
    ui->orientationIndicatorLabel->setEnabled(m_mscope->hasHeadOrientationSupport());
    ui->orientationIndicatorCheckBox->setEnabled(m_mscope->hasHeadOrientationSupport());
    for (const auto &ctl : m_mscope->controls()) {
        const auto w = new MSControlWidget(ctl, ui->gbDeviceCtls);
        m_controlsLayout->insertWidget(0, w);
        connect(w, &MSControlWidget::valueChanged, [&](const QString &id, double value) {
            m_mscope->setControlValue(id.toStdString(), value);
        });
        m_controls.append(w);
    }
}

void MiniscopeSettingsDialog::on_sbCamId_valueChanged(int arg1)
{
    if (m_mscope->isConnected())
        m_mscope->disconnect();
    m_mscope->setScopeCamId(arg1);
    auto vdevName = videoDeviceNameFromId(arg1);
    if (vdevName.empty())
        vdevName = "unknown or invalid";
    ui->camInfoLabel->setText(QStringLiteral("➞ %1").arg(qstr(vdevName)));
}

void MiniscopeSettingsDialog::on_cbExtRecTrigger_toggled(bool checked)
{
    m_mscope->setExternalRecordTrigger(checked);
}

void MiniscopeSettingsDialog::on_sbDisplayMin_valueChanged(int arg1)
{
    m_mscope->setMinFluorDisplay(arg1);
    ui->btnDispLimitsReset->setEnabled(true);
}

void MiniscopeSettingsDialog::on_sbDisplayMax_valueChanged(int arg1)
{
    m_mscope->setMaxFluorDisplay(arg1);
    ui->btnDispLimitsReset->setEnabled(true);
}

void MiniscopeSettingsDialog::on_btnDispLimitsReset_clicked()
{
    ui->sbDisplayMin->setValue(ui->sbDisplayMin->minimum());
    ui->sbDisplayMax->setValue(ui->sbDisplayMax->maximum());
    ui->btnDispLimitsReset->setEnabled(false);
}

void MiniscopeSettingsDialog::on_viewModeCB_currentIndexChanged(int)
{
    if (!m_initDone)
        return;
    const auto mode = ui->viewModeCB->currentData().value<DisplayMode>();

    ui->accAlphaSpinBox->setEnabled(mode == DisplayMode::BackgroundDiff);
    ui->accumulateAlphaLabel->setEnabled(mode == DisplayMode::BackgroundDiff);

    m_mscope->setDisplayMode(mode);
}

void MiniscopeSettingsDialog::on_accAlphaSpinBox_valueChanged(double arg1)
{
    m_mscope->setBgAccumulateAlpha(arg1);
}

void MiniscopeSettingsDialog::on_orientationIndicatorCheckBox_toggled(bool checked)
{
    m_mscope->setBNOIndicatorVisible(checked);
}
