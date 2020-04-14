/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QDialog>
#include "flircamera.h"

namespace Ui {
class FLIRCamSettingsDialog;
}

class FLIRCamSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FLIRCamSettingsDialog(FLIRCamera *camera, QWidget *parent = nullptr);
    ~FLIRCamSettingsDialog();

    QString selectedCameraSerial() const;
    cv::Size resolution() const;

    int framerate() const;
    void setFramerate(int fps);

    void setRunning(bool running);

    void updateValues();

private slots:
    void on_cameraComboBox_currentIndexChanged(int index);

    void on_sbExposure_valueChanged(int arg1);

    void on_sbBrightness_valueChanged(double arg1);
    void on_sliderBrightness_valueChanged(int value);

    void on_sbContrast_valueChanged(double arg1);
    void on_sliderContrast_valueChanged(int value);

    void on_sbSaturation_valueChanged(double arg1);
    void on_sliderSaturation_valueChanged(int value);

    void on_sbGain_valueChanged(double arg1);
    void on_sliderGain_valueChanged(int value);

private:
    Ui::FLIRCamSettingsDialog *ui;

    FLIRCamera *m_camera;
};
