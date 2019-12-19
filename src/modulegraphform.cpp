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

#include "modulegraphform.h"
#include "ui_modulegraphform.h"

#include <QObject>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QDebug>

#include "moduleselectdialog.h"
#include "moduleapi.h"
#include "modulemanager.h"

ModuleGraphForm::ModuleGraphForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ModuleGraphForm),
    m_shutdown(false)
{
    ui->setupUi(this);

    m_modManager = new ModuleManager(parent? parent : this);
    connect(m_modManager, &ModuleManager::moduleCreated, this, &ModuleGraphForm::moduleAdded);

    ui->actionMenu->setEnabled(false);
    ui->actionRemove->setEnabled(false);
    ui->actionConnect->setEnabled(false);
    ui->actionDisconnect->setEnabled(false);
    ui->actionDisplay->setEnabled(false);
    ui->actionSettings->setEnabled(false);

    m_menu = new QMenu(this);
    ui->actionMenu->setMenu(m_menu);
    connect(ui->actionMenu, &QAction::triggered,
            qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionMenu)),
            &QToolButton::showMenu);

    connect(ui->graphView->scene(), &QGraphicsScene::selectionChanged, this, &ModuleGraphForm::on_selectionChanged);
    connect(ui->graphView, &FlowGraphView::renamed, this, &ModuleGraphForm::itemRenamed);
    connect(ui->graphView, &FlowGraphView::connected, this, &ModuleGraphForm::on_portsConnected);
    connect(ui->graphView, &FlowGraphView::disconnected, this, &ModuleGraphForm::on_portsDisconnected);
    connect(m_modManager, &ModuleManager::modulePreRemove, this, &ModuleGraphForm::on_modulePreRemove);

    // test area
    auto tn = new FlowGraphNode("TestDuplex", FlowGraphItem::Duplex);
    tn->addInputPort("InputData");
    tn->addOutputPort("OutputDataPort1");
    tn->addOutputPort("OutputDataPort2");

    auto ts = new FlowGraphNode("TestSink", FlowGraphItem::Input);
    ts->addInputPort("InputData");

    graphView()->addItem(tn);
    graphView()->addItem(ts);
}

ModuleGraphForm::~ModuleGraphForm()
{
    m_shutdown = true; // ignore some pending events while we are deleting the UI
    delete ui;
}

FlowGraphView *ModuleGraphForm::graphView() const
{
    return ui->graphView;
}

ModuleManager *ModuleGraphForm::moduleManager() const
{
    return m_modManager;
}

bool ModuleGraphForm::modifyPossible() const
{
    return m_modifyPossible;
}

void ModuleGraphForm::setModifyPossible(bool allowModify)
{
    m_modifyPossible = allowModify;

    ui->actionAddModule->setEnabled(m_modifyPossible);
}

void ModuleGraphForm::moduleAdded(ModuleInfo *info, AbstractModule *mod)
{
    auto node = new FlowGraphNode(mod->name(), FlowGraphItem::Duplex);
    node->setNodeIcon(info->pixmap());
    Q_FOREACH(auto iport, mod->inPorts())
        node->addInputPort(iport->id());
    Q_FOREACH(auto oport, mod->outPorts())
        node->addOutputPort(oport->id());
    ui->graphView->addItem(node);
    m_nodeModMap.insert(node, mod);
    m_modNodeMap.insert(mod, node);

    connect(mod, &AbstractModule::nameChanged, [=](const QString& name) {
        node->setNodeTitle(name);
    });
}

void ModuleGraphForm::on_actionAddModule_triggered()
{
    ModuleSelectDialog modDialog(m_modManager->moduleInfo(), this);
    if (modDialog.exec() == QDialog::Accepted) {
        //m_runIndicatorWidget->show();
        if (!modDialog.selectedEntryId().isEmpty()) {
            auto mod = m_modManager->createModule(modDialog.selectedEntryId());

            connect(mod, &AbstractModule::stateChanged, this, &ModuleGraphForm::receiveStateChange);
            connect(mod, &AbstractModule::error, this, &ModuleGraphForm::receiveErrorMessage);
            connect(mod, &AbstractModule::statusMessage, this, &ModuleGraphForm::receiveMessage);

            mod->showSettingsUi();
        }
        //m_runIndicatorWidget->hide();
    }
}

void ModuleGraphForm::receiveStateChange(ModuleState state)
{
    const auto mod = qobject_cast<AbstractModule*>(sender());
    const auto node = m_modNodeMap.value(mod);
    if (node == nullptr)
        return;

    node->updateNodeState(state);
}

void ModuleGraphForm::receiveErrorMessage(const QString &message)
{
    const auto mod = qobject_cast<AbstractModule*>(sender());
    const auto node = m_modNodeMap.value(mod);
    auto errorTitle = QStringLiteral("Unknown module error");
    if (mod != nullptr)
        errorTitle = QStringLiteral("Error in: %1").arg(mod->name());

    if (node != nullptr)
        node->setNodeInfoText(message);

    QMessageBox::critical(this, errorTitle, message);
}

void ModuleGraphForm::receiveMessage(const QString &message)
{
    if (m_shutdown)
        return;

    const auto mod = qobject_cast<AbstractModule*>(sender());
    const auto node = m_modNodeMap.value(mod);
    if (node == nullptr)
        return;
    node->setNodeInfoText(message);
}

void ModuleGraphForm::itemRenamed(FlowGraphItem *item, const QString &name)
{
    if (item->type() != FlowGraphNode::Type)
        return;
    auto node = static_cast<FlowGraphNode*>(item);
    auto mod = m_nodeModMap.value(node);
    if (mod == nullptr) {
        qCritical() << "Orphaned node" << node->nodeName() << ", can not change name";
        return;
    }
    mod->setName(name);
    node->setNodeTitle(name);
}

FlowGraphNode *ModuleGraphForm::selectedSingleNode() const
{
    const auto nodes = ui->graphView->selectedNodes();
    if (nodes.size() == 1)
        return nodes.first();
    return nullptr;
}

void ModuleGraphForm::on_selectionChanged()
{
    if (m_shutdown)
        return;

    const auto items = ui->graphView->scene()->selectedItems();
    if (items.count() < 2) {
        ui->actionConnect->setEnabled(false);
        ui->actionDisconnect->setEnabled(false);
    } else {
        ui->actionConnect->setEnabled(true);
        ui->actionDisconnect->setEnabled(true);
    }

    auto node = selectedSingleNode();
    m_menu->clear();
    if (node == nullptr) {
        ui->actionMenu->setEnabled(false);
        ui->actionRemove->setEnabled(false);
        ui->actionDisplay->setEnabled(false);
        ui->actionSettings->setEnabled(false);
    } else {
        auto mod = m_nodeModMap.value(node);
        if (mod == nullptr)
            return;

        ui->actionRemove->setEnabled(true);

        const auto features = mod->features();
        if (features.testFlag(ModuleFeature::SHOW_DISPLAY))
            ui->actionDisplay->setEnabled(true);
        if (features.testFlag(ModuleFeature::SHOW_SETTINGS))
            ui->actionSettings->setEnabled(true);

        Q_FOREACH(auto action, mod->actions())
            m_menu->addAction(action);
        ui->actionMenu->setEnabled(!m_menu->isEmpty());
    }
}

void ModuleGraphForm::on_portsConnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    Q_UNUSED(port1)
    Q_UNUSED(port2)
}

void ModuleGraphForm::on_portsDisconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    Q_UNUSED(port1)
    Q_UNUSED(port2)
}

void ModuleGraphForm::on_actionConnect_triggered()
{
    ui->graphView->connectItems();
}

void ModuleGraphForm::on_actionDisconnect_triggered()
{
    ui->graphView->disconnectItems();
}

void ModuleGraphForm::on_actionSettings_triggered()
{
    const auto node = selectedSingleNode();
    if (node == nullptr)
        return;
    auto mod = m_nodeModMap.value(node);
    if (mod == nullptr)
        return;
    mod->showSettingsUi();
}

void ModuleGraphForm::on_actionDisplay_triggered()
{
    const auto node = selectedSingleNode();
    if (node == nullptr)
        return;
    auto mod = m_nodeModMap.value(node);
    if (mod == nullptr)
        return;
    mod->showDisplayUi();
}

void ModuleGraphForm::on_actionRemove_triggered()
{
    const auto node = selectedSingleNode();
    if (node == nullptr)
        return;
    auto mod = m_nodeModMap.value(node);
    if (mod == nullptr)
        return;
    m_modManager->removeModule(mod);
}

void ModuleGraphForm::on_modulePreRemove(AbstractModule *mod)
{
    if (m_shutdown)
        return;
    auto node = m_modNodeMap.value(mod);
    if (node == nullptr) {
        qCritical() << "Module " << mod->name() << "without node representation is being removed.";
        return;
    }

    m_modNodeMap.remove(mod);
    m_nodeModMap.remove(node);
    ui->graphView->removeItem(node);
    delete node;
}
