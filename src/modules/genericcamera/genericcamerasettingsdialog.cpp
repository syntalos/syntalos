/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "genericcamerasettingsdialog.h"
#include "ui_genericcamerasettingsdialog.h"

GenericCameraSettingsDialog::GenericCameraSettingsDialog(Camera *camera, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenericCameraSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));
    m_camera = camera;

    auto cameras = Camera::availableCameras();
    for (const auto &cameraInfo : cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }

    updateValues();
}

GenericCameraSettingsDialog::~GenericCameraSettingsDialog()
{
    delete ui;
}

QVariant GenericCameraSettingsDialog::selectedCamera() const
{
    return ui->cameraComboBox->currentData();
}

cv::Size GenericCameraSettingsDialog::resolution() const
{
    return cv::Size(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
}

int GenericCameraSettingsDialog::framerate() const
{
    return ui->fpsSpinBox->value();
}

void GenericCameraSettingsDialog::setFramerate(int fps)
{
    ui->fpsSpinBox->setValue(fps);
}

void GenericCameraSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
}

void GenericCameraSettingsDialog::updateValues()
{
    ui->cameraComboBox->clear();
    auto cameras = Camera::availableCameras();
    for (const auto &cameraInfo : cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }

    for (int i = 0; i < ui->cameraComboBox->count(); i++) {
        if (ui->cameraComboBox->itemData(i).toInt() == m_camera->camId()) {
            ui->cameraComboBox->setCurrentIndex(i);
            break;
        }
    }

    ui->spinBoxWidth->setValue(m_camera->resolution().width);
    ui->spinBoxHeight->setValue(m_camera->resolution().height);
    ui->sbExposure->setValue(m_camera->exposure());
    ui->sbBrightness->setValue(m_camera->brightness());
    ui->sbContrast->setValue(m_camera->contrast());
    ui->sbSaturation->setValue(m_camera->saturation());
    ui->sbHue->setValue(m_camera->hue());
    ui->sbGain->setValue(m_camera->gain());
}

void GenericCameraSettingsDialog::on_cameraComboBox_currentIndexChanged(int)
{
    m_camera->setCamId(ui->cameraComboBox->currentData().toInt());
}

void GenericCameraSettingsDialog::on_sbExposure_valueChanged(double arg1)
{
    m_camera->setExposure(arg1);
    ui->sliderExposure->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderExposure_valueChanged(int value)
{
    ui->sbExposure->setValue(value);
}

void GenericCameraSettingsDialog::on_sbBrightness_valueChanged(double arg1)
{
    m_camera->setBrightness(arg1);
    ui->sliderBrightness->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderBrightness_valueChanged(int value)
{
    ui->sbBrightness->setValue(value);
}

void GenericCameraSettingsDialog::on_sbContrast_valueChanged(double arg1)
{
    m_camera->setContrast(arg1);
    ui->sliderContrast->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderContrast_valueChanged(int value)
{
    ui->sbContrast->setValue(value);
}

void GenericCameraSettingsDialog::on_sbSaturation_valueChanged(double arg1)
{
    m_camera->setSaturation(arg1);
    ui->sliderSaturation->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderSaturation_valueChanged(int value)
{
    ui->sbSaturation->setValue(value);
}

void GenericCameraSettingsDialog::on_sbHue_valueChanged(double arg1)
{
    m_camera->setHue(arg1);
    ui->sliderHue->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderHue_valueChanged(int value)
{
    ui->sbHue->setValue(value);
}

void GenericCameraSettingsDialog::on_sbGain_valueChanged(double arg1)
{
    m_camera->setGain(arg1);
    ui->sliderGain->setValue(arg1);
}

void GenericCameraSettingsDialog::on_sliderGain_valueChanged(int value)
{
    ui->sbGain->setValue(value);
}
