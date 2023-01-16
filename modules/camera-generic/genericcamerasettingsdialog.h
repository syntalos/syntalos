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

#ifndef GENERICCAMERASETTINGSDIALOG_H
#define GENERICCAMERASETTINGSDIALOG_H

#include <QDialog>
#include "camera.h"

namespace Ui {
class GenericCameraSettingsDialog;
}

class GenericCameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GenericCameraSettingsDialog(Camera *camera, QWidget *parent = nullptr);
    ~GenericCameraSettingsDialog();

    QVariant selectedCamera() const;
    cv::Size resolution() const;

    int framerate() const;
    void setFramerate(int fps);

    bool quirksEnabled();
    void setQuirksEnabled(bool enabled);

    QString pixelFormatName() const;
    void setPixelFormatName(const QString &pixFmtName);

    void setRunning(bool running);

    void updateValues();

private slots:
    void on_cameraComboBox_currentIndexChanged(int index);
    void on_captureFormatComboBox_currentIndexChanged(int index);

    void on_sbExposure_valueChanged(double arg1);
    void on_sliderExposure_valueChanged(int value);

    void on_sbBrightness_valueChanged(double arg1);
    void on_sliderBrightness_valueChanged(int value);

    void on_sbContrast_valueChanged(double arg1);
    void on_sliderContrast_valueChanged(int value);

    void on_sbSaturation_valueChanged(double arg1);
    void on_sliderSaturation_valueChanged(int value);

    void on_sbHue_valueChanged(double arg1);
    void on_sliderHue_valueChanged(int value);

    void on_sbGain_valueChanged(double arg1);
    void on_sliderGain_valueChanged(int value);

    void on_autoExposureRawSpinBox_valueChanged(int value);
    void on_autoExposureRawInfoButton_clicked();

private:
    Ui::GenericCameraSettingsDialog *ui;

    Camera *m_camera;
    QString m_pixFmtName;
};

#endif // GENERICCAMERASETTINGSDIALOG_H
