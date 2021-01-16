/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "porteditordialog.h"
#include "ui_porteditordialog.h"

#include <QInputDialog>

#include "moduleapi.h"
#include "streams/datatypes.h"

PortEditorDialog::PortEditorDialog(AbstractModule *mod, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PortEditorDialog),
    m_mod(mod)
{
    ui->setupUi(this);
    updatePortLists();
}

PortEditorDialog::~PortEditorDialog()
{
    delete ui;
}

void PortEditorDialog::on_tbAddInputPort_clicked()
{
    auto streamTypeMap = streamTypeIdMap();

    bool ok;
    auto item = QInputDialog::getItem(this,
                                      QStringLiteral("Input Port Data Type"),
                                      QStringLiteral("Data type accepted by the input port:"),
                                      streamTypeMap.keys(),
                                      0,
                                      false,
                                      &ok);
    if (!ok || item.isEmpty())
        return;

    auto idStr = QInputDialog::getText(this,
                                       QStringLiteral("Set Port ID"),
                                       QStringLiteral("An internal, unique ID to identify the port:"),
                                       QLineEdit::Normal,
                                       item.toLower() + QStringLiteral("-in"),
                                       &ok);
    if (!ok || idStr.isEmpty())
        return;

    auto title = QInputDialog::getText(this,
                                        QStringLiteral("Set Port Title"),
                                        QStringLiteral("A human-readable short port title:"),
                                        QLineEdit::Normal,
                                        item + QStringLiteral(" In"),
                                        &ok);
    if (!ok || title.isEmpty())
        return;

    m_mod->registerInputPortByTypeId(streamTypeMap[item], idStr, title);
    updatePortLists();
}

void PortEditorDialog::on_tbAddOutputPort_clicked()
{
    auto streamTypeMap = streamTypeIdMap();

    bool ok;
    auto item = QInputDialog::getItem(this,
                                      QStringLiteral("Output Port Data Type"),
                                      QStringLiteral("Type of emitted data:"),
                                      streamTypeMap.keys(),
                                      0,
                                      false,
                                      &ok);
    if (!ok || item.isEmpty())
        return;

    auto idStr = QInputDialog::getText(this,
                                       QStringLiteral("Set Port ID"),
                                       QStringLiteral("An internal, unique ID to identify the port:"),
                                       QLineEdit::Normal,
                                       item.toLower() + QStringLiteral("-out"),
                                       &ok);
    if (!ok || idStr.isEmpty())
        return;

    auto title = QInputDialog::getText(this,
                                        QStringLiteral("Set Port Title"),
                                        QStringLiteral("A human-readable short port title:"),
                                        QLineEdit::Normal,
                                        item + QStringLiteral(" Out"),
                                        &ok);
    if (!ok || title.isEmpty())
        return;

    m_mod->registerOutputPortByTypeId(streamTypeMap[item], idStr, title);
    updatePortLists();
}

void PortEditorDialog::updatePortLists()
{
    ui->lwInputPorts->clear();
    ui->lwOutputPorts->clear();

    for (const auto &port : m_mod->inPorts()) {
        auto item = new QListWidgetItem(QStringLiteral("%1 (%2) [>>%3]").arg(port->title())
                                                                        .arg(port->id())
                                                                        .arg(port->dataTypeName()),
                                        ui->lwInputPorts);
        item->setData(Qt::UserRole, port->id());
    }

    for (const auto &port : m_mod->outPorts()) {
        auto item = new QListWidgetItem(QStringLiteral("%1 (%2) [<<%3]").arg(port->title())
                                                                        .arg(port->id())
                                                                        .arg(port->dataTypeName()),
                                        ui->lwOutputPorts);
        item->setData(Qt::UserRole, port->id());
    }
}

void PortEditorDialog::on_tbRemoveInputPort_clicked()
{
    auto selItems = ui->lwInputPorts->selectedItems();
    if (selItems.isEmpty())
        return;
    ui->tbRemoveInputPort->setEnabled(false);
    m_mod->removeInPortById(selItems.first()->data(Qt::UserRole).toString());
    updatePortLists();
}

void PortEditorDialog::on_lwInputPorts_currentRowChanged(int currentRow)
{
    ui->tbRemoveInputPort->setEnabled(currentRow >= 0);
}

void PortEditorDialog::on_lwOutputPorts_currentRowChanged(int currentRow)
{
    ui->tbRemoveOutputPort->setEnabled(currentRow >= 0);
}

void PortEditorDialog::on_tbRemoveOutputPort_clicked()
{
    auto selItems = ui->lwOutputPorts->selectedItems();
    if (selItems.isEmpty())
        return;
    ui->tbRemoveOutputPort->setEnabled(false);
    m_mod->removeOutPortById(selItems.first()->data(Qt::UserRole).toString());
    updatePortLists();
}
