/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include <opencv2/core.hpp>
#include "moduleapi.h"

#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>

#include "utils.h"

using namespace Syntalos;

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

QColor ModuleInfo::color() const
{
    auto img = pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
    if (img.isNull())
        return Qt::lightGray;

    int redBucket = 0;
    int greenBucket = 0;
    int blueBucket = 0;
    int totalColorCount = 0;

    auto bits = img.constBits();

    for (int y = 0, h = img.height(); y < h; y++) {
        for (int x = 0, w = img.width(); x < w; x++) {
            QRgb color = ((uint *)bits)[x + y * w];
            if (qAlpha(color) < 100)
                continue;

            const auto red = qRed(color);
            const auto green = qGreen(color);
            const auto blue = qBlue(color);

            // try to ignore colors too close to white or black
            if ((abs(red - green) < 38) && (abs(green - blue) < 38))
                continue;

            redBucket += red;
            greenBucket += green;
            blueBucket += blue;
            totalColorCount++;
        }
    }

    return QColor::fromRgb(redBucket / totalColorCount,
                           greenBucket / totalColorCount,
                           blueBucket / totalColorCount);
}

QString ModuleInfo::storageGroupName() const
{
    return QString();
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

class VarStreamInputPort::Private
{
public:
    Private() { }
    ~Private() { }

    QString id;
    QString title;
    AbstractModule *owner;
    StreamOutputPort *outPort;
};

VarStreamInputPort::VarStreamInputPort(AbstractModule *owner, const QString &id, const QString &title)
    : d(new VarStreamInputPort::Private)
{
    d->id = id;
    d->title = title;
    d->owner = owner;
    d->outPort = nullptr;
}

VarStreamInputPort::~VarStreamInputPort()
{}

bool VarStreamInputPort::hasSubscription() const
{
    return m_sub.has_value();
}

void VarStreamInputPort::setSubscription(StreamOutputPort *src, std::shared_ptr<VariantStreamSubscription> sub)
{
    d->outPort = src;
    m_sub = sub;

    d->owner->inputPortConnected(this);

    // signal interested parties as the input module that new
    // ports were connected
    emit d->owner->portsConnected(this, src);
}

void VarStreamInputPort::resetSubscription()
{
    if (m_sub.has_value())
        m_sub.value()->unsubscribe();
    m_sub.reset();
    d->outPort = nullptr;
}

StreamOutputPort *VarStreamInputPort::outPort() const
{
    if (hasSubscription())
        return d->outPort;
    return nullptr;
}

std::shared_ptr<VariantStreamSubscription> VarStreamInputPort::subscriptionVar()
{
    auto sub = m_sub.value();
    if (sub == nullptr) {
            qCritical().noquote() << "Tried to obtain variant subscription from a port that was not subscribed to anything.";
    }
    return sub;
}

QString VarStreamInputPort::id() const
{
    return d->id;
}

QString VarStreamInputPort::title() const
{
    return d->title;
}

PortDirection VarStreamInputPort::direction() const
{
    return PortDirection::INPUT;
}

AbstractModule *VarStreamInputPort::owner() const
{
    return d->owner;
}

class StreamOutputPort::Private
{
public:
    Private() { }
    ~Private() { }

    QString id;
    QString title;
    std::shared_ptr<VariantDataStream> stream;
    AbstractModule *owner;
};

StreamOutputPort::StreamOutputPort(AbstractModule *owner, const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream)
    : d(new StreamOutputPort::Private)
{
    d->id = id;
    d->title = title;
    d->owner = owner;
    d->stream = stream;
}

StreamOutputPort::~StreamOutputPort()
{}

bool StreamOutputPort::canSubscribe(const QString &typeName)
{
    return typeName == d->stream->dataTypeName();
}

int StreamOutputPort::dataTypeId() const
{
    return d->stream->dataTypeId();
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


void StreamOutputPort::startStream()
{
    d->stream->start();
}

QString StreamOutputPort::id() const
{
    return d->id;
}

QString StreamOutputPort::title() const
{
    return d->title;
}

PortDirection StreamOutputPort::direction() const
{
    return PortDirection::OUTPUT;
}

AbstractModule *StreamOutputPort::owner() const
{
    return d->owner;
}

class AbstractModule::Private
{
public:
    Private()
        : state(ModuleState::INITIALIZING),
          modIndex(0),
          initialized(false)
    {}
    ~Private() {}

    std::atomic<ModuleState> state;
    QString id;
    QString name;
    QString lastError;
    int modIndex;

    QList<QPair<QWidget*, bool>> displayWindows;
    QList<QPair<QWidget*, bool>> settingsWindows;

    std::shared_ptr<EDLGroup> rootDataGroup;
    std::shared_ptr<EDLDataset> defaultDataset;

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

int AbstractModule::index() const
{
    return d->modIndex;
}

QString AbstractModule::name() const
{
    return d->name;
}

void AbstractModule::setName(const QString &name)
{
    d->name = name;
    for (auto &oport : outPorts()) {
        oport->streamVar()->setCommonMetadata(d->id,
                                              d->name,
                                              oport->title());
    }
    emit nameChanged(d->name);
}

ModuleFeatures AbstractModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY |
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

void AbstractModule::serializeSettings(const QString &, QVariantHash &, QByteArray &)
{
    /* do nothing */
}

bool AbstractModule::loadSettings(const QString &, const QVariantHash &, const QByteArray &)
{
    return true;
}

void AbstractModule::inputPortConnected(VarStreamInputPort *)
{
    /* do nothing */
}

QString AbstractModule::lastError() const
{
    return d->lastError;
}

void AbstractModule::clearInPorts()
{
    m_inPorts.clear();
    emit portConfigurationUpdated();
}

void AbstractModule::clearOutPorts()
{
    m_outPorts.clear();
    emit portConfigurationUpdated();
}

void AbstractModule::removeInPortById(const QString &id)
{
    m_inPorts.remove(id);
    emit portConfigurationUpdated();
}

void AbstractModule::removeOutPortById(const QString &id)
{
    m_outPorts.remove(id);
    emit portConfigurationUpdated();
}

QList<std::shared_ptr<VarStreamInputPort> > AbstractModule::inPorts() const
{
    return m_inPorts.values();
}

QList<std::shared_ptr<StreamOutputPort> > AbstractModule::outPorts() const
{
    return m_outPorts.values();
}

std::shared_ptr<VarStreamInputPort> AbstractModule::inPortById(const QString &id) const
{
    return m_inPorts.value(id);
}

std::shared_ptr<StreamOutputPort> AbstractModule::outPortById(const QString &id) const
{
    return m_outPorts.value(id);
}

QList<QPair<intervalEventFunc_t, int> > AbstractModule::intervalEventCallbacks() const
{
    return m_intervalEventCBList;
}

bool AbstractModule::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        raiseError(QStringLiteral("Unable to create directory '%1'.").arg(dir));
        return false;
    }

    return true;
}

QString AbstractModule::datasetNameSuggestion(bool lowercase) const
{
    auto rawName = name();
    if (lowercase)
        rawName = rawName.toLower();
    const auto datasetName = rawName.simplified()
                                    .replace("/", "-")
                                    .replace("\\", "-")
                                    .replace(" ", "_")
                                    .replace(":", "");

    // we should never get here, the dataset name should never consist only
    // of unsuitable characters - but just in ase it does, we safeguard against that
    if (datasetName.isEmpty())
        return createRandomString(8);
    return datasetName;
}

static QStringList qStringSplitLimit(const QString &str, const QChar &sep, int maxSplit, Qt::CaseSensitivity cs = Qt::CaseSensitive)
{
    QStringList list;
    int start = 0;
    int end;
    while ((end = str.indexOf(sep, start, cs)) != -1) {
        if (start != end)
            list.append(str.mid(start, end - start));
        start = end + 1;
        if (maxSplit > 0) {
            if (list.length() > maxSplit)
                break;
        }
    }
    if (start != str.size())
        list.append(str.mid(start));
    return list;
}

QString AbstractModule::datasetNameFromSubMetadata(const QVariantHash &subMetadata)
{
    const auto srcModNameKey = _commonMetadataKeyMap->value(CommonMetadataKey::SrcModName);
    const auto dataNameProposalKey = _commonMetadataKeyMap->value(CommonMetadataKey::DataNameProposal);

    auto dataName = subMetadata.value(dataNameProposalKey).toString();
    if (dataName.isEmpty())
        dataName = subMetadata.value(srcModNameKey, datasetNameSuggestion()).toString();
    else {
        if (dataName.contains('/')) {
            const auto parts = qStringSplitLimit(dataName, '/', 1);
            dataName = parts[0];
        } else {
            dataName = subMetadata.value(srcModNameKey).toString();
        }
    }

    if (dataName.isEmpty())
        dataName = datasetNameSuggestion();

    return dataName;
}

QString AbstractModule::dataBasenameFromSubMetadata(const QVariantHash &subMetadata, const QString &defaultName)
{
    auto dataName = subMetadata.value(_commonMetadataKeyMap->value(CommonMetadataKey::DataNameProposal)).toString();
    if (dataName.contains('/')) {
        const auto parts = qStringSplitLimit(dataName, '/', 1);
        dataName = parts[1];
    } else {
        dataName = defaultName;
    }

    if (dataName.isEmpty())
        dataName = defaultName;

    return dataName;
}

std::shared_ptr<EDLDataset> AbstractModule::getOrCreateDefaultDataset(const QString &preferredName, const QVariantHash &subMetadata)
{
    if (d->defaultDataset.get() != nullptr)
        return d->defaultDataset;
    if (d->rootDataGroup.get() == nullptr) {
        qCritical().noquote() << "Module" << name() << "tried to obtain its default dataset, but no root storage group has been set yet.";
        return nullptr;
    }

    d->defaultDataset = getOrCreateDatasetInGroup(d->rootDataGroup, preferredName, subMetadata);
    return d->defaultDataset;
}

std::shared_ptr<EDLDataset> AbstractModule::getOrCreateDatasetInGroup(std::shared_ptr<EDLGroup> group, const QString &preferredName, const QVariantHash &subMetadata)
{
    auto dataName = preferredName;

    // if we have subscription metadata, try to use that data to determine the
    // data set name
    if (!subMetadata.isEmpty()) {
        dataName = datasetNameFromSubMetadata(subMetadata);
    }

    // just set our module name if we still have no data set name
    if (dataName.isEmpty())
        dataName = datasetNameSuggestion();

    return group->datasetByName(dataName, true);
}

std::shared_ptr<EDLGroup> AbstractModule::createStorageGroup(const QString &groupName)
{
    if (d->rootDataGroup.get() == nullptr) {
        qCritical().noquote() << "Module" << name() << "tried to create a new storage group, but no root storage group has been set yet.";
        return nullptr;
    }

    return d->rootDataGroup->groupByName(groupName, true);
}

void AbstractModule::addDisplayWindow(QWidget *window, bool owned)
{

    d->displayWindows.append(qMakePair(window, owned));
}

void AbstractModule::addSettingsWindow(QWidget *window, bool owned)
{
    d->settingsWindows.append(qMakePair(window, owned));
}

std::unique_ptr<FreqCounterSynchronizer> AbstractModule::initCounterSynchronizer(double frequencyHz)
{
    if ((d->state != ModuleState::PREPARING) && (d->state != ModuleState::READY) && (d->state != ModuleState::RUNNING))
        return nullptr;
    assert(frequencyHz > 0);

    std::unique_ptr<FreqCounterSynchronizer> synchronizer(new FreqCounterSynchronizer(m_syTimer, this, frequencyHz));
    return synchronizer;
}

std::unique_ptr<SecondaryClockSynchronizer> AbstractModule::initClockSynchronizer(double frequencyHz)
{
    if ((d->state != ModuleState::PREPARING) && (d->state != ModuleState::READY) && (d->state != ModuleState::RUNNING))
        return nullptr;
    assert(frequencyHz > 0);

    std::unique_ptr<SecondaryClockSynchronizer> synchronizer(new SecondaryClockSynchronizer(m_syTimer, this, frequencyHz));
    return synchronizer;
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

QVariant AbstractModule::serializeDisplayUiGeometry()
{
    QVariantHash obj;
    for (int i = 0; i < d->displayWindows.size(); i++) {
        const auto wp = d->displayWindows.at(i);

        QVariantHash info;
        info.insert("visible", wp.first->isVisible());
        info.insert("geometry", QString::fromUtf8(wp.first->saveGeometry().toBase64()));
        obj.insert(QString::number(i), info);
    }

    return obj;
}

void AbstractModule::restoreDisplayUiGeometry(const QVariant &var)
{
    const auto info = var.toHash();
    if (info.isEmpty())
        return;
    for (int i = 0; i < d->displayWindows.size(); i++) {
        const auto wp = d->displayWindows.at(i);

        auto winfo = info.value(QString::number(i)).toHash();
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

void AbstractModule::setId(const QString &id)
{
    d->id = id;
}

void AbstractModule::setIndex(int index)
{
    d->modIndex = index;
}

void AbstractModule::setStorageGroup(std::shared_ptr<EDLGroup> edlGroup)
{
    d->defaultDataset.reset();
    d->rootDataGroup = edlGroup;
}

void AbstractModule::resetEventCallbacks()
{
    m_intervalEventCBList.clear();
}

void AbstractModule::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}

void AbstractModule::setTimer(std::shared_ptr<SyncTimer> timer)
{
    m_syTimer = timer;
}
