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
#include "engine.h"
#include "streams/frametype.h"
#include "utils/misc.h"

ModuleGraphForm::ModuleGraphForm(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ModuleGraphForm),
      m_engine(new Engine(this)),
      m_modifyPossible(true),
      m_shutdown(false)
{
    ui->setupUi(this);

    // connect up engine events
    connect(m_engine, &Engine::moduleCreated, this, &ModuleGraphForm::moduleAdded);
    connect(m_engine, &Engine::modulePreRemove, this, &ModuleGraphForm::on_modulePreRemove);

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
    connect(ui->graphView, &FlowGraphView::connected, this, &ModuleGraphForm::on_graphPortsConnected);
    connect(ui->graphView, &FlowGraphView::disconnected, this, &ModuleGraphForm::on_graphPortsDisconnected);

    // set colors for our different data types
    ui->graphView->setPortTypeColor(qMetaTypeId<ControlCommand>(), QColor::fromRgb(0xEFF0F1));
    ui->graphView->setPortTypeColor(qMetaTypeId<Frame>(), QColor::fromRgb(0xECC386));
    ui->graphView->setPortTypeColor(qMetaTypeId<FirmataControl>(), QColor::fromRgb(0xc7abff));
    ui->graphView->setPortTypeColor(qMetaTypeId<FirmataData>(), QColor::fromRgb(0xD38DEF));
    ui->graphView->setPortTypeColor(qMetaTypeId<TableRow>(), QColor::fromRgb(0x8FD6FE));
    ui->graphView->setPortTypeColor(qMetaTypeId<IntSignalBlock>(), QColor::fromRgb(0x2ECC71));
    ui->graphView->setPortTypeColor(qMetaTypeId<FloatSignalBlock>(), QColor::fromRgb(0xAECC70));
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

Engine *ModuleGraphForm::engine() const
{
    return m_engine;
}

bool ModuleGraphForm::modifyPossible() const
{
    return m_modifyPossible;
}

void ModuleGraphForm::setModifyPossible(bool allowModify)
{
    m_modifyPossible = allowModify;

    ui->actionAddModule->setEnabled(m_modifyPossible);
    ui->actionRemove->setEnabled(m_modifyPossible);
    ui->actionConnect->setEnabled(m_modifyPossible);
    ui->actionDisconnect->setEnabled(m_modifyPossible);
    ui->graphView->setAllowEdit(m_modifyPossible);
}

void ModuleGraphForm::moduleAdded(ModuleInfo *info, AbstractModule *mod)
{
    connect(mod, &AbstractModule::stateChanged, this, &ModuleGraphForm::receiveStateChange);
    connect(mod, &AbstractModule::error, this, &ModuleGraphForm::receiveErrorMessage, Qt::QueuedConnection);
    connect(mod, &AbstractModule::statusMessage, this, &ModuleGraphForm::receiveMessage);
    connect(mod, &AbstractModule::portsConnected, this, &ModuleGraphForm::on_portsConnected);

    auto node = new FlowGraphNode(mod);
    node->setNodeIcon(info->icon());
    node->setShadowColor(info->color());
    for (auto &iport : mod->inPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(iport));
    for (auto &oport : mod->outPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(oport));
    ui->graphView->addItem(node);
    m_modNodeMap.insert(mod, node);

    connect(mod, &AbstractModule::nameChanged, [=](const QString& name) {
        node->setNodeTitle(name);
    });

    // we intentionally only connect this now, all previous emissions were not interesting as we just updated
    // the visual port representation to its actual state
    connect(mod, &AbstractModule::portConfigurationUpdated, this, &ModuleGraphForm::on_modulePortConfigChanged);
}

void ModuleGraphForm::on_actionAddModule_triggered()
{
    ModuleSelectDialog modDialog(m_engine->library()->moduleInfo(), this);
    if (modDialog.exec() == QDialog::Accepted) {
        emit busyStart();
        if (!modDialog.selectedEntryId().isEmpty()) {
            m_engine->createModule(modDialog.selectedEntryId());
        }
        emit busyEnd();
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
    if (node == nullptr || node->module() == nullptr) {
        qCritical() << "Orphaned node" << node->nodeName() << ", can not change name";
        return;
    }
    node->module()->setName(simplifyStrForModuleName(name));
    node->setNodeTitle(node->module()->name());
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
        ui->actionConnect->setEnabled(m_modifyPossible);
        ui->actionDisconnect->setEnabled(m_modifyPossible);
    }

    auto node = selectedSingleNode();
    m_menu->clear();
    if (node == nullptr) {
        ui->actionMenu->setEnabled(false);
        ui->actionRemove->setEnabled(false);
        ui->actionDisplay->setEnabled(false);
        ui->actionSettings->setEnabled(false);
    } else {
        auto mod = node->module();
        if (mod == nullptr)
            return;

        ui->actionRemove->setEnabled(m_modifyPossible);

        const auto features = mod->features();
        if (features.testFlag(ModuleFeature::SHOW_DISPLAY))
            ui->actionDisplay->setEnabled(true);
        if (features.testFlag(ModuleFeature::SHOW_SETTINGS))
            ui->actionSettings->setEnabled(true);

        for (auto &action : mod->actions())
            m_menu->addAction(action);
        ui->actionMenu->setEnabled(!m_menu->isEmpty());
    }
}

void ModuleGraphForm::on_graphPortsConnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    // sanity check
    if (!m_modifyPossible) {
        qCritical() << "Tried to connect ports while board modifications were prohibited.";
        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    VarStreamInputPort *inPort = nullptr;
    StreamOutputPort *outPort = nullptr;
    if (port1->isInput())
        inPort = dynamic_cast<VarStreamInputPort*>(port1->streamPort().get());
    if (port2->isInput())
        inPort = dynamic_cast<VarStreamInputPort*>(port2->streamPort().get());
    if (port1->isOutput())
        outPort = dynamic_cast<StreamOutputPort*>(port1->streamPort().get());
    if (port2->isOutput())
        outPort = dynamic_cast<StreamOutputPort*>(port1->streamPort().get());

    if ((inPort == nullptr || outPort == nullptr)) {
        // something went wrong or we connected two ports of the same type
        qWarning().noquote() << "Attempt to connect possibly incompatible ports failed.";

        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    if (!inPort->acceptsSubscription(outPort->dataTypeName())) {
        qWarning().noquote() << "Tried to connect incompatible ports.";
        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    // check if we already are connected - if so, don't connect twice
    if (inPort->hasSubscription()) {
        if (inPort->outPort() == outPort)
            return;
    }

    inPort->setSubscription(outPort, outPort->subscribe());
    qDebug().noquote() << "Connected ports:"
                       << QString("%1[>%2]").arg(outPort->title()).arg(outPort->dataTypeName())
                       << "->"
                       << QString("%1[<%2]").arg(inPort->title()).arg(inPort->dataTypeName());
}

void ModuleGraphForm::on_graphPortsDisconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    // sanity check
    if (!m_modifyPossible) {
        qCritical() << "Disconnected ports in graph UI although board modifications were prohibited. This is a bug.";
        return;
    }

    VarStreamInputPort *inPort = nullptr;
    StreamOutputPort *outPort = nullptr;
    if (port1->isInput())
        inPort = dynamic_cast<VarStreamInputPort*>(port1->streamPort().get());
    if (port2->isInput())
        inPort = dynamic_cast<VarStreamInputPort*>(port2->streamPort().get());
    if (port1->isOutput())
        outPort = dynamic_cast<StreamOutputPort*>(port1->streamPort().get());
    if (port2->isOutput())
        outPort = dynamic_cast<StreamOutputPort*>(port1->streamPort().get());

    if (inPort == nullptr || outPort == nullptr) {
        qCritical() << "Disconnected nonexisting ports. This should not be possible.";
        return;
    }

    // unsubscribing the input port will automatically remove the subscription from the output port
    // as well.
    const auto subscriptionExisted = inPort->hasSubscription();
    inPort->resetSubscription();
    if (subscriptionExisted)
        qDebug().noquote() << "Disconnected ports:"
                           << QString("%1[>%2]").arg(outPort->title()).arg(outPort->dataTypeName())
                           << "->"
                           << QString("%1[<%2]").arg(inPort->title()).arg(inPort->dataTypeName());
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
    auto mod = node->module();
    if (mod == nullptr)
        return;
    mod->showSettingsUi();
}

void ModuleGraphForm::on_actionDisplay_triggered()
{
    const auto node = selectedSingleNode();
    if (node == nullptr)
        return;
    auto mod = node->module();
    if (mod == nullptr)
        return;
    mod->showDisplayUi();
}

void ModuleGraphForm::on_actionRemove_triggered()
{
    const auto node = selectedSingleNode();
    if (node == nullptr)
        return;
    auto mod = node->module();
    if (mod == nullptr)
        return;
    m_engine->removeModule(mod);
}

void ModuleGraphForm::on_modulePreRemove(AbstractModule *mod)
{
    if (m_shutdown)
        return;
    auto node = m_modNodeMap.value(mod);

    // module removals invalidate our connection memory
    m_connMemory.clear();

    // sanity check
    if (node == nullptr) {
        qCritical() << "Module " << mod->name() << "without node representation is being removed.";
        return;
    }

    m_modNodeMap.remove(mod);
    ui->graphView->removeItem(node);
    delete node;
}

void ModuleGraphForm::on_portsConnected(const VarStreamInputPort *inPort, const StreamOutputPort *outPort)
{
    const auto inNode = m_modNodeMap.value(inPort->owner());
    const auto outNode = m_modNodeMap.value(outPort->owner());

    if ((inNode == nullptr) || (outNode == nullptr)) {
        qCritical() << "Ports of modules were connected, but we could not find one or both of their graph nodes.";
        return;
    }

    const auto graphInPort = inNode->findPort(inPort->id(), FlowGraphNodePort::Input, inPort->dataTypeId());
    const auto graphOutPort = outNode->findPort(outPort->id(), FlowGraphNodePort::Output, outPort->dataTypeId());
    ui->graphView->connectItems(graphOutPort, graphInPort);
}

void ModuleGraphForm::on_modulePortConfigChanged()
{
    auto mod = qobject_cast<AbstractModule*>(sender());
    if (mod == nullptr) {
        qCritical().noquote() << "Port configuration of an unknown module has changed.";
        return;
    }
    auto node = m_modNodeMap.value(mod);

    // Re-read all port information in the rare event that the module decides to update ports
    // after it was created.
    // This usually happens only on user-configured modules and is pretty rare (so this function
    // is currently really inefficient)

    // save maping of the old connections, so we can - possibly - restore them later
    for (const auto &port : node->ports()) {
        for (const auto &conn : port->connects()) {
            auto otherPort = conn->port1();
            if (otherPort == port)
                otherPort = conn->port2();

            m_connMemory[mod->name()+port->streamPort()->id()] = qMakePair(otherPort->portNode(), otherPort->streamPort()->id());
        }
    }

    // refresh ports to align view with what the module currently has
    node->removePorts();
    for (auto &iport : mod->inPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(iport));
    for (auto &oport : mod->outPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(oport));

    // restore connections for ports which have the same ID
    for (const auto &port : node->ports()) {
        const auto pairing = m_connMemory.value(mod->name()+port->streamPort()->id());
        if (pairing.first == nullptr)
            continue;
        const auto otherNode = pairing.first;
        FlowGraphNodePort *otherPort = nullptr;
        for (const auto &op : otherNode->ports()) {
            if (op->streamPort()->id() == pairing.second) {
                otherPort = op;
                break;
            }
        }
        if (otherPort == nullptr)
            continue;
        ui->graphView->connectItems(port, otherPort);
    }

    ui->graphView->updatePortTypeColors();
}
