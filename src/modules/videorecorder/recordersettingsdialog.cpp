/**
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "recordersettingsdialog.h"
#include "ui_recordersettingsdialog.h"

#include <QVariant>

RecorderSettingsDialog::RecorderSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RecorderSettingsDialog),
    m_selectedImgSrcMod(nullptr)
{
    ui->setupUi(this);
}

RecorderSettingsDialog::~RecorderSettingsDialog()
{
    delete ui;
}

void RecorderSettingsDialog::setImageSourceModules(const QList<ImageSourceModule *> &mods)
{
    ui->frameSourceComboBox->clear();
    Q_FOREACH(auto mod, mods) {
        ui->frameSourceComboBox->addItem(mod->name(), QVariant(QMetaType::QObjectStar, &mod));
    }
}

ImageSourceModule *RecorderSettingsDialog::selectedImageSourceMod()
{
    return m_selectedImgSrcMod;
}

void RecorderSettingsDialog::setSelectedImageSourceMod(ImageSourceModule *mod)
{
    m_selectedImgSrcMod = mod;
}

void RecorderSettingsDialog::setVideoName(const QString &value)
{
    m_videoName = value.simplified().replace(" ", "_");
    ui->nameLineEdit->setText(m_videoName);
}

QString RecorderSettingsDialog::videoName() const
{
    return m_videoName;
}

bool RecorderSettingsDialog::saveTimestamps() const
{
    return ui->timestampFileCheckBox->isChecked();
}

void RecorderSettingsDialog::on_frameSourceComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_selectedImgSrcMod = ui->frameSourceComboBox->currentData().value<ImageSourceModule*>();
}

void RecorderSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_videoName = arg1.simplified().replace(" ", "_");
}
