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

#include "ueyecamerasettingsdialog.h"
#include "ui_ueyecamerasettingsdialog.h"

UEyeCameraSettingsDialog::UEyeCameraSettingsDialog(UEyeCamera *camera, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UEyeCameraSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));
    m_camera = camera;

    auto cameras = UEyeCamera::availableCameras();
    Q_FOREACH(const auto cameraInfo, cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }
}

UEyeCameraSettingsDialog::~UEyeCameraSettingsDialog()
{
    delete ui;
}

QVariant UEyeCameraSettingsDialog::selectedCamera() const
{
    return ui->cameraComboBox->currentData();
}

cv::Size UEyeCameraSettingsDialog::selectedSize() const
{
    return cv::Size(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
}

int UEyeCameraSettingsDialog::selectedFps() const
{
    return ui->fpsSpinBox->value();
}

void UEyeCameraSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
}

void UEyeCameraSettingsDialog::on_cameraComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_camera->setCamId(ui->cameraComboBox->currentData().toInt());
}

void UEyeCameraSettingsDialog::on_sbGain_valueChanged(int arg1)
{
    m_camera->setAutoGain(arg1);
}

void UEyeCameraSettingsDialog::on_sbExposure_valueChanged(double arg1)
{
    m_camera->setExposureTime(arg1);
}
