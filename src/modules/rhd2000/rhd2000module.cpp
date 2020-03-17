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

#include "rhd2000module.h"

#include <QTimer>

#include "intanui.h"
#include "signalsources.h"
#include "signalgroup.h"

QString Rhd2000ModuleInfo::id() const
{
    return QStringLiteral("intan_rhd2000");
}

QString Rhd2000ModuleInfo::name() const
{
    return QStringLiteral("Intan RHD2000 USB Interface");
}

QString Rhd2000ModuleInfo::description() const
{
    return QStringLiteral("Record biopotential signals via the Intan Technologies RHD2000 Evaluation System "
                          "from up to 256 low-noise amplifier channels using digital electrophysiology chips.");
}

QString Rhd2000ModuleInfo::license() const
{
    return QStringLiteral("Intan Technologies RHD2000 Interface, (c) 2013-2017 <a href=\"http://intantech.com/\">Intan Technologies</a> [LGPLv3+]");
}

QPixmap Rhd2000ModuleInfo::pixmap() const
{
    return QPixmap(":/module/rhd2000");
}

bool Rhd2000ModuleInfo::singleton() const
{
    return true;
}

AbstractModule *Rhd2000ModuleInfo::createModule(QObject *parent)
{
    return new Rhd2000Module(parent);
}

Rhd2000Module::Rhd2000Module(QObject *parent)
    : AbstractModule(parent),
      m_intanUi(new IntanUi(this)),
      m_evTimer(new QTimer(this))
{
    // set up Intan GUI and board
    m_intanUi->setWindowIcon(QIcon(":/icons/generic-config"));
    m_intanUi->displayWidget()->setWindowIcon(QIcon(":/icons/generic-view"));

    addSettingsWindow(m_intanUi);
    addDisplayWindow(m_intanUi->displayWidget(), false);

    m_runAction = new QAction(this);
    m_runAction->setText("&Run without recording");
    m_runAction->setCheckable(true);
    connect(m_runAction, &QAction::triggered, this, &Rhd2000Module::noRecordRunActionTriggered);
    m_actions.append(m_runAction);

    m_actions.append(m_intanUi->renameChannelAction);
    m_actions.append(m_intanUi->toggleChannelEnableAction);
    m_actions.append(m_intanUi->enableAllChannelsAction);
    m_actions.append(m_intanUi->disableAllChannelsAction);
    m_actions.append(m_intanUi->originalOrderAction);
    m_actions.append(m_intanUi->alphaOrderAction);

    connect(m_intanUi, &IntanUi::portsScanned, this, &Rhd2000Module::on_portsScanned);
    on_portsScanned(m_intanUi->getSignalSources());

    // set up DAQ timer - unfortunately, we need to acquire all data in the UI thread
    m_evTimer->setInterval(0);
    connect(m_evTimer, &QTimer::timeout, this, &Rhd2000Module::runBoardDAQ);
}

bool Rhd2000Module::prepare(const TestSubject &testSubject)
{
    if (m_intanUi->isRunning()) {
        raiseError(QStringLiteral("Can not launch experiment because Intan module is already running, likely in no-record mode.\nPlease stop the module first to continue."));
        return false;
    }

    QString intanBasePart;
    if (testSubject.id.isEmpty())
        intanBasePart = QStringLiteral("intan/ephys");
    else
        intanBasePart = QStringLiteral("intan/%2_ephys").arg(testSubject.id);

    auto intanBaseFilename = getDataStoragePath(intanBasePart);
    if (intanBaseFilename.isEmpty())
        return false;
    m_intanUi->setBaseFileName(intanBaseFilename);

    m_intanUi->interfaceBoardPrepareRecording();
    m_intanUi->interfaceBoardInitRun(m_syTimer);
    m_runAction->setEnabled(false);

    // set sampling rate metadata and nullify every data block
    for (auto &pair : m_ampStreamBlocks) {
        pair.first->setMetadataValue(QStringLiteral("samplingRate"), m_intanUi->getSampleRate());

        pair.second->timestamps.fill(0);
        for (auto &v : pair.second->data)
            std::fill(v.begin(), v.end(), 0);
    }
    for (auto &pair : boardADCStreamBlocks) {
        pair.first->setMetadataValue(QStringLiteral("samplingRate"), m_intanUi->getSampleRate());

        pair.second->timestamps.fill(0);
        for (auto &v : pair.second->data)
            std::fill(v.begin(), v.end(), 0);
    }
    for (auto &pair : boardDINStreamBlocks) {
        pair.first->setMetadataValue(QStringLiteral("samplingRate"), m_intanUi->getSampleRate());

        pair.second->timestamps.fill(0);
        for (auto &v : pair.second->data)
            std::fill(v.begin(), v.end(), 0);
    }

    for (auto &port : outPorts())
        port->startStream();

    // set up slave-clock synchronizer
    clockSync = initCounterSynchronizer(m_intanUi->getSampleRate());
    clockSync->setStrategies(TimeSyncStrategy::WRITE_TSYNCFILE);
    clockSync->setTimeSyncBasename(intanBaseFilename);

    // permit 2ms tolerance - this was a very realistic tolerance to achieve in tests,
    // while lower values resulted in constant adjustment attempts
    clockSync->setTolerance(std::chrono::microseconds(2000));

    // check accuracy every two seconds
    clockSync->setCheckInterval(std::chrono::seconds(2));

    // we only permit calibration with the very first data block - this seems to be sufficient and
    // yielded the best results (due to device and USB buffering, the later data blocks are more
    // susceptible to error)
    clockSync->setMinimumBaseTSCalibrationPoints(Rhd2000DataBlock::getSamplesPerDataBlock());

    if (!clockSync->start()) {
        raiseError(QStringLiteral("Unable to set up timestamp synchronizer!"));
        return false;
    }

    return true;
}

void Rhd2000Module::start()
{
    m_intanUi->interfaceBoardStartRun();
    m_evTimer->start();
}

void Rhd2000Module::runBoardDAQ()
{
    if (!m_intanUi->interfaceBoardRunCycle()) {
        raiseError(QStringLiteral("Intan data acquisition failed."));
        m_evTimer->stop();
        return;
    }

    auto fifoPercentageFull = m_intanUi->currentFifoPercentageFull();
    if (fifoPercentageFull > 75.0) {
        setStatusMessage(QStringLiteral("<html>Buffer: <font color=\"red\"><b>") + QString::number(fifoPercentageFull, 'f', 0) + QStringLiteral("%</b> full</font>"));
    } else {
        setStatusMessage(QStringLiteral("Buffer: ") + QString::number(fifoPercentageFull, 'f', 0) + QStringLiteral("% full"));
    }
}

void Rhd2000Module::stop()
{
    m_intanUi->interfaceBoardStopFinalize();
    m_runAction->setEnabled(true);
    m_evTimer->stop();
    clockSync->stop();
}

QList<QAction *> Rhd2000Module::actions()
{
    return m_actions;
}

QByteArray Rhd2000Module::serializeSettings(const QString &)
{
    QByteArray intanSettings;
    QDataStream intanSettingsStream(&intanSettings, QIODevice::WriteOnly);
    m_intanUi->exportSettings(intanSettingsStream);

    return intanSettings;
}

bool Rhd2000Module::loadSettings(const QString &, const QByteArray &data)
{
    m_intanUi->loadSettings(data);
    return true;
}

void Rhd2000Module::emitStatusInfo(const QString &text)
{
    setStatusMessage(text);
}

void Rhd2000Module::pushSignalData()
{
    // amplifier signals
    for (auto &pair : m_ampStreamBlocks)
        pair.first->push(*pair.second.get());

    // board ADC inputs
    for (auto &pair : boardADCStreamBlocks)
        pair.first->push(*pair.second.get());

    // board digital inputs
    for (auto &pair : boardDINStreamBlocks)
        pair.first->push(*pair.second.get());
}

void Rhd2000Module::on_portsScanned(SignalSources *sources)
{
    // reset all our ports, we are adding new ones
    clearOutPorts();
    clearInPorts();

    // map all exported amplifier channels by their stream ID and chip-channel,
    // so we can quickly access them when fetching data from the board
    // there are at maximum 32 chip channels per stream
    m_ampStreamBlocks.clear();
    ampSdiByStreamCC.clear();
    ampSdiByStreamCC.resize(sources->signalPort.size());
    for (int i = 0; i < sources->signalPort.size(); i++) {
        ampSdiByStreamCC[i].resize(32); // max. 32 chip channels
        for (size_t j = 0; j < ampSdiByStreamCC[i].size(); j++)
            ampSdiByStreamCC[i][j] = StreamDataInfo<FloatSignalBlock>(false);
    }

    // clear blocks reserved for board inputs/outputs
    boardADCStreamBlocks.clear();
    boardDINStreamBlocks.clear();

    // We hardcode the 6 ports (and the amount of ports) that the Intan Evalualtion board has here.
    // Not very elegant, but the original software does that too
    for (int portId = 0; portId < sources->signalPort.size(); portId++) {
        const auto sg = sources->signalPort[portId];

        // check whether we have to create a Syntalos input port for this
        bool isInput = sg.prefix == QStringLiteral("DOUT");

        // check whether this channel is digital
        bool isDigital = (sg.prefix == QStringLiteral("DIN")) || (sg.prefix == QStringLiteral("DOUT"));

        // check whether this channel is getting signal from a headstage amplifier
        bool isAmplifier = sg.name.startsWith(QStringLiteral("Port"));

        if (isInput) {
            // we ignore the board output channel, as it would have to be reflected as
            // Syntalos input port here, not as an output port.
            // This has to be implemented later.
            continue;
        }

        // for amplifier-board channels, we only consider the amplifier signal channels,
        // and ignore the aux channels on the chip (which are used for accelerometer data etc.) for now
        int chanCount;
        if (isAmplifier)
            chanCount = sg.numAmplifierChannels();
        else
            chanCount = sg.numChannels();

        auto numBlocks = std::ceil(chanCount / 16.0);

        // Syntalos arranges high-speed data streams to output in blocks of 16 streams per block
        for (int b = 0; b < numBlocks; b++) {
            const auto syPortId = QStringLiteral("port-%1.%2_%3").arg(portId).arg(b).arg(sg.prefix);
            const auto firstChanId = b * 16;
            const auto lastChanId = (b == (numBlocks - 1))? chanCount - 1 : ((b + 1) * 16) - 1;

            const auto portName = QStringLiteral("%1 [%2..%3]").arg(sg.name).arg(firstChanId).arg(lastChanId);

            if (isDigital) {
                // we have the digital board input here (as the board output isn't currently supported)
                auto intStream = registerOutputPort<IntSignalBlock>(syPortId, portName);
                intStream->setMetadataValue(QStringLiteral("firstChannelNo"), firstChanId);
                intStream->setMetadataValue(QStringLiteral("lastChannelNo"), lastChanId);

                std::shared_ptr<IntSignalBlock> intSigBlock(new IntSignalBlock(SAMPLES_PER_DATA_BLOCK));
                boardDINStreamBlocks.push_back(std::make_pair(intStream, intSigBlock));
            } else {
                auto fpStream = registerOutputPort<FloatSignalBlock>(syPortId, portName);
                fpStream->setMetadataValue(QStringLiteral("firstChannelNo"), firstChanId);
                fpStream->setMetadataValue(QStringLiteral("lastChannelNo"), lastChanId);

                std::shared_ptr<FloatSignalBlock> signalBlock(new FloatSignalBlock(SAMPLES_PER_DATA_BLOCK));

                if (isAmplifier) {
                    // we have an amplifier stream from a headstage
                    m_ampStreamBlocks.push_back(std::make_pair(fpStream, signalBlock));

                    // we have an amplifier Syntalos output stream,
                    // mark all possibly exported channels
                    int sbChan = 0;
                    for (int chan = firstChanId; chan <= lastChanId; chan++) {
                        if (sbChan >= 16)
                            sbChan = 0;
                        const auto boardStreamId = sources->signalPort[portId].channelByIndex(chan)->boardStream;
                        const auto chipChan = sources->signalPort[portId].channelByIndex(chan)->chipChannel;

                        ampSdiByStreamCC[boardStreamId][chipChan].active = sg.enabled;
                        ampSdiByStreamCC[boardStreamId][chipChan].stream = fpStream;
                        ampSdiByStreamCC[boardStreamId][chipChan].signalBlock = signalBlock;
                        ampSdiByStreamCC[boardStreamId][chipChan].chan = chan;
                        ampSdiByStreamCC[boardStreamId][chipChan].sbChan = sbChan;

                        sbChan++;
                    }
                } else {
                    // we have a non-digital auxiliary stream (usually from the board itself)
                    // this one doesn't need mapping, yay!
                    boardADCStreamBlocks.push_back(std::make_pair(fpStream, signalBlock));
                }

            }

        } // end of blocks (ports) loop
    }
}

void Rhd2000Module::noRecordRunActionTriggered()
{
    if (!m_intanUi->isRunning())
        m_intanUi->runInterfaceBoard();
    else
        m_intanUi->stopInterfaceBoard();
}
