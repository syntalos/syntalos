/**
 * Copyright (C) 2019-2023 Matthias Klumpp <matthias@tenstral.net>
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

#include "canvassettingsdialog.h"
#include "ui_canvassettingsdialog.h"

#include <QMessageBox>
#include <QThread>
#include <QVariant>

CanvasSettingsDialog::CanvasSettingsDialog(CanvasWindow *cvView, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CanvasSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));
    setWindowTitle("Canvas Settings");

    m_cvView = cvView;
    updateUi();
}

CanvasSettingsDialog::~CanvasSettingsDialog()
{
    delete ui;
}

void CanvasSettingsDialog::updateUi()
{
    ui->highlightSaturationCheckBox->setChecked(m_cvView->highlightSaturation());
}

void CanvasSettingsDialog::on_highlightSaturationCheckBox_toggled(bool checked)
{
    m_cvView->setHighlightSaturation(checked);
}
