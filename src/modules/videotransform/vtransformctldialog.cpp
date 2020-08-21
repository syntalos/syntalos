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

#include "vtransformctldialog.h"
#include "ui_vtransformctldialog.h"

#include <QDebug>
#include <QInputDialog>

VTransformCtlDialog::VTransformCtlDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VTransformCtlDialog),
    m_curSettingsPanel(nullptr),
    m_running(false)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    m_vtfListModel = new VTransformListModel(this);
    ui->activeTFListView->setModel(m_vtfListModel);
}

VTransformCtlDialog::~VTransformCtlDialog()
{
    delete ui;
}

void VTransformCtlDialog::setRunning(bool running)
{
    if (running != m_running)
        unselectAll();
    m_running = running;
    ui->modButtonsWidget->setEnabled(!m_running);
}

void VTransformCtlDialog::unselectAll()
{
    ui->activeTFListView->clearSelection();
    resetSettingsPanel();
}

void VTransformCtlDialog::resetSettingsPanel()
{
    if (m_curSettingsPanel != nullptr)
        delete m_curSettingsPanel;
    m_curSettingsPanel = new QWidget(ui->widgetSettingsContainer);
    ui->widgetSettingsContainer->layout()->addWidget(m_curSettingsPanel);
    ui->labelSettingsHeader->setText(QStringLiteral("Settings"));
}

QList<std::shared_ptr<VideoTransform> > VTransformCtlDialog::transformList()
{
    return m_vtfListModel->toList();
}

QVariantHash VTransformCtlDialog::serializeSettings() const
{
    return m_vtfListModel->toVariantHash();
}

void VTransformCtlDialog::loadSettings(const QVariantHash &settings)
{
    m_vtfListModel->fromVariantHash(settings);
    unselectAll();
}

void VTransformCtlDialog::on_btnAdd_clicked()
{
    bool ok;
    auto item = QInputDialog::getItem(this,
                                      QStringLiteral("Select Transformation"),
                                      QStringLiteral("New Transformation:"),
                                      QStringList() << "Crop" << "Scale" << "Remove Color",
                                      0,
                                      false,
                                      &ok);
    if (!ok || item.isEmpty())
        return;

    VideoTransform *tfPtr;
    if (item == "Scale")
        tfPtr = new ScaleTransform;
    else if (item == "Crop")
        tfPtr = new CropTransform;
    else
        return;

    std::shared_ptr<VideoTransform> tf(tfPtr);
    m_vtfListModel->addTransform(tf);
}

void VTransformCtlDialog::on_btnRemove_clicked()
{
    if (!ui->activeTFListView->currentIndex().isValid())
        return;
    m_vtfListModel->removeRow(ui->activeTFListView->currentIndex().row());
    resetSettingsPanel();
}

void VTransformCtlDialog::on_activeTFListView_activated(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    resetSettingsPanel();
    auto tf = m_vtfListModel->transform(index.row());
    tf->createSettingsUi(m_curSettingsPanel);
    ui->labelSettingsHeader->setText(QStringLiteral("Settings for %1").arg(tf->name()));

    if (m_running && !tf->allowOnlineModify())
        m_curSettingsPanel->setEnabled(false);
}
