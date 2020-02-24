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
    return QStringLiteral("Allows to record biopotential signals via the Intan Technologies RHD2000 Evaluation System"
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
    : AbstractModule(parent)
{
    // set up Intan GUI and board
    m_intanUi = new IntanUi(this);
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
}

bool Rhd2000Module::prepare(const TestSubject &testSubject)
{
    assert(m_intanUi);

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
    m_intanUi->interfaceBoardInitRun();
    m_runAction->setEnabled(false);

    for (auto &port : outPorts())
        port->startStream();

    return true;
}

void Rhd2000Module::start()
{
    m_intanUi->interfaceBoardStartRun();
}

bool Rhd2000Module::runUIEvent()
{
    // we don't assert m_intanUi here for performance reasons
    auto ret = m_intanUi->interfaceBoardRunCycle();

    auto fifoPercentageFull = m_intanUi->currentFifoPercentageFull();
    if (fifoPercentageFull > 75.0) {
        setStatusMessage(QStringLiteral("<html>Buffer: <font color=\"red\"><b>") + QString::number(fifoPercentageFull, 'f', 0) + QStringLiteral("%</b> full</font>"));
    } else {
        setStatusMessage(QStringLiteral("Buffer: ") + QString::number(fifoPercentageFull, 'f', 0) + QStringLiteral("% full"));
    }

    return ret;
}

void Rhd2000Module::stop()
{
    m_intanUi->interfaceBoardStopFinalize();
    m_runAction->setEnabled(true);
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

void Rhd2000Module::pushAmplifierData()
{
    for (auto &pair : m_streamSigBlocks)
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
    m_streamSigBlocks.clear();
    fsdiByStreamCC.clear();
    fsdiByStreamCC.resize(sources->signalPort.size());
    for (int i = 0; i < sources->signalPort.size(); i++) {
        fsdiByStreamCC[i].resize(32);
        for (size_t j = 0; j < fsdiByStreamCC[i].size(); j++)
            fsdiByStreamCC[i][j] = FloatStreamDataInfo(false);
    }

    // We hardcode the 6 ports (and the amount of ports) that the Intan Evalualtion board has here.
    // Not very elegant, but the original software does that too
    for (int portId = 0; portId < sources->signalPort.size(); portId++) {
        const auto sg = sources->signalPort[portId];

        // ignore disabled channels
        if (!sg.enabled)
            continue;

        if (sg.prefix == QStringLiteral("DOUT")) {
            // we ignore the board output channel, as it would have to be reflected as
            // Syntalos input port here, not as an outpt port.
            // This has to be implemented later.
            continue;
        }

        bool isDigital = (sg.prefix == QStringLiteral("DIN")) || (sg.prefix == QStringLiteral("DOUT"));

        // for amplifier-board channels, we only consider the amplifier signal channels,
        // and ignore the aux channels for now
        int chanCount;
        if (sg.name.startsWith(QStringLiteral("Port")))
            chanCount = sg.numAmplifierChannels();
        else
            chanCount = sg.numChannels();

        auto numBlocks = std::ceil((chanCount - 10) / 16.0);

        // Syntalos arranges high-speed data streams to output in blocks of 16 streams per block
        for (int b = 0; b < numBlocks; b++) {
            const auto syPortId = QStringLiteral("port-%1.%2_%3").arg(portId).arg(b).arg(sg.prefix);
            const auto firstChanId = b * 16;
            const auto lastChanId = (b == (numBlocks - 1))? chanCount - 1 : ((b + 1) * 16) - 1;

            const auto portName = QStringLiteral("%1 [%2..%3]").arg(sg.name).arg(firstChanId).arg(lastChanId);

            if (isDigital)
                registerOutputPort<IntSignalBlock>(syPortId, portName);
            else {
                auto fpStream = registerOutputPort<FloatSignalBlock>(syPortId, portName);

                std::shared_ptr<FloatSignalBlock> signalBlock(new FloatSignalBlock(SAMPLES_PER_DATA_BLOCK));
                m_streamSigBlocks.push_back(std::make_pair(fpStream, signalBlock));

                // we have an amplifier Syntalos output stream,
                // mark all possibly exported channels
                int sbChan = 0;
                for (int chan = firstChanId; chan <= lastChanId; chan++) {
                    if (sbChan >= 16)
                        sbChan = 0;
                    const auto boardStreamId = sources->signalPort[portId].channelByIndex(chan)->boardStream;
                    const auto chipChan = sources->signalPort[portId].channelByIndex(chan)->chipChannel;

                    fsdiByStreamCC[boardStreamId][chipChan].active = true;
                    fsdiByStreamCC[boardStreamId][chipChan].stream = fpStream;
                    fsdiByStreamCC[boardStreamId][chipChan].signalBlock = signalBlock;
                    fsdiByStreamCC[boardStreamId][chipChan].chan = chan;
                    fsdiByStreamCC[boardStreamId][chipChan].sbChan = sbChan;

                    sbChan++;
                }
            }
        }
    }
}

void Rhd2000Module::noRecordRunActionTriggered()
{
    if (!m_intanUi->isRunning())
        m_intanUi->runInterfaceBoard();
    else
        m_intanUi->stopInterfaceBoard();
}
