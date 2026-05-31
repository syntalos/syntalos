/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "lccamera.h"
#include <QDialog>

namespace Ui
{
class LcSettingsDialog;
}

class LcSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LcSettingsDialog(LcCamera *camera, QWidget *parent = nullptr);
    ~LcSettingsDialog() override;

    cv::Size resolution() const;

    double framerate() const;
    void setFramerate(double fps);

    QString pixelFormatName() const;
    void setPixelFormatName(const QString &pixFmtName);

    void setRunning(bool running);

    void updateValues();

private slots:
    void on_cameraComboBox_currentIndexChanged(int index);
    void on_captureFormatComboBox_currentIndexChanged(int index);

    void on_autoExposureCheckBox_toggled(bool checked);
    void on_sbExposure_valueChanged(double value);
    void on_sbGain_valueChanged(double value);
    void on_sbBrightness_valueChanged(double value);
    void on_sbContrast_valueChanged(double value);
    void on_sbSaturation_valueChanged(double value);
    void on_powerLineComboBox_currentIndexChanged(int index);

private:
    void refreshResolutions();
    void refreshControls();

    Ui::LcSettingsDialog *ui;

    LcCamera *m_camera;
    QString m_pixFmtName;
};
