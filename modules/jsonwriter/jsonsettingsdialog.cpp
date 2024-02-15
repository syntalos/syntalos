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

#include "jsonsettingsdialog.h"
#include "ui_jsonsettingsdialog.h"

#include <QMessageBox>
#include <QThread>
#include <QVariant>

#include "utils/misc.h"

JSONSettingsDialog::JSONSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::JSONSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    // take name from source module by default
    ui->nameFromSrcCheckBox->setChecked(true);

    // select all incoming data for recording by default
    ui->useAllDataCheckBox->setChecked(true);
    on_useAllDataCheckBox_toggled(true);

    // register formats
    ui->formatComboBox->addItem("Pandas-compatible JSON", "pandas-split");
    ui->formatComboBox->addItem("Metadata-extended JSON", "extended-pandas");
}

JSONSettingsDialog::~JSONSettingsDialog()
{
    delete ui;
}

bool JSONSettingsDialog::useNameFromSource() const
{
    return ui->nameFromSrcCheckBox->isChecked();
}

void JSONSettingsDialog::setUseNameFromSource(bool fromSource)
{
    ui->nameFromSrcCheckBox->setChecked(fromSource);
}

void JSONSettingsDialog::setDataName(const QString &value)
{
    m_dataName = simplifyStrForFileBasename(value);
    ui->nameLineEdit->setText(m_dataName);
}

QString JSONSettingsDialog::jsonFormat() const
{
    return ui->formatComboBox->currentData().toString();
}

void JSONSettingsDialog::setJsonFormat(const QString &format)
{
    for (int i = 0; i < ui->formatComboBox->count(); i++) {
        if (ui->formatComboBox->itemData(i).toString() == format) {
            ui->formatComboBox->setCurrentIndex(i);
            break;
        }
    }
}

bool JSONSettingsDialog::recordAllData() const
{
    return ui->useAllDataCheckBox->isChecked();
}

void JSONSettingsDialog::setRecordAllData(bool enabled)
{
    ui->useAllDataCheckBox->setChecked(enabled);
}

void JSONSettingsDialog::setAvailableEntries(const QStringList &list)
{
    ui->availableListWidget->clear();
    ui->availableListWidget->addItems(list);

    if (list.isEmpty()) {
        ui->useAllDataCheckBox->setChecked(true);
        ui->useAllDataCheckBox->setEnabled(false);
    } else {
        ui->useAllDataCheckBox->setEnabled(true);
    }
}

QStringList JSONSettingsDialog::availableEntries()
{
    QStringList list;
    for (int i = 0; i < ui->availableListWidget->count(); i++) {
        list.append(ui->availableListWidget->item(i)->text());
    }

    return list;
}

QSet<QString> JSONSettingsDialog::recordedEntriesSet() const
{
    QSet<QString> recSet;
    for (int i = 0; i < ui->recordListWidget->count(); i++) {
        recSet.insert(ui->recordListWidget->item(i)->text());
    }

    return recSet;
}

QStringList JSONSettingsDialog::recordedEntries() const
{
    QStringList list;
    for (int i = 0; i < ui->recordListWidget->count(); i++) {
        list.append(ui->recordListWidget->item(i)->text());
    }

    return list;
}

void JSONSettingsDialog::setRecordedEntries(const QStringList &list)
{
    ui->recordListWidget->clear();
    ui->recordListWidget->addItems(list);
}

QString JSONSettingsDialog::dataName() const
{
    return m_dataName;
}

void JSONSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_dataName = simplifyStrForFileBasename(arg1);
}

void JSONSettingsDialog::on_nameFromSrcCheckBox_toggled(bool checked)
{
    ui->nameLineEdit->setEnabled(!checked);
}

void JSONSettingsDialog::on_useAllDataCheckBox_toggled(bool checked)
{
    ui->dataBoxesWidget->setEnabled(!checked);
}

void JSONSettingsDialog::setRunning(bool running)
{
    ui->generalWidget->setEnabled(!running);
    ui->dataSelectWidget->setEnabled(!running);
}

void JSONSettingsDialog::on_addRecordedButton_clicked()
{
    const auto currentAvItem = ui->availableListWidget->currentItem();
    if (currentAvItem == nullptr)
        return;
    const auto currentSelection = currentAvItem->text();
    for (int i = 0; i < ui->recordListWidget->count(); i++) {
        if (ui->recordListWidget->item(i)->text() == currentSelection)
            return;
    }

    ui->recordListWidget->addItem(currentSelection);
}

void JSONSettingsDialog::on_removeRecordedButton_clicked()
{
    const auto currentRecItem = ui->recordListWidget->currentItem();
    if (currentRecItem == nullptr)
        return;
    delete currentRecItem;
}
