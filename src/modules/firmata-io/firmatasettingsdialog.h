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

#ifndef FIRMATASETTINGSDIALOG_H
#define FIRMATASETTINGSDIALOG_H

#include <QDialog>

namespace Ui {
class FirmataSettingsDialog;
}

class FirmataSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirmataSettingsDialog(QWidget *parent = nullptr);
    ~FirmataSettingsDialog();

    void setRunning(bool running);

private:
    Ui::FirmataSettingsDialog *ui;
};

#endif // FIRMATASETTINGSDIALOG_H
