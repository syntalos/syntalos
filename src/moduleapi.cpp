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

#include <opencv2/core.hpp>
#include "moduleapi.h"

#include <QDir>
#include <QMessageBox>
#include <QJsonDocument>
#include <QDebug>

QString ModuleInfo::id() const
{
    return QStringLiteral("unknown");
}

QString ModuleInfo::name() const
{
    return QStringLiteral("Unknown Module");
}

QString ModuleInfo::description() const
{
    return QStringLiteral("An unknown description.");
}

QString ModuleInfo::license() const
{
    return QString();
}

QPixmap ModuleInfo::pixmap() const
{
    return QPixmap(":/module/generic");
}

bool ModuleInfo::singleton() const
{
    return false;
}

int ModuleInfo::count() const
{
    return m_count;
}

void ModuleInfo::setCount(int count)
{
    m_count = count;
}

StreamInputPort::StreamInputPort(const QString &title)
    : m_title(title)
{
}

QString StreamInputPort::acceptedTypeName() const
{
    return m_acceptedTypeName;
}

bool StreamInputPort::acceptsSubscription(const QString &typeName)
{
    return m_acceptedTypeName == typeName;
}

bool StreamInputPort::hasSubscription() const
{
    return m_sub.has_value();
}

void StreamInputPort::setSubscription(std::shared_ptr<VariantStreamSubscription> sub)
{
    m_sub = sub;
}

void StreamInputPort::resetSubscription()
{
    m_sub.reset();
}

StreamOutputPort::StreamOutputPort(const QString &title, std::shared_ptr<VariantDataStream> stream)
    : m_title(title),
      m_stream(stream)
{
}

bool StreamOutputPort::canSubscribe(const QString &typeName)
{
    return typeName == m_stream->dataTypeName();
}

QString StreamOutputPort::dataTypeName() const
{
    return m_stream->dataTypeName();
}

AbstractModule::AbstractModule(QObject *parent) :
    QObject(parent),
    m_state(ModuleState::INITIALIZING),
    m_msgStream(new DataStream<ModuleMessage>()),
    m_initialized(false)
{
    m_id = QStringLiteral("unknown");
    m_name = QStringLiteral("Unknown Module");
}

ModuleState AbstractModule::state() const
{
    return m_state;
}

QString AbstractModule::id() const
{
    return m_id;
}

QString AbstractModule::name() const
{
    return m_name;
}

void AbstractModule::setName(const QString &name)
{
    m_name = name;
    emit nameChanged(m_name);
}

ModuleFeatures AbstractModule::features() const
{
    return ModuleFeature::DISPLAY |
           ModuleFeature::SETTINGS |
           ModuleFeature::ACTIONS;
}

void AbstractModule::start()
{
    setState(ModuleState::RUNNING);
}

bool AbstractModule::runCycle()
{
    return true;
}

void AbstractModule::finalize()
{
    // Do nothing.
}

void AbstractModule::showDisplayUi()
{
    Q_FOREACH(auto w, m_displayWindows) {
        w->show();
        w->raise();
    }
}

bool AbstractModule::isDisplayUiVisible()
{
    Q_FOREACH(auto w, m_displayWindows) {
        if (w->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::showSettingsUi()
{
    Q_FOREACH(auto w, m_settingsWindows) {
        w->show();
        w->raise();
    }
}

bool AbstractModule::isSettingsUiVisible()
{
    Q_FOREACH(auto w, m_settingsWindows) {
        if (w->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::hideDisplayUi()
{
    Q_FOREACH(auto w, m_displayWindows)
        w->hide();
}

void AbstractModule::hideSettingsUi()
{
    Q_FOREACH(auto w, m_settingsWindows)
        w->hide();
}

QList<QAction *> AbstractModule::actions()
{
    QList<QAction*> res;
    return res;
}

QByteArray AbstractModule::serializeSettings(const QString &confBaseDir)
{
    Q_UNUSED(confBaseDir)
    QByteArray zero;
    return zero;
}

bool AbstractModule::loadSettings(const QString &confBaseDir, const QByteArray &data)
{
    Q_UNUSED(confBaseDir)
    Q_UNUSED(data)
    return true;
}

QString AbstractModule::lastError() const
{
    return m_lastError;
}

bool AbstractModule::canRemove(AbstractModule *mod)
{
    Q_UNUSED(mod)
    return true;
}

std::shared_ptr<StreamSubscription<ModuleMessage>> AbstractModule::getMessageSubscription()
{
    return m_msgStream->subscribe(nullptr);
}

void AbstractModule::subscribeToSysEvents(std::shared_ptr<StreamSubscription<SystemStatusEvent> > subscription)
{
    m_sysEventsSub = subscription;
}

QList<std::shared_ptr<StreamInputPort> > AbstractModule::inPorts() const
{
    return m_inPorts;
}

QList<std::shared_ptr<StreamOutputPort> > AbstractModule::outPorts() const
{
    return m_outPorts;
}

bool AbstractModule::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        raiseError(QStringLiteral("Unable to create directory '%1'.").arg(dir));
        return false;
    }

    return true;
}

void AbstractModule::setInitialized()
{
    m_initialized = true;
}

bool AbstractModule::initialized() const
{
    return m_initialized;
}

QJsonValue AbstractModule::serializeDisplayUiGeometry()
{
    QJsonObject obj;
    for (int i = 0; i < m_displayWindows.size(); i++) {
        auto w = m_displayWindows.at(i);

        QJsonObject info;
        info.insert("visible", w->isVisible());
        info.insert("geometry", QString::fromUtf8(w->saveGeometry().toBase64()));
        obj.insert(QString::number(i), info);
    }

    return obj;
}

void AbstractModule::restoreDisplayUiGeomatry(QJsonObject info)
{
    for (int i = 0; i < m_displayWindows.size(); i++) {
        auto w = m_displayWindows.at(i);

        auto winfo = info.value(QString::number(i)).toObject();
        if (winfo.isEmpty())
            continue;
        if (winfo.value("visible").toBool())
            w->show();

        auto b64Geometry = winfo.value("geometry").toString();
        w->restoreGeometry(QByteArray::fromBase64(b64Geometry.toUtf8()));
    }
}

void AbstractModule::setState(ModuleState state)
{
    m_state = state;
    emit stateChanged(state);
}

void AbstractModule::raiseError(const QString &message)
{
    m_lastError = message;
    emit error(message);
    setState(ModuleState::ERROR);
    qCritical() << message;
}

QByteArray AbstractModule::jsonObjectToBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson();
}

QJsonObject AbstractModule::jsonObjectFromBytes(const QByteArray &data)
{
    auto doc = QJsonDocument::fromJson(data);
    return doc.object();
}

void AbstractModule::setId(const QString &id)
{
    m_id = id;
}

void AbstractModule::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}
