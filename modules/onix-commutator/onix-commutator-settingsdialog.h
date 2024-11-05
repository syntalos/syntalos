/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QDialog>

namespace Ui
{
class ONIXCommutatorSettingsDialog;
}

class ONIXCommutatorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ONIXCommutatorSettingsDialog(QWidget *parent = nullptr);
    ~ONIXCommutatorSettingsDialog();

    void scanDevices();
    void setRunning(bool running);

    QString serialPort() const;
    void setSerialPort(QString port);

    bool statusLedEnabled() const;
    void setStatusLedEnabled(bool enabled);

    double speed() const;
    void setSpeed(double speed);

    double acceleration() const;
    void setAcceleration(double acceleration);

private:
    Ui::ONIXCommutatorSettingsDialog *ui;
};
