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
#include "waveplot.h"

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
    return QStringLiteral("Central hardware component of the RHD2000 Evaluation System, allowing users to record biopotential signals "
                          "from up to 256 low-noise amplifier channels using digital electrophysiology chips from Intan Technologies.");
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
    setName(QStringLiteral("Intan RHD2000 USB Interface"));
    m_intanUi = nullptr;

    // set up Intan GUI and board
    m_intanUi = new IntanUI(this);
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
    assert(m_intanUi);
    m_intanUi->interfaceBoardStopFinalize();
    m_runAction->setEnabled(true);
}

void Rhd2000Module::finalize()
{
    assert(m_intanUi);
    return;
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
    assert(m_intanUi);
    m_intanUi->loadSettings(data);
    return true;
}

void Rhd2000Module::setPlotProxy(TracePlotProxy *proxy)
{
    assert(m_intanUi);
    m_intanUi->getWavePlot()->setPlotProxy(proxy);
}

void Rhd2000Module::setStatusMessage(const QString &message)
{
    AbstractModule::setStatusMessage(message);
}

void Rhd2000Module::noRecordRunActionTriggered()
{
    if (!m_intanUi->isRunning())
        m_intanUi->runInterfaceBoard();
    else
        m_intanUi->stopInterfaceBoard();
}
