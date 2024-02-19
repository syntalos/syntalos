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

#include "tablesettingsdialog.h"
#include "ui_tablesettingsdialog.h"

#include <QMessageBox>
#include <QThread>
#include <QVariant>

#include "utils/misc.h"

TableSettingsDialog::TableSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TableSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // take name from source module by default
    ui->nameFromSrcCheckBox->setChecked(true);

    // save & display all data by default
    ui->saveCheckBox->setChecked(true);
    ui->displayCheckBox->setChecked(true);
}

TableSettingsDialog::~TableSettingsDialog()
{
    delete ui;
}

bool TableSettingsDialog::useNameFromSource() const
{
    return ui->nameFromSrcCheckBox->isChecked();
}

void TableSettingsDialog::setUseNameFromSource(bool fromSource)
{
    ui->nameFromSrcCheckBox->setChecked(fromSource);
}

void TableSettingsDialog::setDataName(const QString &value)
{
    m_dataName = simplifyStrForFileBasename(value);
    ui->nameLineEdit->setText(m_dataName);
}

bool TableSettingsDialog::saveData() const
{
    return ui->saveCheckBox->isChecked();
}

void TableSettingsDialog::setSaveData(bool save)
{
    ui->saveCheckBox->setChecked(save);
}

bool TableSettingsDialog::displayData() const
{
    return ui->displayCheckBox->isChecked();
}

void TableSettingsDialog::setDisplayData(bool display)
{
    ui->displayCheckBox->setChecked(display);
}

QString TableSettingsDialog::dataName() const
{
    return m_dataName;
}

void TableSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_dataName = simplifyStrForFileBasename(arg1);
}

void TableSettingsDialog::on_nameFromSrcCheckBox_toggled(bool checked)
{
    ui->nameLineEdit->setEnabled(!checked);
}

void TableSettingsDialog::setRunning(bool running)
{
    ui->generalWidget->setEnabled(!running);
}
