/**
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "miniscopesettingsdialog.h"
#include "ui_miniscopesettingsdialog.h"

#include <QDebug>
#include <QVariant>
#include <miniscope.h>

MiniscopeSettingsDialog::MiniscopeSettingsDialog(MScope::MiniScope *mscope, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MiniscopeSettingsDialog),
    m_mscope(mscope)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // don't display log by default
    ui->logTextList->setVisible(false);

    // display default values
    updateValues();
}

MiniscopeSettingsDialog::~MiniscopeSettingsDialog()
{
    delete ui;
}

void MiniscopeSettingsDialog::updateValues()
{
    ui->sbCamId->setValue(m_mscope->scopeCamId());
    ui->fpsSpinBox->setValue(m_mscope->fps());
    ui->sbExposure->setValue(static_cast<int>(m_mscope->exposure()));
    ui->sbExcitation->setValue(m_mscope->excitation());
    ui->sbGain->setValue(static_cast<int>(m_mscope->gain()));
    ui->accAlphaSpinBox->setValue(m_mscope->bgAccumulateAlpha());
}

void MiniscopeSettingsDialog::on_sbExposure_valueChanged(int arg1)
{
    m_mscope->setExposure(arg1);
}

void MiniscopeSettingsDialog::on_sbExcitation_valueChanged(double arg1)
{
    arg1 = round(arg1 * 100) / 100;
    m_mscope->setExcitation(arg1);

    double intpart;
    if (std::modf(arg1, &intpart) == 0.0)
        ui->dialExcitation->setValue(static_cast<int>(arg1));
}

void MiniscopeSettingsDialog::on_dialExcitation_valueChanged(int value)
{
    ui->sbExcitation->setValue(value);
}

void MiniscopeSettingsDialog::on_sbGain_valueChanged(int arg1)
{
    m_mscope->setGain(arg1);
}

void MiniscopeSettingsDialog::on_cbExtRecTrigger_toggled(bool checked)
{
    m_mscope->setExternalRecordTrigger(checked);
}

void MiniscopeSettingsDialog::on_sbDisplayMin_valueChanged(int arg1)
{
    m_mscope->setMinFluorDisplay(arg1);
}

void MiniscopeSettingsDialog::on_sbDisplayMax_valueChanged(int arg1)
{
    m_mscope->setMaxFluorDisplay(arg1);
}

void MiniscopeSettingsDialog::on_fpsSpinBox_valueChanged(int arg1)
{
    m_mscope->setFps(static_cast<uint>(arg1));
}

void MiniscopeSettingsDialog::on_bgSubstCheckBox_toggled(bool checked)
{
    if (checked) {
        ui->bgDivCheckBox->setChecked(false);
        m_mscope->setDisplayBgDiffMethod(MScope::BackgroundDiffMethod::Subtraction);
    } else {
        m_mscope->setDisplayBgDiffMethod(MScope::BackgroundDiffMethod::None);
    }
}

void MiniscopeSettingsDialog::on_bgDivCheckBox_toggled(bool checked)
{
    if (checked) {
        ui->bgSubstCheckBox->setChecked(false);
        m_mscope->setDisplayBgDiffMethod(MScope::BackgroundDiffMethod::Division);
    } else {
        m_mscope->setDisplayBgDiffMethod(MScope::BackgroundDiffMethod::None);
    }
}

void MiniscopeSettingsDialog::on_accAlphaSpinBox_valueChanged(double arg1)
{
    m_mscope->setBgAccumulateAlpha(arg1);
}

void MiniscopeSettingsDialog::on_sbCamId_valueChanged(int arg1)
{
    m_mscope->setScopeCamId(arg1);
}
