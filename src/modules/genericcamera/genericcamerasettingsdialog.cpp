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
    Q_FOREACH(const auto cameraInfo, cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }
}

GenericCameraSettingsDialog::~GenericCameraSettingsDialog()
{
    delete ui;
}

QVariant GenericCameraSettingsDialog::selectedCamera() const
{
    return ui->cameraComboBox->currentData();
}

cv::Size GenericCameraSettingsDialog::selectedSize() const
{
    return cv::Size(ui->spinBoxWidth->value(), ui->spinBoxHeight->value());
}

int GenericCameraSettingsDialog::selectedFps() const
{
    return ui->fpsSpinBox->value();
}

void GenericCameraSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
}

void GenericCameraSettingsDialog::on_cameraComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_camera->setCamId(ui->cameraComboBox->currentData().toInt());
}

void GenericCameraSettingsDialog::on_sbExposure_valueChanged(int arg1)
{
    m_camera->setExposure(arg1);
}

void GenericCameraSettingsDialog::on_sbGain_valueChanged(int arg1)
{
    m_camera->setGain(arg1);
}
