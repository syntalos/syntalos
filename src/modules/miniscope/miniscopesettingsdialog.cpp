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
    ui->sbExposure->setValue(static_cast<int>(m_mscope->exposure()));
    ui->sbExcitation->setValue(m_mscope->excitation());
    ui->sbGain->setValue(static_cast<int>(m_mscope->gain()));
    ui->accAlphaSpinBox->setValue(m_mscope->bgAccumulateAlpha());

    // ensure codecs and container UI is aligned with the MiniScope settings
    ui->codecComboBox->setCurrentIndex(0);
    this->on_codecComboBox_currentIndexChanged(ui->codecComboBox->currentText());
    ui->containerComboBox->setCurrentIndex(0);
    this->on_containerComboBox_currentIndexChanged(ui->containerComboBox->currentText());
    ui->losslessCheckBox->setChecked(true);
    on_sliceIntervalSpinBox_valueChanged(ui->sliceIntervalSpinBox->value());
}

MiniscopeSettingsDialog::~MiniscopeSettingsDialog()
{
    delete ui;
}

void MiniscopeSettingsDialog::setRecName(const QString &value)
{
    m_recName = value.simplified().replace(" ", "_");
}

QString MiniscopeSettingsDialog::recName() const
{
    return m_recName;
}

void MiniscopeSettingsDialog::setLossless(bool lossless)
{
    ui->losslessCheckBox->setChecked(lossless);
}

bool MiniscopeSettingsDialog::isLossless() const
{
    return ui->losslessCheckBox->isChecked();
}

void MiniscopeSettingsDialog::setSliceInterval(uint interval)
{
    ui->sliceIntervalSpinBox->setValue(static_cast<int>(interval));
}

uint MiniscopeSettingsDialog::sliceInterval() const
{
    return static_cast<uint>(ui->sliceIntervalSpinBox->value());
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

void MiniscopeSettingsDialog::on_losslessCheckBox_toggled(bool checked)
{
    m_mscope->setRecordLossless(checked);
}

void MiniscopeSettingsDialog::on_containerComboBox_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "MKV")
        m_mscope->setVideoContainer(MScope::VideoContainer::Matroska);
    else if (arg1 == "AVI")
        m_mscope->setVideoContainer(MScope::VideoContainer::AVI);
    else
        qCritical() << "Unknown video container option selected:" << arg1;
}

void MiniscopeSettingsDialog::on_codecComboBox_currentIndexChanged(const QString &arg1)
{
    // reset state of lossless infobox
    ui->losslessCheckBox->setEnabled(true);
    ui->losslessLabel->setEnabled(true);
    ui->losslessCheckBox->setChecked(m_mscope->recordLossless());
    ui->containerComboBox->setEnabled(true);

    if (arg1 == "AV1") {
        m_mscope->setVideoCodec(MScope::VideoCodec::AV1);

    } else if (arg1 == "FFV1") {
        m_mscope->setVideoCodec(MScope::VideoCodec::FFV1);

        // FFV1 is always lossless
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(true);

    } else if (arg1 == "VP9") {
        m_mscope->setVideoCodec(MScope::VideoCodec::VP9);

    } else if (arg1 == "H.265") {
        m_mscope->setVideoCodec(MScope::VideoCodec::H265);

        // H.256 only works with MKV and MP4 containers, select MKV by default
        ui->containerComboBox->setCurrentIndex(0);
        ui->containerComboBox->setEnabled(false);

    } else if (arg1 == "MPEG-4") {
        m_mscope->setVideoCodec(MScope::VideoCodec::MPEG4);

        // MPEG-4 can't do lossless encoding
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(false);

    } else if (arg1 == "None") {
        m_mscope->setVideoCodec(MScope::VideoCodec::Raw);

        // Raw is always lossless
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(true);

        // Raw RGB only works with AVI containers
        ui->containerComboBox->setCurrentIndex(1);
        ui->containerComboBox->setEnabled(false);

    } else
        qCritical() << "Unknown video codec option selected:" << arg1;
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

void MiniscopeSettingsDialog::on_sliceIntervalSpinBox_valueChanged(int arg1)
{
    m_mscope->setRecordingSliceInterval(static_cast<uint>(arg1));
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

void MiniscopeSettingsDialog::on_recNameLineEdit_textChanged(const QString &arg1)
{
    setRecName(arg1);
    ui->recNameLineEdit->setText(recName());
}
