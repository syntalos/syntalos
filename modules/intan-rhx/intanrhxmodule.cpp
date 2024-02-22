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
#include <QMessageBox>

#include "utils/misc.h"
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
    return QStringLiteral("Copyright © 2020-2024 <a href=\"https://intantech.com/\">Intan Technologies</a> [GPL-3.0+]");
}

bool IntanRhxModuleInfo::singleton() const
{
    return true;
}

AbstractModule *IntanRhxModuleInfo::createModule(QObject *parent)
{
    return new IntanRhxModule(id(), this, parent);
}

IntanRhxModule::IntanRhxModule(const QString &id, IntanRhxModuleInfo *modInfo, QObject *parent)
    : AbstractModule(id, parent),
      m_chanExportDlg(nullptr),
      m_sysState(nullptr)
{
    blocksPerTimestamp = 5;
    m_boardSelectDlg = new BoardSelectDialog(this);
    m_modIcon = modInfo->icon();
    m_boardSelectDlg->setWindowIcon(m_modIcon);
}

IntanRhxModule::~IntanRhxModule()
{
    delete m_boardSelectDlg;
    if (m_ctlWindow != nullptr)
        delete m_ctlWindow;
}

bool IntanRhxModule::initialize()
{
    if (m_boardSelectDlg->getControlWindow() == nullptr)
        m_boardSelectDlg->exec();

    m_ctlWindow = m_boardSelectDlg->getControlWindow();
    m_sysState = m_boardSelectDlg->systemState();
    m_controllerIntf = m_boardSelectDlg->getControllerInterface();
    if (m_ctlWindow == nullptr) {
        raiseError(QStringLiteral("No reference to control window found. This is an internal error."));
        return false;
    }
    m_ctlWindow->setWindowIcon(m_modIcon);

    if ((m_sysState == nullptr) || (m_controllerIntf == nullptr)) {
        raiseError(QStringLiteral("Failed to initialize module."));
        return false;
    }

    m_chanExportDlg = new ChanExportDialog(m_sysState);
    m_chanExportDlg->setWindowIcon(m_modIcon);
    addSettingsWindow(m_chanExportDlg);
    addDisplayWindow(m_ctlWindow, false);

    connect(m_chanExportDlg, &ChanExportDialog::exportedChannelsChanged,
            this, &IntanRhxModule::onExportedChannelsChanged);

    // be nice and warn the user in case udev rules are missing
    if (!hostUdevRuleExists("90-syntalos-intan.rules")) {
        QMessageBox::warning(m_ctlWindow, QStringLiteral("Hardware configuration not installed"),
                             QStringLiteral("The hardware rules for Syntalos/Intan may not be installed on this system. "
                                            "This means the Intan hardware may not be accessible and may not work. "
                                            "Please install the necessary data (udev rules) on the host system!"),
                             QMessageBox::Ok);
    }

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
    m_controllerIntf->updateStartWaitCondition(waitCondition);
}

bool IntanRhxModule::prepare(const TestSubject &)
{
    // the Intan module is a singleton, so we can "grab" this very generic name here
    auto dstore = createDefaultDataset(QStringLiteral("intan-signals"));
    if (dstore.get() == nullptr)
        return false;

    // we use the ugly scanning method -for now
    dstore->setDataScanPattern(QStringLiteral("*.rhd"), QStringLiteral("Electrophysiology data"));
    dstore->addAuxDataScanPattern(QStringLiteral("*.tsync"), QStringLiteral("Timestamp synchronization information"));
    dstore->addAuxDataScanPattern(QStringLiteral("settings.xml"), QStringLiteral("Intan DAQ configuration used for this recording"));

    const auto intanBasePart = QStringLiteral("%1-data").arg(dstore->collectionShortTag());
    const auto intanBaseFilename = dstore->pathForDataBasename(intanBasePart);
    if (intanBaseFilename.isEmpty())
        return false;
    m_ctlWindow->setSaveFilenameTemplate(intanBaseFilename + QStringLiteral(".rhd"));

    m_controllerIntf->setDefaultRealtimePriority(defaultRealtimePriority());

    // set port metadata
    const auto sampleRate = m_controllerIntf->getRhxController()->getSampleRate();
    for (auto &blocks : intSdiByGroupChannel) {
        for (uint i = 0; i < blocks.size(); ++ i) {
            auto &sdi = blocks[i];
            if (!sdi.active)
                continue;
            sdi.stream->setMetadataValue(QStringLiteral("sample_rate"), sampleRate);
            sdi.stream->setMetadataValue("time_unit", "index");
            sdi.stream->setMetadataValue("data_unit", "µV");
            sdi.stream->setMetadataValue("signal_names", QStringList() << QStringLiteral("I%1").arg(i));
        }
    }
    for (auto &blocks : floatSdiByGroupChannel) {
        for (uint i = 0; i < blocks.size(); ++ i) {
            auto &sdi = blocks[i];
            if (!sdi.active)
                continue;
            sdi.stream->setMetadataValue(QStringLiteral("sample_rate"), sampleRate);
            sdi.stream->setMetadataValue("time_unit", "index");
            sdi.stream->setMetadataValue("data_unit", "µV");
            sdi.stream->setMetadataValue("signal_names", QStringList() << QStringLiteral("F%1").arg(i));
        }
    }

    // start output port streams
    for (auto &port : outPorts())
        port->startStream();

    // set up slave-clock synchronizer
    clockSync = initCounterSynchronizer(sampleRate);
    clockSync->setStrategies(TimeSyncStrategy::WRITE_TSYNCFILE);
    clockSync->setTimeSyncBasename(intanBaseFilename, dstore->collectionId());

    // permit 1.4ms tolerance - this was a very realistic tolerance to achieve in tests,
    // while lower values resulted in constant adjustment attempts
    clockSync->setTolerance(std::chrono::microseconds(1400));

    // we only permit calibration with the very first data block - this seems to be sufficient and
    // yielded the best results (due to device and USB buffering, the later data blocks are more
    // susceptible to error)
    clockSync->setCalibrationBlocksCount((sampleRate / RHXDataBlock::samplesPerDataBlock(m_controllerIntf->getRhxController()->getType())) * 20);

    if (!clockSync->start()) {
        raiseError(QStringLiteral("Unable to set up timestamp synchronizer!"));
        return false;
    }

    // call stop, to catch the case when a user was starting an experiment run while also
    // having an ongoing sweep action
    m_ctlWindow->stopControllerSlot();

    currentBlockIdx = 0;

    // run (but wait for the starting signal)
    m_ctlWindow->recordControllerSlot();
    return true;
}

void IntanRhxModule::start()
{
    m_controllerIntf->startDAQWithSyntalosStartTime(m_syTimer->startTime());
    AbstractModule::start();
}

void IntanRhxModule::processUiEvents()
{
    if (!m_controllerIntf->controllerRunIter())
        m_ctlWindow->stopAndReportAnyErrors();
}

void IntanRhxModule::stop()
{
    m_controllerIntf->updateStartWaitCondition(nullptr);
    m_ctlWindow->stopControllerSlot();

    // since the synchronizer sees data that never gets written to disk, due to IntanRHX' threading implementation,
    // we add a very dirty hack here to get close to the last datapoint, but not quite there (usually not more than two
    // data blocks are missing from the final file)
    const auto timePerPointUs = (1.0 / m_controllerIntf->getRhxController()->getSampleRate()) * 1000.0 * 1000.0;
    const auto fakeSemiLastDatapoint = clockSync->lastMasterAssumedAcqTS() -
                                       microseconds_t(std::lround(RHXDataBlock::samplesPerDataBlock(m_controllerIntf->getRhxController()->getType()) * 2 * timePerPointUs));
    safeStopSynchronizer(clockSync, fakeSemiLastDatapoint);

    AbstractModule::stop();
}

void IntanRhxModule::serializeSettings(const QString&, QVariantHash &settings, QByteArray &extraData)
{
    extraData = m_ctlWindow->globalSettingsAsByteArray();

    settings.insert("port_channel_names", m_chanExportDlg->exportedChannelNames());
}

bool IntanRhxModule::loadSettings(const QString &, const QVariantHash &settings, const QByteArray &extraData)
{
    if (!extraData.isEmpty()) {
        const auto ret = m_ctlWindow->globalSettingsFromByteArray(extraData);
        if (!ret)
            return ret;
    }

    m_chanExportDlg->removeAllChannels();
    const auto exportedChannelNames = settings.value("port_channel_names").toStringList();
    for (const auto &chanName : exportedChannelNames)
        m_chanExportDlg->addChannel(chanName, false);
    m_chanExportDlg->updateExportChannelsTable();

    return true;
}

void IntanRhxModule::stopWithError(const QString &message)
{
    raiseError(message);
}

void IntanRhxModule::setPortSignalBlockSampleSize(size_t sampleNum)
{
    for (auto &blocks : intSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            sdi.signalBlock->timestamps.resize(sampleNum);
            sdi.signalBlock->data.resize(sampleNum, 1);
        }
    }

    for (auto &blocks : floatSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            sdi.signalBlock->timestamps.resize(sampleNum);
            sdi.signalBlock->data.resize(sampleNum, 1);
        }
    }
}

void IntanRhxModule::onExportedChannelsChanged(const QList<Channel *> &channels)
{
    // reset all our ports, we are adding new ones
    clearOutPorts();
    clearInPorts();
    intSdiByGroupChannel.clear();
    floatSdiByGroupChannel.clear();

    auto signalSources = m_sysState->signalSources;

    // add new ports
    for (const auto &channel : channels) {
        bool isDigital = (channel->getSignalType() == BoardDigitalInSignal) ||
                         (channel->getSignalType() == BoardDigitalOutSignal);
        if (isDigital) {
            const auto groupIndex = signalSources->groupIndexByName(channel->getGroupName());
            if ((int) intSdiByGroupChannel.size() <= groupIndex)
                intSdiByGroupChannel.resize(groupIndex + 1);
            if ((int) intSdiByGroupChannel[groupIndex].size() <= channel->getNativeChannelNumber())
                intSdiByGroupChannel[groupIndex].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<IntSignalBlock> sdi(groupIndex, channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<IntSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());
            sdi.active = true;

            intSdiByGroupChannel[groupIndex][channel->getNativeChannelNumber()] = sdi;
        } else {
            const auto groupIndex = signalSources->groupIndexByName(channel->getGroupName());
            if ((int) floatSdiByGroupChannel.size() <= groupIndex)
                floatSdiByGroupChannel.resize(groupIndex + 1);
            if ((int) floatSdiByGroupChannel[groupIndex].size() <= channel->getNativeChannelNumber())
                floatSdiByGroupChannel[groupIndex].resize(channel->getNativeChannelNumber() + 1);

            StreamDataInfo<FloatSignalBlock> sdi(groupIndex, channel->getNativeChannelNumber());
            sdi.stream = registerOutputPort<FloatSignalBlock>(channel->getNativeName(), channel->getNativeAndCustomNames());
            sdi.active = true;

            floatSdiByGroupChannel[groupIndex][channel->getNativeChannelNumber()] = sdi;
        }
    }
}
