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

class ModuleInfo::Private
{
public:
    Private() { }
    ~Private() { }

    int count;
};

ModuleInfo::ModuleInfo(QObject *parent)
    : QObject(parent),
      d(new ModuleInfo::Private)
{
    d->count = 0;
}

ModuleInfo::~ModuleInfo()
{}

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
    return d->count;
}

void ModuleInfo::setCount(int count)
{
    d->count = count;
}

class StreamInputPort::Private
{
public:
    Private() { }
    ~Private() { }

    QString id;
    QString title;
};

StreamInputPort::StreamInputPort(const QString &id, const QString &title)
    : d(new StreamInputPort::Private)
{
    d->id = id;
    d->title = title;
}

StreamInputPort::~StreamInputPort()
{}

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
    return d->id;
}

QString StreamInputPort::title() const
{
    return d->title;
}

bool StreamInputPort::isInput() const
{
    return true;
}

class StreamOutputPort::Private
{
public:
    Private() { }
    ~Private() { }

    QString id;
    QString title;
    std::shared_ptr<VariantDataStream> stream;
};

StreamOutputPort::StreamOutputPort(const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream)
    : d(new StreamOutputPort::Private)
{
    d->id = id;
    d->title = title;
    d->stream = stream;
}

StreamOutputPort::~StreamOutputPort()
{}

bool StreamOutputPort::canSubscribe(const QString &typeName)
{
    return typeName == d->stream->dataTypeName();
}

QString StreamOutputPort::dataTypeName() const
{
    return d->stream->dataTypeName();
}

std::shared_ptr<VariantDataStream> StreamOutputPort::streamVar()
{
    return d->stream;
}

std::shared_ptr<VariantStreamSubscription> StreamOutputPort::subscribe()
{
    return d->stream->subscribeVar();
}

void StreamOutputPort::stopStream()
{
    if (d->stream->active())
        d->stream->stop();
}

QString StreamOutputPort::id() const
{
    return d->id;
}

QString StreamOutputPort::title() const
{
    return d->title;
}

bool StreamOutputPort::isOutput() const
{
    return true;
}

class AbstractModule::Private
{
public:
    Private()
        : state(ModuleState::INITIALIZING),
          initialized(false)
    {}
    ~Private() {}

    std::atomic<ModuleState> state;
    QString lastError;
    QString id;
    QString name;

    QList<QPair<QWidget*, bool>> displayWindows;
    QList<QPair<QWidget*, bool>> settingsWindows;

    bool initialized;
};

AbstractModule::AbstractModule(QObject *parent) :
    QObject(parent),
    m_running(false),
    d(new AbstractModule::Private)
{
    d->id = QStringLiteral("unknown");
    d->name = QStringLiteral("Unknown Module");
}

AbstractModule::~AbstractModule()
{
    // delete windows if we own them
    for (auto wp : d->displayWindows) {
        if (wp.second)
            delete wp.first;
    }
    for (auto wp : d->settingsWindows) {
        if (wp.second)
            delete wp.first;
    }
}

ModuleState AbstractModule::state() const
{
    return d->state;
}

void AbstractModule::setStateIdle()
{
    if ((d->state == ModuleState::RUNNING) ||
        (d->state == ModuleState::INITIALIZING))
        setState(ModuleState::IDLE);
}

void AbstractModule::setStateReady()
{
    if (d->state == ModuleState::PREPARING)
        setState(ModuleState::READY);
}

QString AbstractModule::id() const
{
    return d->id;
}

QString AbstractModule::name() const
{
    return d->name;
}

void AbstractModule::setName(const QString &name)
{
    d->name = name;
    emit nameChanged(d->name);
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
    for (auto const wp : d->displayWindows) {
        wp.first->show();
        wp.first->raise();
    }
}

bool AbstractModule::isDisplayUiVisible()
{
    for (auto const wp : d->displayWindows) {
        if (wp.first->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::showSettingsUi()
{
    for (auto const wp : d->settingsWindows) {
        wp.first->show();
        wp.first->raise();
    }
}

bool AbstractModule::isSettingsUiVisible()
{
    for (auto const wp : d->settingsWindows) {
        if (wp.first->isVisible())
            return true;
    }
    return false;
}

void AbstractModule::hideDisplayUi()
{
    for (auto const wp : d->displayWindows)
        wp.first->hide();
}

void AbstractModule::hideSettingsUi()
{
    for (auto const wp : d->settingsWindows)
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
    return d->lastError;
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

    d->displayWindows.append(qMakePair(window, owned));
}

void AbstractModule::addSettingsWindow(QWidget *window, bool owned)
{
    d->settingsWindows.append(qMakePair(window, owned));
}

void AbstractModule::setInitialized()
{
    if (d->initialized)
        return;
    d->initialized = true;
    setState(ModuleState::IDLE);
}

bool AbstractModule::initialized() const
{
    return d->initialized;
}

QJsonValue AbstractModule::serializeDisplayUiGeometry()
{
    QJsonObject obj;
    for (int i = 0; i < d->displayWindows.size(); i++) {
        const auto wp = d->displayWindows.at(i);

        QJsonObject info;
        info.insert("visible", wp.first->isVisible());
        info.insert("geometry", QString::fromUtf8(wp.first->saveGeometry().toBase64()));
        obj.insert(QString::number(i), info);
    }

    return obj;
}

void AbstractModule::restoreDisplayUiGeometry(QJsonObject info)
{
    for (int i = 0; i < d->displayWindows.size(); i++) {
        const auto wp = d->displayWindows.at(i);

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
    d->state = state;
    emit stateChanged(state);
}

void AbstractModule::raiseError(const QString &message)
{
    d->lastError = message;
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
    d->id = id;
}

void AbstractModule::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}

void AbstractModule::setTimer(std::shared_ptr<HRTimer> timer)
{
    m_timer = timer;
}
