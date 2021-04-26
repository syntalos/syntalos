/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "intanrhxmodule.h"

#include <QTimer>

#include "boardselectdialog.h"
#include "chanexportdialog.h"

SYNTALOS_MODULE(IntanRhxModule)


QString IntanRhxModuleInfo::id() const
{
    return QStringLiteral("intan-rhx");
}

QString IntanRhxModuleInfo::name() const
{
    return QStringLiteral("Intan RHX");
}

QString IntanRhxModuleInfo::description() const
{
    return QStringLiteral("Record electrophysiological signals from any Intan RHD or RHS system using "
                          "an RHD USB interface board, RHD recording controller, or RHS stim/recording controller.");
}

QString IntanRhxModuleInfo::license() const
{
    return QStringLiteral("Copyright (c) 2020-2021 <a href=\"https://intantech.com/\">Intan Technologies</a> [GPLv3+]");
}

QIcon IntanRhxModuleInfo::icon() const
{
    return QIcon(":/module/intan-rhx");
}

bool IntanRhxModuleInfo::singleton() const
{
    return true;
}

AbstractModule *IntanRhxModuleInfo::createModule(QObject *parent)
{
    return new IntanRhxModule(parent);
}

IntanRhxModule::IntanRhxModule(QObject *parent)
    : AbstractModule(parent),
      m_chanExportDlg(nullptr),
      m_sysState(nullptr)
{
    m_boardSelectDlg = new BoardSelectDialog;
    m_ctlWindow = m_boardSelectDlg->getControlWindow();
    if (m_ctlWindow != nullptr)
        m_ctlWindow->hide();
    m_sysState = m_boardSelectDlg->systemState();
    m_controllerIntf = m_boardSelectDlg->getControllerInterface();

    m_boardSelectDlg->setWindowIcon(QIcon(":/module/intan-rhx"));
    m_ctlWindow->setWindowIcon(QIcon(":/module/intan-rhx"));

    if (m_sysState == nullptr)
        return;
}

IntanRhxModule::~IntanRhxModule()
{
    delete m_boardSelectDlg;
    delete m_ctlWindow;
}

bool IntanRhxModule::initialize()
{
    if (m_ctlWindow == nullptr) {
        raiseError(QStringLiteral("No reference to control window found. This is an internal error."));
        return false;
    }
    if (m_sysState == nullptr) {
        raiseError(QStringLiteral("Failed to initialize module."));
        return false;
    }

    m_chanExportDlg = new ChanExportDialog(m_sysState);
    addSettingsWindow(m_chanExportDlg);
    addDisplayWindow(m_ctlWindow, false);

    connect(m_chanExportDlg, &ChanExportDialog::exportedChannelsChanged,
            this, &IntanRhxModule::onExportedChannelsChanged);

    return true;
}

ModuleFeatures IntanRhxModule::features() const
{
    return ModuleFeature::CALL_UI_EVENTS |
           ModuleFeature::SHOW_SETTINGS |
            ModuleFeature::SHOW_DISPLAY;
}

ModuleDriverKind IntanRhxModule::driver() const
{
    return ModuleDriverKind::NONE;
}

void IntanRhxModule::updateStartWaitCondition(OptionalWaitCondition *waitCondition)
{
    m_controllerIntf->updateStartWaitCondition(this, waitCondition);
}

bool IntanRhxModule::prepare(const TestSubject &)
{
    // the Intan module is a singleton, so we can "grab" this very generic name here
    auto dstore = getOrCreateDefaultDataset(QStringLiteral("intan-signals"));

    // we use the ugly scanning method -for now
    dstore->setDataScanPattern(QStringLiteral("*.rhd"));
    dstore->setAuxDataScanPattern(QStringLiteral("*.tsync"));

    const auto intanBasePart = QStringLiteral("%1_data.rhd").arg(dstore->collectionId().toString(QUuid::WithoutBraces).left(4));
    const auto intanBaseFilename = dstore->pathForDataBasename(intanBasePart);
    if (intanBaseFilename.isEmpty())
        return false;
    m_ctlWindow->setSaveFilenameTemplate(intanBaseFilename);

    // run (but wait for the starting signal)
    m_ctlWindow->recordControllerSlot();
    return true;
}

void IntanRhxModule::processUiEvents()
{
    m_controllerIntf->controllerRunIter();
}

void IntanRhxModule::start()
{
    AbstractModule::start();
}

void IntanRhxModule::stop()
{
    m_ctlWindow->stopControllerSlot();
    AbstractModule::stop();
}

void IntanRhxModule::onExportedChannelsChanged(const QList<Channel *> &channels)
{
    // reset all our ports, we are adding new ones
    clearOutPorts();
    clearInPorts();

    // add new ports
    for (const auto &channel : channels) {
        bool isDigital = (channel->getSignalType() == BoardDigitalInSignal) ||
                         (channel->getSignalType() == BoardDigitalOutSignal);
        if (isDigital) {
            if ((int) intSdiByGroupChannel.size() <= channel->getGroupID())
                intSdiByGroupChannel.resize(channel->getGroupID() + 1);
            if ((int) intSdiByGroupChannel[channel->getGroupID()].size() <= channel->getNativeChannelNumber())
                intSdiByGroupChannel[channel->getGroupID()].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<IntSignalBlock> sdi(channel->getGroupID(), channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<IntSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());

            intSdiByGroupChannel[channel->getGroupID()][channel->getNativeChannelNumber()] = sdi;
        } else {
            if ((int) floatSdiByGroupChannel.size() <= channel->getGroupID())
                floatSdiByGroupChannel.resize(channel->getGroupID() + 1);
            if ((int) floatSdiByGroupChannel[channel->getGroupID()].size() <= channel->getNativeChannelNumber())
                floatSdiByGroupChannel[channel->getGroupID()].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<FloatSignalBlock> sdi(channel->getGroupID(), channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<FloatSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());

            floatSdiByGroupChannel[channel->getGroupID()][channel->getNativeChannelNumber()] = sdi;
        }
    }
}
