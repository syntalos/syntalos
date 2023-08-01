/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "clocksettingsdialog.h"
#include "ui_clocksettingsdialog.h"

#include <QIcon>

ClockSettingsDialog::ClockSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ClockSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));
}

ClockSettingsDialog::~ClockSettingsDialog()
{
    delete ui;
}

long ClockSettingsDialog::pulseIntervalUs() const
{
    return ui->pulseIntervalSpinBox->value();
}

void ClockSettingsDialog::setPulseIntervalUs(long usec)
{
    ui->pulseIntervalSpinBox->setValue(usec);
}

bool ClockSettingsDialog::highPriorityThread() const
{
    return ui->hpThreadCheckBox->isChecked();
}

void ClockSettingsDialog::setHighPriorityThread(bool enabled)
{
    ui->hpThreadCheckBox->setChecked(enabled);
}
