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
#include <QMessageBox>
#include <miniscope.h>

#include "mscontrolwidget.h"

MiniscopeSettingsDialog::MiniscopeSettingsDialog(MScope::Miniscope *mscope, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MiniscopeSettingsDialog),
    m_mscope(mscope)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // Set up layout for Miniscope controls
    m_controlsLayout = new QVBoxLayout(this);
    m_controlsLayout->setMargin(2);
    m_controlsLayout->setSpacing(4);
    ui->gbDeviceCtls->setLayout(m_controlsLayout);
    m_controlsLayout->addStretch();

    // register available Miniscope types
    ui->scopeTypeComboBox->addItems(m_mscope->availableMiniscopeTypes());

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
    ui->accAlphaSpinBox->setValue(m_mscope->bgAccumulateAlpha());

    for (const auto &w : m_controls)
        w->setValue(m_mscope->controlValue(w->controlId()));
}

void MiniscopeSettingsDialog::setRunning(bool running)
{
    for (const auto &w : m_controls) {
        if (running)
            m_mscope->setControlValue(w->controlId(), w->value());
        if (w->controlId() == "frameRate")
            w->setEnabled(!running);
    }
}

void MiniscopeSettingsDialog::setDeviceType(const QString &devType)
{
    if (devType.isEmpty())
        return;
    ui->scopeTypeComboBox->setCurrentText(devType);
}

void MiniscopeSettingsDialog::setCurrentPixRangeValues(int min, int max)
{
    ui->labelScopeMin->setText(QString::number(min).rightJustified(3, '0'));
    ui->labelScopeMax->setText(QString::number(max).rightJustified(3, '0'));
}

void MiniscopeSettingsDialog::on_scopeTypeComboBox_currentIndexChanged(const QString &arg1)
{
    // clear previous controls
    for (const auto &control : m_controls)
        delete control;
    m_controls.clear();

    // load new controls
    if (!m_mscope->loadDeviceConfig(arg1)) {
        QMessageBox::critical(this,
                              "Error",
                              QString("Unable to load device configuration: %1")
                              .arg(m_mscope->lastError()));
        return;
    }

    // display widgets for new controls
    for (const auto &ctl : m_mscope->controls()) {
        const auto w = new MSControlWidget(ctl, ui->gbDeviceCtls);
        m_controlsLayout->insertWidget(0, w);
        connect(w, &MSControlWidget::valueChanged, [&](const QString &id, double value) {
            m_mscope->setControlValue(id, value);
        });
        m_controls.append(w);
    }
}

void MiniscopeSettingsDialog::on_sbCamId_valueChanged(int arg1)
{
    m_mscope->setScopeCamId(arg1);
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

void MiniscopeSettingsDialog::on_alwaysReinitCheckBox_toggled(bool checked)
{
    m_mscope->setAlwaysReinitializeDevice(checked);
}
