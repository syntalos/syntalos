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

#ifndef UEYECAMERASETTINGSDIALOG_H
#define UEYECAMERASETTINGSDIALOG_H

#include <QDialog>
#include "ueyecamera.h"

class QCheckBox;
class QLabel;

namespace Ui {
class UEyeCameraSettingsDialog;
}

class UEyeCameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UEyeCameraSettingsDialog(UEyeCamera *camera, QWidget *parent = nullptr);
    ~UEyeCameraSettingsDialog();

    QVariant selectedCamera() const;
    cv::Size selectedSize() const;
    int selectedFps() const;
    void setRunning(bool running); 

private slots:
    void on_sbGain_valueChanged(int arg1);
    void on_sbExposure_valueChanged(double arg1);
    void on_cameraComboBox_currentIndexChanged(int index);

private:
    Ui::UEyeCameraSettingsDialog *ui;
    QCheckBox *m_gainCB;
    QLabel *m_ueyeConfFileLbl;
    QCheckBox *m_camFlashMode;

    UEyeCamera *m_camera;
};

#endif // UEYECAMERASETTINGSDIALOG_H
