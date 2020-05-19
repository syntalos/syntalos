/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "globalconfigdialog.h"
#include "ui_globalconfigdialog.h"

GlobalConfigDialog::GlobalConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GlobalConfigDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Syntalos Settings"));
    setWindowModality(Qt::WindowModal);

    m_settings = new QSettings("DraguhnLab", "Syntalos", this);
}

GlobalConfigDialog::~GlobalConfigDialog()
{
    delete ui;
}

bool GlobalConfigDialog::saveExperimentDiagnostics() const
{
    return ui->cbSaveDiagnostic->isChecked();
}

void GlobalConfigDialog::on_cbSaveDiagnostic_toggled(bool checked)
{
    m_settings->setValue("devel/saveDiagnostics", checked);
}
