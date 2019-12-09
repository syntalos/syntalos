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
#include <QMessageBox>
#include <QDebug>
#include <QInputDialog>

#include "modulemanager.h"

#pragma GCC diagnostic ignored "-Wpadded"
class ModuleIndicator::MIData : public QSharedData
{
public:
    MIData() { }
    ~MIData() { }

    AbstractModule *module;
    ModuleManager *manager;
    QMenu *menu;
    QAction *editNameAction;
};
#pragma GCC diagnostic pop

ModuleIndicator::ModuleIndicator(ModuleInfo *modInfo, AbstractModule *module, ModuleManager *manager, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::ModuleIndicator),
    d(new MIData)
{
    ui->setupUi(this);

    d->menu = new QMenu(this);
    d->editNameAction = new QAction(this);
    d->editNameAction->setText(QStringLiteral("Edit Name"));
    d->menu->addAction(d->editNameAction);
    connect(d->editNameAction, &QAction::triggered, this, &ModuleIndicator::on_editNameActionTriggered);
    ui->menuButton->setMenu(d->menu);
    connect(ui->menuButton, &QToolButton::clicked, ui->menuButton, &QToolButton::showMenu);

    d->module = module;
    d->manager = manager;

    ui->showButton->setEnabled(false);
    ui->configButton->setEnabled(false);
    receiveStateChange(ModuleState::PREPARING);

    ui->moduleImage->setPixmap(modInfo->pixmap());
    ui->moduleNameLabel->setText(d->module->name());
    ui->infoLabel->setText("");

    ui->showButton->setVisible(false);
    ui->configButton->setVisible(false);

    const auto features = d->module->features();
    if (features.testFlag(ModuleFeature::DISPLAY))
        ui->showButton->setVisible(true);
    if (features.testFlag(ModuleFeature::SETTINGS))
        ui->configButton->setVisible(true);

    connect(d->module, &AbstractModule::actionsUpdated, this, &ModuleIndicator::receiveActionsUpdated);
    connect(d->module, &AbstractModule::stateChanged, this, &ModuleIndicator::receiveStateChange);
    connect(d->module, &AbstractModule::error, this, &ModuleIndicator::receiveErrorMessage);
    connect(d->module, &AbstractModule::statusMessage, this, &ModuleIndicator::receiveMessage);
    connect(d->manager, &ModuleManager::modulePreRemove, this, &ModuleIndicator::on_modulePreRemove);
    connect(d->module, &AbstractModule::nameChanged, [=](const QString& name) {
        ui->moduleNameLabel->setText(name);
    });
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
    d->menu->clear();
    d->menu->addAction(d->editNameAction);
    d->menu->addSeparator();
    ui->menuButton->setVisible(true);
    Q_FOREACH(auto action, d->module->actions())
        d->menu->addAction(action);
}

void ModuleIndicator::receiveStateChange(ModuleState state)
{
    switch (state) {
    case ModuleState::INITIALIZING:
        ui->statusImage->setPixmap(QPixmap(":/status/preparing"));
        ui->statusLabel->setText("Initializing...");
        ui->removeButton->setEnabled(false);
        d->editNameAction->setEnabled(false);
        break;
    case ModuleState::PREPARING:
        ui->statusImage->setPixmap(QPixmap(":/status/preparing"));
        ui->statusLabel->setText("Preparing...");
        ui->removeButton->setEnabled(false);
        d->editNameAction->setEnabled(false);
        break;
    case ModuleState::WAITING:
        ui->statusImage->setPixmap(QPixmap(":/status/ready"));
        ui->statusLabel->setText("Waiting...");
        ui->showButton->setEnabled(true);
        ui->configButton->setEnabled(true);
        ui->removeButton->setEnabled(false);
        d->editNameAction->setEnabled(true);
        break;
    case ModuleState::READY:
        ui->statusImage->setPixmap(QPixmap(":/status/ready"));
        ui->statusLabel->setText("Ready.");
        ui->showButton->setEnabled(true);
        ui->configButton->setEnabled(true);
        ui->removeButton->setEnabled(true);
        d->editNameAction->setEnabled(true);
        break;
    case ModuleState::RUNNING:
        ui->statusImage->setPixmap(QPixmap(":/status/running"));
        ui->statusLabel->setText("Running...");
        ui->removeButton->setEnabled(false);
        d->editNameAction->setEnabled(false);
        break;
    case ModuleState::ERROR:
        ui->statusImage->setPixmap(QPixmap(":/status/error"));
        ui->statusLabel->setText("Error!");
        ui->removeButton->setEnabled(true);
        d->editNameAction->setEnabled(true);
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
    auto mod = qobject_cast<AbstractModule*>(sender());
    auto errorTitle = QStringLiteral("Unknown module error");
    if (mod != nullptr)
        errorTitle = QStringLiteral("Error in: %1").arg(mod->name());

    ui->infoLabel->setText(message);
    QMessageBox::critical(this, errorTitle, message);
}

void ModuleIndicator::receiveMessage(const QString &message)
{
    ui->infoLabel->setText(message);

    // update status change immediately in UI
    QApplication::processEvents();
}

void ModuleIndicator::on_configButton_clicked()
{
    if (d->module == nullptr) return;
    if (d->module->isSettingsUiVisible())
        d->module->hideSettingsUi();
    else
        d->module->showSettingsUi();
}

void ModuleIndicator::on_modulePreRemove(AbstractModule *mod)
{
    if (mod == d->module) {
        d->module = nullptr;
        this->deleteLater();
    }
}

void ModuleIndicator::on_editNameActionTriggered()
{
    if (d->module == nullptr) return;
    bool ok;
    auto text = QInputDialog::getText(this, QStringLiteral("Edit module name"),
                                      QStringLiteral("New name for '%1' module:").arg(d->module->id()), QLineEdit::Normal,
                                      d->module->name(), &ok);
    if (ok && !text.isEmpty())
        d->module->setName(text);
}

void ModuleIndicator::on_showButton_clicked()
{
    if (d->module == nullptr) return;
    if (d->module->isDisplayUiVisible())
        d->module->hideDisplayUi();
    else
        d->module->showDisplayUi();
}

void ModuleIndicator::on_removeButton_clicked()
{
    if (d->manager != nullptr) {
        if (d->manager->removeModule(d->module))
            ui->infoLabel->setText("Deleted.");
    }
}
