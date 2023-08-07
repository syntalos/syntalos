/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "genericcamerasettingsdialog.h"
#include "ui_genericcamerasettingsdialog.h"

#include <QMessageBox>

GenericCameraSettingsDialog::GenericCameraSettingsDialog(Camera *camera, QWidget *parent)
    : QDialog(parent),
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

bool GenericCameraSettingsDialog::quirksEnabled()
{
    return ui->quirkGroupBox->isChecked();
}

void GenericCameraSettingsDialog::setQuirksEnabled(bool enabled)
{
    ui->quirkGroupBox->setChecked(enabled);
}

QString GenericCameraSettingsDialog::pixelFormatName() const
{
    return m_pixFmtName;
}

void GenericCameraSettingsDialog::setPixelFormatName(const QString &pixFmtName)
{
    m_pixFmtName = pixFmtName;
    for (int i = 0; i < ui->captureFormatComboBox->count(); i++) {
        const auto pixFmt = ui->captureFormatComboBox->itemData(i).value<CameraPixelFormat>();
        if (pixFmt.name == pixFmtName) {
            ui->captureFormatComboBox->setCurrentIndex(i);
            on_captureFormatComboBox_currentIndexChanged(i);
            break;
        }
    }
}

void GenericCameraSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
    ui->quirkGroupBox->setEnabled(!running);
}

void GenericCameraSettingsDialog::updateValues()
{
    const auto prevCamId = m_camera->camId();
    const auto prevPixFmtName = m_pixFmtName;
    ui->cameraComboBox->clear();
    auto cameras = Camera::availableCameras();
    for (const auto &cameraInfo : cameras) {
        ui->cameraComboBox->addItem(cameraInfo.first, QVariant(cameraInfo.second));
    }

    for (int i = 0; i < ui->cameraComboBox->count(); i++) {
        if (ui->cameraComboBox->itemData(i).toInt() == prevCamId) {
            ui->cameraComboBox->setCurrentIndex(i);
            on_cameraComboBox_currentIndexChanged(i);
            break;
        }
    }

    setPixelFormatName(prevPixFmtName);
    ui->spinBoxWidth->setValue(m_camera->resolution().width);
    ui->spinBoxHeight->setValue(m_camera->resolution().height);
    ui->sbExposure->setValue(m_camera->exposure());
    ui->sbBrightness->setValue(m_camera->brightness());
    ui->sbContrast->setValue(m_camera->contrast());
    ui->sbSaturation->setValue(m_camera->saturation());
    ui->sbHue->setValue(m_camera->hue());
    ui->sbGain->setValue(m_camera->gain());
    ui->autoExposureRawSpinBox->setValue(m_camera->autoExposureRaw());
}

void GenericCameraSettingsDialog::on_cameraComboBox_currentIndexChanged(int)
{
    m_camera->setCamId(ui->cameraComboBox->currentData().toInt());

    ui->captureFormatComboBox->clear();
    for (const auto &pixFmt : m_camera->readPixelFormats()) {
        ui->captureFormatComboBox->addItem(pixFmt.name, QVariant::fromValue(pixFmt));
    }
}

void GenericCameraSettingsDialog::on_captureFormatComboBox_currentIndexChanged(int)
{
    const auto pixFmt = ui->captureFormatComboBox->currentData().value<CameraPixelFormat>();
    m_camera->setPixelFormat(pixFmt);
    m_pixFmtName = pixFmt.name;
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

void GenericCameraSettingsDialog::on_autoExposureRawSpinBox_valueChanged(int value)
{
    m_camera->setAutoExposureRaw(value);
}

void GenericCameraSettingsDialog::on_autoExposureRawInfoButton_clicked()
{
    QMessageBox::information(
        this,
        QStringLiteral("Information on Auto Exposure"),
        QStringLiteral(
            "<html>According to the OpenCV/V4L documentation, values for this should be:<br/>"
            "<b>0</b>: Auto Mode <br/><b>1</b>: Manual Mode <br/><b>2</b>: Shutter Priority Mode <br/><b>3</b>: "
            "Aperture Priority Mode<br/>"
            "However, not all cameras seem to behave this way, many times a value of 1 seems to disable auto "
            "exposure.<br/>"
            "So, depending on your camera, you may need to play with this value to properly disable auto exposure."));
}
