/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "flircamsettingsdialog.h"
#include "ui_flircamsettingsdialog.h"

FLIRCamSettingsDialog::FLIRCamSettingsDialog(FLIRCamera *camera, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FLIRCamSettingsDialog)
{
    ui->setupUi(this);
    m_camera = camera;
    setWindowIcon(QIcon(":/icons/generic-config"));

    ui->cbGamma->setChecked(false);
    ui->cbSaturation->setChecked(false);
}

FLIRCamSettingsDialog::~FLIRCamSettingsDialog()
{
    delete ui;
}

QString FLIRCamSettingsDialog::selectedCameraSerial() const
{
    return ui->cameraComboBox->currentData().toString();
}

cv::Size FLIRCamSettingsDialog::resolution() const
{
    return cv::Size(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
}

int FLIRCamSettingsDialog::framerate() const
{
    return ui->fpsSpinBox->value();
}

void FLIRCamSettingsDialog::setFramerate(int fps)
{
    ui->fpsSpinBox->setValue(fps);
}

void FLIRCamSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
}

void FLIRCamSettingsDialog::updateValues()
{
    ui->cameraComboBox->clear();
    auto cameras = m_camera->availableCameras();
    for (const auto &cameraInfo : cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }

    for (int i = 0; i < ui->cameraComboBox->count(); i++) {
        if (ui->cameraComboBox->itemData(i).toString() == m_camera->serial()) {
            ui->cameraComboBox->setCurrentIndex(i);
            break;
        }
    }

    ui->spinBoxWidth->setValue(m_camera->resolution().width);
    ui->spinBoxHeight->setValue(m_camera->resolution().height);
    ui->sbExposure->setValue(m_camera->exposureTime().count());
    ui->sbGain->setValue(m_camera->gain());

    if (m_camera->gamma() > 0)
        ui->sbGamma->setValue(m_camera->gamma());
    else
        ui->cbGamma->setChecked(false);
}

void FLIRCamSettingsDialog::on_cameraComboBox_currentIndexChanged(int)
{
    m_camera->setSerial(ui->cameraComboBox->currentData().toString());
}

void FLIRCamSettingsDialog::on_sbExposure_valueChanged(int arg1)
{
    ui->sliderExposure->setValue(arg1);
    m_camera->setExposureTime(microseconds_t(arg1));
}

void FLIRCamSettingsDialog::on_cbGamma_toggled(bool checked)
{
    if (checked)
        m_camera->setGamma(ui->sbGamma->value());
    else
        m_camera->setGamma(-1);
}

void FLIRCamSettingsDialog::on_sbGamma_valueChanged(double arg1)
{
    ui->sliderGamma->setValue(qRound(arg1));
    m_camera->setGamma(arg1);
}

void FLIRCamSettingsDialog::on_sliderGamma_valueChanged(int value)
{
    ui->sbGamma->setValue(value);
}

void FLIRCamSettingsDialog::on_sbSaturation_valueChanged(double arg1)
{
    ui->sliderSaturation->setValue(arg1);
}

void FLIRCamSettingsDialog::on_sliderSaturation_valueChanged(int value)
{
    ui->sbSaturation->setValue(value);
}

void FLIRCamSettingsDialog::on_sbGain_valueChanged(double arg1)
{
    ui->sliderGain->setValue(arg1);
    m_camera->setGain(arg1);
}

void FLIRCamSettingsDialog::on_sliderGain_valueChanged(int value)
{
    ui->sbGain->setValue(value);
}
