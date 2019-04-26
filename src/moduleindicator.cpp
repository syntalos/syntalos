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
};
#pragma GCC diagnostic pop

ModuleIndicator::ModuleIndicator(AbstractModule *module, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ModuleIndicator),
    d(new MIData)
{
    ui->setupUi(this);

    d->module = module;

    // defaults
    ui->showButton->setEnabled(false);
    receiveStateChange(ModuleState::PREPARING);

    connect(d->module, &AbstractModule::stateChanged, this, &ModuleIndicator::receiveStateChange);
    connect(d->module, &AbstractModule::errorMessage, this, &ModuleIndicator::receiveErrorMessage);
}

ModuleIndicator::~ModuleIndicator()
{
    delete ui;
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
