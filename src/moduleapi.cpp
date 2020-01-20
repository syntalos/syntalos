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

StreamInputPort::StreamInputPort(const QString &id, const QString &title)
    : m_id(id),
      m_title(title)
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
    if (m_sub.has_value())
        m_sub.value()->unsubscribe();
    m_sub.reset();
}

QString StreamInputPort::id() const
{
    return m_id;
}

QString StreamInputPort::title() const
{
    return m_title;
}

bool StreamInputPort::isInput() const
{
    return true;
}

StreamOutputPort::StreamOutputPort(const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream)
    : m_id(id),
      m_title(title),
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

std::shared_ptr<VariantStreamSubscription> StreamOutputPort::subscribe()
{
    return m_stream->subscribeVar();
}

void StreamOutputPort::stopStream()
{
    if (m_stream->active())
        m_stream->stop();
}

QString StreamOutputPort::id() const
{
    return m_id;
}

QString StreamOutputPort::title() const
{
    return m_title;
}

bool StreamOutputPort::isOutput() const
{
    return true;
}

AbstractModule::AbstractModule(QObject *parent) :
    QObject(parent),
    m_running(false),
    m_state(ModuleState::INITIALIZING),
    m_initialized(false)
{
    m_id = QStringLiteral("unknown");
    m_name = QStringLiteral("Unknown Module");
}

AbstractModule::~AbstractModule()
{
    // delete windows if we own them
    for (auto wp : m_displayWindows) {
        if (wp.second)
            delete wp.first;
    }
    for (auto wp : m_settingsWindows) {
        if (wp.second)
            delete wp.first;
    }
}

ModuleState AbstractModule::state() const
{
    return m_state;
}

void AbstractModule::setStateIdle()
{
    if ((m_state == ModuleState::RUNNING) ||
        (m_state == ModuleState::INITIALIZING))
        setState(ModuleState::IDLE);
}

void AbstractModule::setStateReady()
{
    if (m_state == ModuleState::PREPARING)
        setState(ModuleState::READY);
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
    return ModuleFeature::RUN_EVENTS |
           ModuleFeature::SHOW_DISPLAY |
           ModuleFeature::SHOW_SETTINGS |
           ModuleFeature::SHOW_ACTIONS;
}

bool AbstractModule::initialize()
{
    assert(!initialized());
    setInitialized();
    return true;
}

void AbstractModule::start()
{
    m_running = true;
    setState(ModuleState::RUNNING);
}

bool AbstractModule::runEvent()
{
    return true;
}

void AbstractModule::runThread(OptionalWaitCondition *)
{
    // Do nothing
}

void AbstractModule::stop()
{
    m_running = false;
}

void AbstractModule::finalize()
{
    // Do nothing.
}

void AbstractModule::showDisplayUi()
{
    for (auto const wp : m_displayWindows) {
        wp.first->show();
        wp.first->raise();
    }
}

bool AbstractModule::isDisplayUiVisible()
{
    for (auto const wp : m_displayWindows) {
        if (wp.first->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::showSettingsUi()
{
    for (auto const wp : m_settingsWindows) {
        wp.first->show();
        wp.first->raise();
    }
}

bool AbstractModule::isSettingsUiVisible()
{
    for (auto const wp : m_settingsWindows) {
        if (wp.first->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::hideDisplayUi()
{
    for (auto const wp : m_displayWindows)
        wp.first->hide();
}

void AbstractModule::hideSettingsUi()
{
    for (auto const wp : m_settingsWindows)
        wp.first->hide();
}

QList<QAction *> AbstractModule::actions()
{
    QList<QAction*> res;
    return res;
}

QByteArray AbstractModule::serializeSettings(const QString &)
{
    QByteArray zero;
    return zero;
}

bool AbstractModule::loadSettings(const QString &, const QByteArray &)
{
    return true;
}

QString AbstractModule::lastError() const
{
    return m_lastError;
}

QList<std::shared_ptr<StreamInputPort> > AbstractModule::inPorts() const
{
    return m_inPorts.values();
}

QList<std::shared_ptr<StreamOutputPort> > AbstractModule::outPorts() const
{
    return m_outPorts.values();
}

bool AbstractModule::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        raiseError(QStringLiteral("Unable to create directory '%1'.").arg(dir));
        return false;
    }

    return true;
}

void AbstractModule::addDisplayWindow(QWidget *window, bool owned)
{

    m_displayWindows.append(qMakePair(window, owned));
}

void AbstractModule::addSettingsWindow(QWidget *window, bool owned)
{
    m_settingsWindows.append(qMakePair(window, owned));
}

void AbstractModule::setInitialized()
{
    if (m_initialized)
        return;
    m_initialized = true;
    setState(ModuleState::IDLE);
}

bool AbstractModule::initialized() const
{
    return m_initialized;
}

QJsonValue AbstractModule::serializeDisplayUiGeometry()
{
    QJsonObject obj;
    for (int i = 0; i < m_displayWindows.size(); i++) {
        const auto wp = m_displayWindows.at(i);

        QJsonObject info;
        info.insert("visible", wp.first->isVisible());
        info.insert("geometry", QString::fromUtf8(wp.first->saveGeometry().toBase64()));
        obj.insert(QString::number(i), info);
    }

    return obj;
}

void AbstractModule::restoreDisplayUiGeometry(QJsonObject info)
{
    for (int i = 0; i < m_displayWindows.size(); i++) {
        const auto wp = m_displayWindows.at(i);

        auto winfo = info.value(QString::number(i)).toObject();
        if (winfo.isEmpty())
            continue;
        if (winfo.value("visible").toBool())
            wp.first->show();

        auto b64Geometry = winfo.value("geometry").toString();
        wp.first->restoreGeometry(QByteArray::fromBase64(b64Geometry.toUtf8()));
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

void AbstractModule::setTimer(std::shared_ptr<HRTimer> timer)
{
    m_timer = timer;
}
