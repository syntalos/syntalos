/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "intervalrundialog.h"
#include "ui_intervalrundialog.h"

using namespace Syntalos;

IntervalRunDialog::IntervalRunDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::IntervalRunDialog)
{
    ui->setupUi(this);
}

IntervalRunDialog::~IntervalRunDialog()
{
    delete ui;
}

bool IntervalRunDialog::intervalRunEnabled() const
{
    return ui->ivrGroupBox->isChecked();
}

int IntervalRunDialog::runsN() const
{
    return ui->spinBoxRunsN->value();
}

double IntervalRunDialog::runDurationMin() const
{
    return ui->spinBoxDuration->value();
}

double IntervalRunDialog::delayMin() const
{
    return ui->spinBoxDelay->value();
}
