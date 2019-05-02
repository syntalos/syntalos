/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleindicator.h"
#include "ui_moduleindicator.h"

#include <QPixmap>
#include <QMenu>

#include "modulemanager.h"

#pragma GCC diagnostic ignored "-Wpadded"
class ModuleIndicator::MIData : public QSharedData {
public:
    MIData()
    {

    }

    ~MIData()
    {

    }

    AbstractModule *module;
    ModuleManager *manager;
    QMenu *menu;
};
#pragma GCC diagnostic pop

ModuleIndicator::ModuleIndicator(AbstractModule *module, ModuleManager *manager, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ModuleIndicator),
    d(new MIData)
{
    ui->setupUi(this);
    d->menu = new QMenu(this);

    d->module = module;
    d->manager = manager;

    ui->showButton->setEnabled(false);
    ui->configButton->setEnabled(false);
    receiveStateChange(ModuleState::PREPARING);

    ui->moduleImage->setPixmap(d->module->pixmap());
    ui->moduleNameLabel->setText(d->module->displayName());

    connect(d->module, &AbstractModule::actionsUpdated, this, &ModuleIndicator::receiveActionsUpdated);
    connect(d->module, &AbstractModule::stateChanged, this, &ModuleIndicator::receiveStateChange);
    connect(d->module, &AbstractModule::errorMessage, this, &ModuleIndicator::receiveErrorMessage);
}

ModuleIndicator::~ModuleIndicator()
{
    delete ui;
}

AbstractModule *ModuleIndicator::module() const
{
    return d->module;
}

void ModuleIndicator::receiveActionsUpdated()
{
    if (d->module->actions().isEmpty()) {
        ui->menuButton->setVisible(false);
    } else {
        d->menu->clear();
        ui->menuButton->setVisible(true);
        foreach(auto action, d->module->actions())
            d->menu->addAction(action);
        ui->menuButton->setMenu(d->menu);
        connect(ui->menuButton, &QToolButton::clicked, ui->menuButton, &QToolButton::showMenu);
    }
}

void ModuleIndicator::receiveStateChange(ModuleState state)
{
    switch (state) {
    case ModuleState::PREPARING:
        ui->statusImage->setPixmap(QPixmap(":/status/preparing"));
        ui->statusLabel->setText("Preparing...");
        break;
    case ModuleState::READY:
        ui->statusImage->setPixmap(QPixmap(":/status/ready"));
        ui->statusLabel->setText("Ready.");
        ui->showButton->setEnabled(true);
        ui->configButton->setEnabled(true);
        break;
    case ModuleState::RUNNING:
        ui->statusImage->setPixmap(QPixmap(":/status/running"));
        ui->statusLabel->setText("Running...");
        break;
    case ModuleState::ERROR:
        ui->statusImage->setPixmap(QPixmap(":/status/error"));
        ui->statusLabel->setText("Error!");
        break;
    default:
        ui->statusImage->setPixmap(QPixmap(":/status/preparing"));
        ui->statusLabel->setText("Module is in an unknown state.");
        break;
    }

    // update status change immediately in UI
    QApplication::processEvents();
}

void ModuleIndicator::receiveErrorMessage(const QString &message)
{
    ui->infoLabel->setText(message);
}

void ModuleIndicator::on_configButton_clicked()
{
    d->module->showSettingsUi();
}

void ModuleIndicator::on_showButton_clicked()
{
    d->module->showDisplayUi();
}

void ModuleIndicator::on_removeButton_clicked()
{
    if (d->manager != nullptr) {
        d->manager->removeModule(d->module);
        d->module = nullptr;
       // this->hide();
    }

}
