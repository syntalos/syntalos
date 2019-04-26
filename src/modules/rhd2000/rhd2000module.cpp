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
    // set up Intan GUI and board
    m_intanUi = new IntanUI(this);

    m_actions.append(m_intanUi->renameChannelAction);
    m_actions.append(m_intanUi->toggleChannelEnableAction);
    m_actions.append(m_intanUi->enableAllChannelsAction);
    m_actions.append(m_intanUi->disableAllChannelsAction);
    m_actions.append(m_intanUi->originalOrderAction);
    m_actions.append(m_intanUi->alphaOrderAction);
}

Rhd2000Module::~Rhd2000Module()
{
    delete m_intanUi;
}

QString Rhd2000Module::name() const
{
    return QStringLiteral("intan_rhd2000");
}

QString Rhd2000Module::displayName() const
{
    return QStringLiteral("Intan RHD2000 USB Eval");
}

QPixmap Rhd2000Module::pixmap() const
{
    return QPixmap(":/module/rhd2000");
}

bool Rhd2000Module::initialize()
{
    setState(ModuleState::READY);
    return true;
}

bool Rhd2000Module::prepare(const QString &storageRootDir, const QString &subjectId)
{
    setState(ModuleState::PREPARING);

    QString intanBaseName;
    if (subjectId.isEmpty())
        intanBaseName = QString::fromUtf8("%1/intan/ephys").arg(storageRootDir);
    else
        intanBaseName = QString::fromUtf8("%1/intan/%2_ephys").arg(storageRootDir).arg(subjectId);

    m_intanUi->setBaseFileName(intanBaseName);

    return true;
}

bool Rhd2000Module::runCycle()
{
    return true;
}

void Rhd2000Module::stop()
{
    return;
}

void Rhd2000Module::finalize()
{
    return;
}

void Rhd2000Module::showDisplayUi()
{
    m_intanUi->displayWidget()->show();
}

void Rhd2000Module::hideDisplayUi()
{
    m_intanUi->displayWidget()->hide();
}

void Rhd2000Module::showSettingsUi()
{
    m_intanUi->show();
}

void Rhd2000Module::hideSettingsUi()
{
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
    m_intanUi->loadSettings(data);
    return true;
}

void Rhd2000Module::setPlotProxy(TracePlotProxy *proxy)
{
    m_intanUi->getWavePlot()->setPlotProxy(proxy);
}

void Rhd2000Module::setStatusMessage(const QString &message)
{
    AbstractModule::setStatusMessage(message);
}
