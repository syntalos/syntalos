/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "modulegraphform.h"
#include "ui_modulegraphform.h"

#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QToolButton>

#include "engine.h"
#include "moduleapi.h"
#include "moduleselectdialog.h"
#include "datactl/frametype.h"
#include "utils/misc.h"
#include "utils/style.h"

namespace Syntalos
{
Q_LOGGING_CATEGORY(logGraphUi, "graphui")
}

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

    ui->actionRemove->setEnabled(false);
    ui->actionConnect->setEnabled(false);
    ui->actionDisconnect->setEnabled(false);
    ui->actionDisplay->setEnabled(false);
    ui->actionSettings->setEnabled(false);
    ui->actionModifiers->setEnabled(false);

    m_modifiersMenu = new QMenu(this);
    ui->actionModifiers->setMenu(m_modifiersMenu);
    connect(
        ui->actionModifiers,
        &QAction::triggered,
        qobject_cast<QToolButton *>(ui->toolBar->widgetForAction(ui->actionModifiers)),
        &QToolButton::showMenu);

    connect(ui->graphView->scene(), &QGraphicsScene::selectionChanged, this, &ModuleGraphForm::on_selectionChanged);
    connect(ui->graphView, &FlowGraphView::renamed, this, &ModuleGraphForm::itemRenamed);
    connect(ui->graphView, &FlowGraphView::connected, this, &ModuleGraphForm::on_graphPortsConnected);
    connect(ui->graphView, &FlowGraphView::disconnected, this, &ModuleGraphForm::on_graphPortsDisconnected);

    // set colors for our different data types
    ui->graphView->setPortTypeColor(ControlCommand::staticTypeId(), QColor::fromRgb(0xEFF0F1));
    ui->graphView->setPortTypeColor(Frame::staticTypeId(), QColor::fromRgb(0xECC386));
    ui->graphView->setPortTypeColor(FirmataControl::staticTypeId(), QColor::fromRgb(0xc7abff));
    ui->graphView->setPortTypeColor(FirmataData::staticTypeId(), QColor::fromRgb(0xD38DEF));
    ui->graphView->setPortTypeColor(TableRow::staticTypeId(), QColor::fromRgb(0x8FD6FE));
    ui->graphView->setPortTypeColor(IntSignalBlock::staticTypeId(), QColor::fromRgb(0x2ECC71));
    ui->graphView->setPortTypeColor(FloatSignalBlock::staticTypeId(), QColor::fromRgb(0xAECC70));

    // add rename ation to the menu
    auto renameAction = new QAction("Rename module", this);
    m_modifiersMenu->addAction(renameAction);
    connect(renameAction, &QAction::triggered, [this]() {
        auto node = selectedSingleNode();
        if (node == nullptr)
            return;
        ui->graphView->renameItem(node);
    });

    // add modifier actions to the menu
    auto enAction = new QAction("Enabled", this);
    m_modifierActions[ModuleModifier::ENABLED] = enAction;
    enAction->setCheckable(true);
    m_modifiersMenu->addAction(enAction);
    connect(enAction, &QAction::triggered, [this](bool checked) {
        auto node = selectedSingleNode();
        if (node == nullptr)
            return;
        auto mod = node->module();
        if (mod == nullptr)
            return;

        auto modifiers = mod->modifiers();
        modifiers.setFlag(ModuleModifier::ENABLED, checked);
        mod->setModifiers(modifiers);
    });

    auto errorAction = new QAction("Stop run on module failure", this);
    m_modifierActions[ModuleModifier::STOP_ON_FAILURE] = errorAction;
    errorAction->setCheckable(true);
    m_modifiersMenu->addAction(errorAction);
    connect(errorAction, &QAction::triggered, [this](bool checked) {
        auto node = selectedSingleNode();
        if (node == nullptr)
            return;
        auto mod = node->module();
        if (mod == nullptr)
            return;

        auto modifiers = mod->modifiers();
        modifiers.setFlag(ModuleModifier::STOP_ON_FAILURE, checked);
        mod->setModifiers(modifiers);
    });
}

ModuleGraphForm::~ModuleGraphForm()
{
    m_shutdown = true; // ignore some pending events while we are deleting the UI
    delete ui;
}

void ModuleGraphForm::updateIconStyles()
{
    bool isDark = currentThemeIsDark();
    setWidgetIconFromResource(ui->actionSettings, "settings", isDark);
    setWidgetIconFromResource(ui->actionModifiers, "menu", isDark);
    setWidgetIconFromResource(ui->actionDisplay, "show-all-windows", isDark);
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

FlowGraphEdge *ModuleGraphForm::updateConnectionHeat(
    const VarStreamInputPort *inPort,
    const StreamOutputPort *outPort,
    ConnectionHeatLevel hlevel)
{
    const auto inNode = m_modNodeMap.value(inPort->owner());
    const auto outNode = m_modNodeMap.value(outPort->owner());

    if ((inNode == nullptr) || (outNode == nullptr)) {
        qCCritical(logGraphUi).noquote() << "Unable to find port graph nodes to update edge heat level. Source owner:"
                                         << inPort->owner()->name();
        return nullptr;
    }

    const auto graphInPort = inNode->findPort(inPort->id(), FlowGraphNodePort::Input, inPort->dataTypeId());
    const auto graphOutPort = outNode->findPort(outPort->id(), FlowGraphNodePort::Output, outPort->dataTypeId());

    auto edge = graphInPort->findConnect(graphOutPort);
    if (edge == nullptr) {
        qCCritical(logGraphUi).noquote() << "Unable to find graph edge connecting" << inPort->owner()->name() << "and"
                                         << outPort->owner()->name() << "to update its heat level.";
        return nullptr;
    }

    edge->setHeatLevel(hlevel);
    return edge;
}

void ModuleGraphForm::moduleAdded(ModuleInfo *info, AbstractModule *mod)
{
    connect(mod, &AbstractModule::stateChanged, this, &ModuleGraphForm::receiveStateChange);
    connect(mod, &AbstractModule::error, this, &ModuleGraphForm::receiveErrorMessage, Qt::QueuedConnection);
    connect(mod, &AbstractModule::statusMessage, this, &ModuleGraphForm::receiveMessage);
    connect(mod, &AbstractModule::portsConnected, this, &ModuleGraphForm::on_portsConnected);
    connect(mod, &AbstractModule::modifiersUpdated, this, &ModuleGraphForm::on_moduleModifiersUpdated);

    auto node = new FlowGraphNode(mod);
    node->setNodeIcon(info->icon());
    node->setShadowColor(info->color());
    for (auto &iport : mod->inPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(iport));
    for (auto &oport : mod->outPorts())
        node->addPort(std::dynamic_pointer_cast<AbstractStreamPort>(oport));
    ui->graphView->addItem(node);
    m_modNodeMap.insert(mod, node);

    connect(mod, &AbstractModule::nameChanged, [=](const QString &name) {
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
        AbstractModule *mod = nullptr;
        if (!modDialog.selectedEntryId().isEmpty()) {
            mod = m_engine->createModule(modDialog.selectedEntryId());
        }
        emit busyEnd();

        if (mod) {
            QCoreApplication::processEvents();
            auto newNode = m_modNodeMap.value(mod, nullptr);

            // select the new node, if any was registered
            if (newNode) {
                ui->graphView->clearSelection();
                newNode->setSelected(true);
            }
        }
    }
}

void ModuleGraphForm::receiveStateChange(ModuleState state)
{
    const auto mod = qobject_cast<AbstractModule *>(sender());
    const auto node = m_modNodeMap.value(mod);
    if (node == nullptr)
        return;

    node->updateNodeState(state);
}

void ModuleGraphForm::receiveErrorMessage(const QString &message)
{
    const auto mod = qobject_cast<AbstractModule *>(sender());
    const auto node = m_modNodeMap.value(mod);

    if (node != nullptr) {
        node->setNodeInfoText(message);

        // update path immediately here instead of asynchronously, so
        // the node is shown correctly even if an error message box blocks
        // any UI updating further down.
        node->updatePath();
    }
}

void ModuleGraphForm::receiveMessage(const QString &message)
{
    if (m_shutdown)
        return;

    const auto mod = qobject_cast<AbstractModule *>(sender());
    const auto node = m_modNodeMap.value(mod);
    if (node == nullptr)
        return;
    node->setNodeInfoText(message);
}

void ModuleGraphForm::itemRenamed(FlowGraphItem *item, const QString &name)
{
    if (item->type() != FlowGraphNode::Type)
        return;
    auto node = static_cast<FlowGraphNode *>(item);
    if (node == nullptr || node->module() == nullptr) {
        qCCritical(logGraphUi).noquote() << "Orphaned node" << node->nodeName() << ", can not change name";
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

    ui->actionRemove->setEnabled(false);
    ui->actionDisplay->setEnabled(false);
    ui->actionSettings->setEnabled(false);
    ui->actionModifiers->setEnabled(false);

    auto node = selectedSingleNode();
    if (node == nullptr)
        return;

    auto mod = node->module();
    if (mod == nullptr)
        return;

    ui->actionRemove->setEnabled(m_modifyPossible);
    ui->actionModifiers->setEnabled(m_modifyPossible);

    m_modifierActions[ModuleModifier::ENABLED]->setChecked(mod->modifiers().testFlag(ModuleModifier::ENABLED));
    m_modifierActions[ModuleModifier::STOP_ON_FAILURE]->setChecked(
        mod->modifiers().testFlag(ModuleModifier::STOP_ON_FAILURE));

    const auto features = mod->features();
    if (features.testFlag(ModuleFeature::SHOW_DISPLAY))
        ui->actionDisplay->setEnabled(true);
    if (features.testFlag(ModuleFeature::SHOW_SETTINGS))
        ui->actionSettings->setEnabled(true);
}

void ModuleGraphForm::on_graphPortsConnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    // sanity check
    if (!m_modifyPossible) {
        qCCritical(logGraphUi).noquote() << "Tried to connect ports while board modifications were prohibited.";
        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    VarStreamInputPort *inPort = nullptr;
    StreamOutputPort *outPort = nullptr;
    if (port1->isInput())
        inPort = dynamic_cast<VarStreamInputPort *>(port1->streamPort().get());
    if (port2->isInput())
        inPort = dynamic_cast<VarStreamInputPort *>(port2->streamPort().get());
    if (port1->isOutput())
        outPort = dynamic_cast<StreamOutputPort *>(port1->streamPort().get());
    if (port2->isOutput())
        outPort = dynamic_cast<StreamOutputPort *>(port1->streamPort().get());

    if ((inPort == nullptr || outPort == nullptr)) {
        // something went wrong or we connected two ports of the same type
        qCWarning(logGraphUi).noquote() << "Attempt to connect possibly incompatible ports failed.";

        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    if (!inPort->acceptsSubscription(outPort->dataTypeName())) {
        qCWarning(logGraphUi).noquote().nospace() << "Tried to connect incompatible ports. (" << outPort->dataTypeName()
                                                  << " -> " << inPort->dataTypeName() << ")";
        ui->graphView->disconnectItems(port1, port2);
        return;
    }

    // check if we already are connected - if so, don't connect twice
    if (inPort->hasSubscription()) {
        if (inPort->outPort() == outPort)
            return;
    }

    inPort->setSubscription(outPort, outPort->subscribe());
    qCDebug(logGraphUi).noquote() << "Connected ports:"
                                  << QString("%1[>%2]").arg(outPort->title()).arg(outPort->dataTypeName()) << "->"
                                  << QString("%1[<%2]").arg(inPort->title()).arg(inPort->dataTypeName());
}

void ModuleGraphForm::on_graphPortsDisconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2)
{
    // sanity check
    if (!m_modifyPossible) {
        qCCritical(logGraphUi).noquote()
            << "Disconnected ports in graph UI although board modifications were prohibited. This is a bug.";
        return;
    }

    VarStreamInputPort *inPort = nullptr;
    StreamOutputPort *outPort = nullptr;
    if (port1->isInput())
        inPort = dynamic_cast<VarStreamInputPort *>(port1->streamPort().get());
    if (port2->isInput())
        inPort = dynamic_cast<VarStreamInputPort *>(port2->streamPort().get());
    if (port1->isOutput())
        outPort = dynamic_cast<StreamOutputPort *>(port1->streamPort().get());
    if (port2->isOutput())
        outPort = dynamic_cast<StreamOutputPort *>(port1->streamPort().get());

    if (inPort == nullptr || outPort == nullptr) {
        qCCritical(logGraphUi).noquote() << "Disconnected nonexisting ports. This should not be possible.";
        return;
    }

    // unsubscribing the input port will automatically remove the subscription from the output port
    // as well.
    const auto subscriptionExisted = inPort->hasSubscription();
    inPort->resetSubscription();
    if (subscriptionExisted)
        qCDebug(logGraphUi).noquote() << "Disconnected ports:"
                                      << QString("%1[>%2]").arg(outPort->title()).arg(outPort->dataTypeName()) << "->"
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
        qCCritical(logGraphUi).noquote() << "Module " << mod->name() << "without node representation is being removed.";
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
        qCCritical(logGraphUi).noquote()
            << "Ports of modules were connected, but we could not find one or both of their graph nodes.";
        return;
    }

    const auto graphInPort = inNode->findPort(inPort->id(), FlowGraphNodePort::Input, inPort->dataTypeId());
    const auto graphOutPort = outNode->findPort(outPort->id(), FlowGraphNodePort::Output, outPort->dataTypeId());
    ui->graphView->connectItems(graphOutPort, graphInPort);
}

void ModuleGraphForm::on_modulePortConfigChanged()
{
    auto mod = qobject_cast<AbstractModule *>(sender());
    if (mod == nullptr) {
        qCCritical(logGraphUi).noquote() << "Port configuration of an unknown module has changed.";
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

            m_connMemory[mod->name() + port->streamPort()->id()] = qMakePair(
                otherPort->portNode(), otherPort->streamPort()->id());
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
        const auto pairing = m_connMemory.value(mod->name() + port->streamPort()->id());
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

void ModuleGraphForm::on_moduleModifiersUpdated()
{
    const auto mod = qobject_cast<AbstractModule *>(sender());
    const auto node = m_modNodeMap.value(mod);
    if (node == nullptr)
        return;

    const auto modifiers = mod->modifiers();

    m_modifierActions[ModuleModifier::ENABLED]->setChecked(modifiers.testFlag(ModuleModifier::ENABLED));
    m_modifierActions[ModuleModifier::STOP_ON_FAILURE]->setChecked(modifiers.testFlag(ModuleModifier::STOP_ON_FAILURE));

    node->setOpacity(modifiers.testFlag(ModuleModifier::ENABLED) ? 1.0 : 0.6);
    node->setStopOnErrorAttribute(modifiers.testFlag(ModuleModifier::STOP_ON_FAILURE));
}
