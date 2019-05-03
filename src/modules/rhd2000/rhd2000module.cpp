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

Rhd2000Module::Rhd2000Module(QObject *parent)
    : AbstractModule(parent)
{
    m_intanUi = nullptr;
}

Rhd2000Module::~Rhd2000Module()
{
    if (m_intanUi != nullptr)
        delete m_intanUi;
}

QString Rhd2000Module::id() const
{
    return QStringLiteral("intan_rhd2000");
}

QString Rhd2000Module::displayName() const
{
    return QStringLiteral("Intan RHD2000 USB Eval");
}

QString Rhd2000Module::description() const
{
    return QStringLiteral("Central hardware component of the RHD2000 Evaluation System, allowing users to record biopotential signals "
                          "from up to 256 low-noise amplifier channels using digital electrophysiology chips from Intan Technologies.");
}

QPixmap Rhd2000Module::pixmap() const
{
    return QPixmap(":/module/rhd2000");
}

bool Rhd2000Module::singleton() const
{
    return true;
}

bool Rhd2000Module::initialize(ModuleManager *manager)
{
    Q_UNUSED(manager);
    assert(!initialized());

    setState(ModuleState::PREPARING);
    // set up Intan GUI and board
    m_intanUi = new IntanUI(this);
    m_intanUi->setWindowIcon(QIcon(":/icons/generic-config"));
    m_intanUi->displayWidget()->setWindowIcon(QIcon(":/icons/generic-view"));

    auto runAction = new QAction(this);
    runAction->setText("&Run without recording");
    connect(runAction, &QAction::triggered, this, &Rhd2000Module::noRecordRunActionTriggered);
    m_actions.append(runAction);

    m_actions.append(m_intanUi->renameChannelAction);
    m_actions.append(m_intanUi->toggleChannelEnableAction);
    m_actions.append(m_intanUi->enableAllChannelsAction);
    m_actions.append(m_intanUi->disableAllChannelsAction);
    m_actions.append(m_intanUi->originalOrderAction);
    m_actions.append(m_intanUi->alphaOrderAction);

    emit actionsUpdated();
    setState(ModuleState::READY);
    setInitialized();
    return true;
}

bool Rhd2000Module::prepare(const QString &storageRootDir, const QString &subjectId)
{
    assert(m_intanUi);

    setState(ModuleState::PREPARING);

    QString intanBaseName;
    if (subjectId.isEmpty())
        intanBaseName = QString::fromUtf8("%1/intan/ephys").arg(storageRootDir);
    else
        intanBaseName = QString::fromUtf8("%1/intan/%2_ephys").arg(storageRootDir).arg(subjectId);

    m_intanUi->setBaseFileName(intanBaseName);

    m_intanUi->interfaceBoardInitRun();
    setState(ModuleState::RUNNING);

    return true;
}

bool Rhd2000Module::runCycle()
{
    // we don't assert m_intanUi here for performance reasons
    return m_intanUi->interfaceBoardRunCycle();
}

void Rhd2000Module::stop()
{
    assert(m_intanUi);
    m_intanUi->interfaceBoardStopFinalize();
}

void Rhd2000Module::finalize()
{
    assert(m_intanUi);
    return;
}

void Rhd2000Module::showDisplayUi()
{
    assert(m_intanUi);
    m_intanUi->displayWidget()->show();
}

void Rhd2000Module::hideDisplayUi()
{
    assert(m_intanUi);
    m_intanUi->displayWidget()->hide();
}

void Rhd2000Module::showSettingsUi()
{
    assert(m_intanUi);
    m_intanUi->show();
}

void Rhd2000Module::hideSettingsUi()
{
    assert(m_intanUi);
    m_intanUi->hide();
}

QList<QAction *> Rhd2000Module::actions()
{
    return m_actions;
}

QByteArray Rhd2000Module::serializeSettings()
{
    QByteArray intanSettings;
    QDataStream intanSettingsStream(&intanSettings, QIODevice::WriteOnly);
    m_intanUi->exportSettings(intanSettingsStream);

    return intanSettings;
}

bool Rhd2000Module::loadSettings(const QByteArray &data)
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
    // FIXME
    //setRunPossible(false);
    //setStopPossible(true);

    // run Intan acquisition
    //m_statusWidget->setIntanStatus(StatusWidget::Active);
    m_intanUi->runInterfaceBoard();
}
