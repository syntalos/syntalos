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
    cv::Size selectedSize() const;
    int selectedFps() const;
    void setRunning(bool running);

private slots:
    void on_cameraComboBox_currentIndexChanged(int index);

private:
    Ui::GenericCameraSettingsDialog *ui;

    Camera *m_camera;
};

#endif // GENERICCAMERASETTINGSDIALOG_H
