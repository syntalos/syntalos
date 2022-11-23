/**
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <QList>

namespace Ui {
class AudioSettingsDialog;
}

class AudioSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);
    ~AudioSettingsDialog();

    bool startImmediately() const;
    void setStartImmediately(bool value);

    int waveKind() const;
    void setWaveKind(int value);

    double frequency() const;
    void setFrequency(double value);

    double volume() const;
    void setVolume(double value);

private:
    Ui::AudioSettingsDialog *ui;
};
