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

GenericCameraSettingsDialog::GenericCameraSettingsDialog(GenericCamera *camera, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GenericCameraSettingsDialog)
{
    ui->setupUi(this);
    m_camera = camera;

    m_camera->setFramerate(ui->fpsSpinBox->value());

    Q_FOREACH(auto cinfo, m_camera->getCameraList()) {
        ui->cameraComboBox->addItem(cinfo.first, cinfo.second);
    }
}

GenericCameraSettingsDialog::~GenericCameraSettingsDialog()
{
    delete ui;
}

void GenericCameraSettingsDialog::on_fpsSpinBox_valueChanged(int arg1)
{
    m_camera->setFramerate(arg1);
}
