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

#include <QToolButton>
#include <QFileDialog>
#include <QCheckBox>

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

    // video settings panel
    m_gainCB = new QCheckBox(this);
    m_gainCB->setChecked(false);
    ui->uEyeLayout->addRow(new QLabel("Automatic gain", this), m_gainCB);

    auto ueyeConfFileWidget = new QWidget(this);
    auto ueyeConfFileLayout = new QHBoxLayout;
    ueyeConfFileWidget->setLayout(ueyeConfFileLayout);
    ueyeConfFileLayout->setMargin(0);
    ui->uEyeLayout->addRow(new QLabel("uEye Configuration File", this), ueyeConfFileWidget);

    m_ueyeConfFileLbl = new QLabel(this);
    ueyeConfFileLayout->addWidget(m_ueyeConfFileLbl);
    auto ueyeConfFileBtn = new QToolButton(this);
    ueyeConfFileLayout->addWidget(ueyeConfFileBtn);
    ueyeConfFileBtn->setIcon(QIcon::fromTheme("folder-open"));
    m_ueyeConfFileLbl->setText("No file selected.");

    connect(ueyeConfFileBtn, &QToolButton::clicked, [=]() {
        auto fileName = QFileDialog::getOpenFileName(this,
                                                     tr("Select uEye Settings"), ".",
                                                     tr("uEye Settings (*.ini)"));
        if (fileName.isEmpty())
            return;
        m_ueyeConfFileLbl->setText(fileName);
    });

    m_camFlashMode = new QCheckBox(this);
    m_camFlashMode->setChecked(true);
    ui->uEyeLayout->addRow(new QLabel("Enable GPIO flash", this), m_camFlashMode);
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
    auto size = ui->resolutionComboBox->currentData().value<QSize>();
    return cv::Size(size.width(), size.height());
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

    // probe the new camera for its resolution list
    auto camera = new UEyeCamera;
    auto ret = camera->getResolutionList(m_camera->camId());
    delete camera;

    ui->resolutionComboBox->clear();
    Q_FOREACH(auto res, ret)
        ui->resolutionComboBox->addItem(QStringLiteral("%1x%2").arg(res.width()).arg(res.height()), res);
}

void UEyeCameraSettingsDialog::on_sbGain_valueChanged(int arg1)
{
    m_camera->setAutoGain(arg1);
}

void UEyeCameraSettingsDialog::on_sbExposure_valueChanged(double arg1)
{
    m_camera->setExposureTime(arg1);
}
