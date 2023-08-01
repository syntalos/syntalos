/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "runcmdsettingsdlg.h"
#include "ui_runcmdsettingsdlg.h"

#include <QIcon>

RunCmdSettingsDlg::RunCmdSettingsDlg(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::RunCmdSettingsDlg)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));
}

RunCmdSettingsDlg::~RunCmdSettingsDlg()
{
    delete ui;
}

QString RunCmdSettingsDlg::executable() const
{
    return ui->exeLineEdit->text();
}

void RunCmdSettingsDlg::setExecutable(const QString &exe)
{
    ui->exeLineEdit->setText(exe);
}

QString RunCmdSettingsDlg::parametersStr() const
{
    return ui->parametersLineEdit->text();
}

void RunCmdSettingsDlg::setParametersStr(const QString parameters)
{
    ui->parametersLineEdit->setText(parameters);
}

bool RunCmdSettingsDlg::runOnHost() const
{
    return ui->runOnHostCheckBox->isChecked();
}

void RunCmdSettingsDlg::setRunOnHost(bool onHost)
{
    ui->runOnHostCheckBox->setChecked(onHost);
}

void RunCmdSettingsDlg::setSandboxUiVisible(bool visible)
{
    ui->runOnHostCheckBox->setVisible(visible);
    ui->runOnHostLabel->setVisible(visible);
}
