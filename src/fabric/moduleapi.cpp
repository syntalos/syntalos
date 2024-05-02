/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleapi.h"

#include "config.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMainWindow>
#include <QMessageBox>
#include <QStandardPaths>
#include <QCursor>

#include "datactl/frametype.h"
#include "utils/misc.h"

using namespace Syntalos;

class ModuleInfo::Private
{
public:
    Private() {}
    ~Private() {}

    int count;
    QString rootDir;
    QIcon icon;
};

ModuleInfo::ModuleInfo()
    : d(new ModuleInfo::Private)
{
    d->count = 0;
}

ModuleInfo::~ModuleInfo() {}

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

QIcon ModuleInfo::icon() const
{
    if (d->icon.isNull())
        return QIcon(":/module/generic");
    return d->icon;
}

void ModuleInfo::setIcon(const QIcon &icon)
{
    d->icon = icon;
}

void ModuleInfo::refreshIcon()
{
    QIcon icon = QIcon(":/module/generic");
    if (!d->rootDir.isEmpty()) {
        const auto iconExts = QStringList() << ".svg"
                                            << ".svgz"
                                            << ".png";
        for (const auto &iconExt : iconExts) {
            const auto iconFname = QDir(d->rootDir).filePath(id() + iconExt);
            if (QFileInfo::exists(iconFname)) {
                icon = QIcon(iconFname);
                break;
            }
        }
    }

    d->icon = icon;
}

QColor ModuleInfo::color() const
{
    auto img = icon().pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
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

    // return a gray/black-ish color in case everything was black
    // or has no usable colors
    if (totalColorCount == 0)
        return QColor::fromRgb(77, 77, 77);

    return QColor::fromRgb(redBucket / totalColorCount, greenBucket / totalColorCount, blueBucket / totalColorCount);
}

ModuleCategories ModuleInfo::categories() const
{
    return ModuleCategory::NONE;
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

QString ModuleInfo::rootDir() const
{
    return d->rootDir;
}

void ModuleInfo::setCount(int count)
{
    d->count = count;
}

void ModuleInfo::setRootDir(const QString &dir)
{
    d->rootDir = dir;
    refreshIcon();
}

class VarStreamInputPort::Private
{
public:
    Private() {}
    ~Private() {}

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

VarStreamInputPort::~VarStreamInputPort() {}

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
        qCritical().noquote()
            << "Tried to obtain variant subscription from a port that was not subscribed to anything.";
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
    Private() {}
    ~Private() {}

    QString id;
    QString title;
    std::shared_ptr<VariantDataStream> stream;
    AbstractModule *owner;
};

StreamOutputPort::StreamOutputPort(
    AbstractModule *owner,
    const QString &id,
    const QString &title,
    std::shared_ptr<VariantDataStream> stream)
    : d(new StreamOutputPort::Private)
{
    d->id = id;
    d->title = title;
    d->owner = owner;
    d->stream = stream;
}

StreamOutputPort::~StreamOutputPort() {}

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

#define CHECK_RETURN_INPUT_PORT(T) \
    if (typeId == BaseDataType::T) \
        return new StreamInputPort<T>(mod, id, title);

#define CHECK_RETURN_STREAM(T)     \
    if (typeId == BaseDataType::T) \
        return new DataStream<T>();

VarStreamInputPort *Syntalos::newInputPortForType(
    int typeId,
    AbstractModule *mod,
    const QString &id,
    const QString &title = QString())
{
    CHECK_RETURN_INPUT_PORT(ControlCommand)
    CHECK_RETURN_INPUT_PORT(TableRow)
    CHECK_RETURN_INPUT_PORT(FirmataControl)
    CHECK_RETURN_INPUT_PORT(FirmataData)
    CHECK_RETURN_INPUT_PORT(Frame)
    CHECK_RETURN_INPUT_PORT(IntSignalBlock)
    CHECK_RETURN_INPUT_PORT(FloatSignalBlock)

    qCritical() << "Unable to create input port for unknown type ID" << typeId;
    return nullptr;
}

VariantDataStream *Syntalos::newStreamForType(int typeId)
{
    CHECK_RETURN_STREAM(ControlCommand)
    CHECK_RETURN_STREAM(TableRow)
    CHECK_RETURN_STREAM(FirmataControl)
    CHECK_RETURN_STREAM(FirmataData)
    CHECK_RETURN_STREAM(Frame)
    CHECK_RETURN_STREAM(IntSignalBlock)
    CHECK_RETURN_STREAM(FloatSignalBlock)

    qCritical() << "Unable to create data stream for unknown type ID" << typeId;
    return nullptr;
}

QString Syntalos::toString(ModuleCategory category)
{
    switch (category) {
    case ModuleCategory::SYNTALOS_DEV:
        return "sydevel";
    case ModuleCategory::EXAMPLE:
        return "example";
    case ModuleCategory::DEVICE:
        return "device";
    case ModuleCategory::GENERATOR:
        return "generator";
    case ModuleCategory::SCRIPTING:
        return "scripting";
    case ModuleCategory::DISPLAY:
        return "display";
    case ModuleCategory::WRITERS:
        return "writers";
    case ModuleCategory::PROCESSING:
        return "processing";
    default:
        return "none";
    }
}

ModuleCategory Syntalos::moduleCategoryFromString(const QString &categoryStr)
{
    static const QMap<QString, ModuleCategory> categoryMap = {
        {"none",       ModuleCategory::NONE        },
        {"sydevel",    ModuleCategory::SYNTALOS_DEV},
        {"example",    ModuleCategory::EXAMPLE     },
        {"device",     ModuleCategory::DEVICE      },
        {"generator",  ModuleCategory::GENERATOR   },
        {"scripting",  ModuleCategory::SCRIPTING   },
        {"display",    ModuleCategory::DISPLAY     },
        {"writers",    ModuleCategory::WRITERS     },
        {"processing", ModuleCategory::PROCESSING  }
    };

    return categoryMap.value(categoryStr, ModuleCategory::NONE);
}

ModuleCategories Syntalos::moduleCategoriesFromString(const QString &categoriesStr)
{
    ModuleCategories categories = ModuleCategory::NONE;
    auto catsList = categoriesStr.split(';', Qt::SkipEmptyParts);
    for (const QString &name : catsList)
        categories |= moduleCategoryFromString(name);

    return categories;
}

class AbstractModule::Private
{
public:
    Private()
        : state(ModuleState::INITIALIZING),
          modIndex(0),
          simpleStorageNames(true),
          initialized(false)
    {
    }
    ~Private() {}

    std::atomic<ModuleState> state;
    QString id;
    QString name;
    QString lastError;
    int modIndex;
    uint potentialNoaffinityCPUCount;
    int defaultRealtimePriority;
    static int s_eventsMaxModulesPerThread;

    QList<QPair<QWidget *, bool>> displayWindows;
    QList<QPair<QWidget *, bool>> settingsWindows;

    bool simpleStorageNames;
    std::shared_ptr<EDLGroup> rootDataGroup;
    std::shared_ptr<EDLDataset> defaultDataset;

    bool initialized;
    bool runIsEmphemeral;
};

// instantiate static field
int AbstractModule::Private::s_eventsMaxModulesPerThread = -1;

AbstractModule::AbstractModule(QObject *parent)
    : QObject(parent),
      m_running(false),
      d(new AbstractModule::Private)
{
    d->id = QStringLiteral("unknown");
    d->name = QStringLiteral("Unknown Module");
    d->s_eventsMaxModulesPerThread = -1;
    d->runIsEmphemeral = false;
}

AbstractModule::AbstractModule(const QString &id, QObject *parent)
    : AbstractModule(parent)
{
    setId(id);
}

AbstractModule::~AbstractModule()
{
    // delete windows if we own them
    for (auto &wp : d->displayWindows) {
        if (wp.second)
            delete wp.first;
    }
    for (auto &wp : d->settingsWindows) {
        if (wp.second)
            delete wp.first;
    }
}

ModuleState AbstractModule::state() const
{
    return d->state;
}

void AbstractModule::setStateDormant()
{
    if ((d->state == ModuleState::RUNNING) || (d->state == ModuleState::INITIALIZING))
        setState(ModuleState::DORMANT);
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
    d->name = simplifyStrForModuleName(name);
    for (auto &oport : outPorts()) {
        oport->streamVar()->setCommonMetadata(d->id, d->name, oport->title());
    }
    emit nameChanged(d->name);
}

ModuleDriverKind AbstractModule::driver() const
{
    return ModuleDriverKind::NONE;
}

ModuleFeatures AbstractModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY | ModuleFeature::SHOW_SETTINGS | ModuleFeature::SHOW_ACTIONS;
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
    /* do nothing */
}

void AbstractModule::processUiEvents()
{
    /* do nothing */
}

void AbstractModule::stop()
{
    m_running = false;
    setState(ModuleState::IDLE);
}

void AbstractModule::finalize()
{
    /* do nothing */
}

void AbstractModule::showDisplayUi()
{
    const bool onlyOne = d->displayWindows.size() == 1;
    for (auto const wp : d->displayWindows) {
        if (onlyOne)
            wp.first->setWindowTitle(name());
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
    const bool onlyOne = d->settingsWindows.size() == 1;
    for (auto const wp : d->settingsWindows) {
        if (onlyOne)
            wp.first->setWindowTitle(QStringLiteral("%1 - Settings").arg(name()));

        // set an initial position if we do not have one yet
        if (wp.first->pos().isNull()) {
            auto pos = QCursor::pos();
            pos.setY(pos.y() - (wp.first->height() / 2));
            wp.first->move(pos);
        }

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
    QList<QAction *> res;
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

void AbstractModule::updateStartWaitCondition(OptionalWaitCondition *)
{
    /* do nothing */
}

QString AbstractModule::lastError() const
{
    return d->lastError;
}

QString AbstractModule::moduleRootDir() const
{
    auto moduleDir = QStringLiteral("%1/../modules/%2").arg(QCoreApplication::applicationDirPath(), id());
    QFileInfo checkModDir(moduleDir);
    if (moduleDir.startsWith("/usr/") || !checkModDir.exists())
        moduleDir = QStringLiteral("%1/%2").arg(SY_MODULESDIR, id());
    const auto origModuleDir = moduleDir;
    QFileInfo fi(moduleDir);
    moduleDir = fi.canonicalFilePath();

    return moduleDir.isEmpty() ? origModuleDir : moduleDir;
}

void AbstractModule::setEventsMaxModulesPerThread(int maxModuleCount)
{
    d->s_eventsMaxModulesPerThread = maxModuleCount;
}

int AbstractModule::eventsMaxModulesPerThread() const
{
    return d->s_eventsMaxModulesPerThread;
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

QList<std::shared_ptr<VarStreamInputPort>> AbstractModule::inPorts() const
{
    return m_inPorts.values();
}

QList<std::shared_ptr<StreamOutputPort>> AbstractModule::outPorts() const
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

QList<QPair<intervalEventFunc_t, int>> AbstractModule::intervalEventCallbacks() const
{
    return m_intervalEventCBList;
}

QList<QPair<recvDataEventFunc_t, std::shared_ptr<VariantStreamSubscription>>> AbstractModule::recvDataEventCallbacks()
    const
{
    return m_recvDataEventCBList;
}

bool AbstractModule::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        raiseError(QStringLiteral("Unable to create directory '%1'.").arg(dir));
        return false;
    }

    return true;
}

void AbstractModule::appProcessEvents()
{
    qApp->processEvents();
}

QString AbstractModule::datasetNameSuggestion(bool lowercase) const
{
    auto rawName = name();
    if (lowercase)
        rawName = rawName.toLower();

    QString datasetName;
    if (d->simpleStorageNames)
        datasetName = simplifyStrForFileBasenameLower(rawName);
    else
        datasetName = simplifyStrForFileBasename(rawName);

    // this check should never fail, the dataset name should never consist only
    // of unsuitable characters - but just in ase it does, we safeguard against that
    if (datasetName.isEmpty())
        return createRandomString(8);
    return datasetName;
}

QString AbstractModule::datasetNameFromSubMetadata(const QVariantHash &subMetadata) const
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
    else if (d->simpleStorageNames)
        dataName = simplifyStrForFileBasenameLower(dataName);

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
    if (d->simpleStorageNames)
        dataName = simplifyStrForFileBasenameLower(dataName);

    return dataName;
}

QString AbstractModule::datasetNameFromParameters(const QString &preferredName, const QVariantHash &subMetadata) const
{
    QString datasetName;

    // if we have subscription metadata, try to use that data to determine the
    // data set name
    if (!subMetadata.isEmpty()) {
        datasetName = datasetNameFromSubMetadata(subMetadata);
    }

    // attempt to use the preferred name (if we have one, and don't already have a dataset name from subscription
    // metadata)
    if (datasetName.isEmpty() && !preferredName.isEmpty()) {
        if (d->simpleStorageNames)
            datasetName = simplifyStrForFileBasenameLower(preferredName);
        else
            datasetName = simplifyStrForFileBasename(preferredName);
    }

    // just set our module name if we still have no data set name
    if (datasetName.isEmpty())
        datasetName = datasetNameSuggestion();

    return datasetName;
}

std::shared_ptr<EDLDataset> AbstractModule::createDefaultDataset(
    const QString &preferredName,
    const QVariantHash &subMetadata)
{
    if (d->defaultDataset.get() != nullptr)
        return d->defaultDataset;
    if (d->rootDataGroup.get() == nullptr) {
        qCritical().noquote().nospace()
            << "Module \"" << name()
            << "\" tried to obtain its default dataset, but no root storage group has been set yet.";
        return nullptr;
    }

    d->defaultDataset = createDatasetInGroup(d->rootDataGroup, preferredName, subMetadata);
    return d->defaultDataset;
}

std::shared_ptr<EDLDataset> AbstractModule::createDatasetInGroup(
    std::shared_ptr<EDLGroup> group,
    const QString &preferredName,
    const QVariantHash &subMetadata)
{
    const auto datasetName = datasetNameFromParameters(preferredName, subMetadata);

    // check if the dataset already exists
    auto dset = group->datasetByName(datasetName);
    if (dset.get() != nullptr) {
        if (!dset->isEmpty()) {
            raiseError(QStringLiteral("Tried to use dataset '%1' for storage, but the dataset was already in use. "
                                      "Please ensure unique names for data storage!")
                           .arg(datasetName));
            return nullptr;
        }
    }

    return group->datasetByName(datasetName, true);
}

std::shared_ptr<EDLDataset> AbstractModule::getDefaultDataset()
{
    return d->defaultDataset;
}

std::shared_ptr<EDLDataset> AbstractModule::getDatasetInGroup(
    std::shared_ptr<EDLGroup> group,
    const QString &preferredName,
    const QVariantHash &subMetadata)
{
    const auto datasetName = datasetNameFromParameters(preferredName, subMetadata);
    return group->datasetByName(datasetName);
}

std::shared_ptr<EDLGroup> AbstractModule::createStorageGroup(const QString &groupName)
{
    if (d->rootDataGroup.get() == nullptr) {
        qCritical().noquote() << "Module" << name()
                              << "tried to create a new storage group, but no root storage group has been set yet.";
        return nullptr;
    }

    return d->rootDataGroup->groupByName(groupName, true);
}

QWidget *AbstractModule::addDisplayWindow(QWidget *window, bool owned)
{
    d->displayWindows.append(qMakePair(window, owned));
    return window;
}

QWidget *AbstractModule::addSettingsWindow(QWidget *window, bool owned)
{
    d->settingsWindows.append(qMakePair(window, owned));
    return window;
}

void AbstractModule::clearDataReceivedEventRegistrations()
{
    m_recvDataEventCBList.clear();
}

std::unique_ptr<FreqCounterSynchronizer> AbstractModule::initCounterSynchronizer(double frequencyHz)
{
    if ((d->state != ModuleState::PREPARING) && (d->state != ModuleState::READY) && (d->state != ModuleState::RUNNING))
        return nullptr;
    assert(frequencyHz > 0);

    auto synchronizer = std::make_unique<FreqCounterSynchronizer>(m_syTimer, name(), frequencyHz);
    synchronizer->setNotifyCallbacks(
        [this](const QString &id, const TimeSyncStrategies &strategies, const microseconds_t &tolerance) {
            Q_EMIT synchronizerDetailsChanged(id, strategies, tolerance);
        },
        [this](const QString &id, const microseconds_t &currentOffset) {
            Q_EMIT synchronizerOffsetChanged(id, currentOffset);
        });

    return synchronizer;
}

std::unique_ptr<SecondaryClockSynchronizer> AbstractModule::initClockSynchronizer(double expectedFrequencyHz)
{
    if ((d->state != ModuleState::PREPARING) && (d->state != ModuleState::READY) && (d->state != ModuleState::RUNNING))
        return nullptr;

    auto synchronizer = std::make_unique<SecondaryClockSynchronizer>(m_syTimer, name());
    if (expectedFrequencyHz > 0)
        synchronizer->setExpectedClockFrequencyHz(expectedFrequencyHz);

    synchronizer->setNotifyCallbacks(
        [this](const QString &id, const TimeSyncStrategies &strategies, const microseconds_t &tolerance) {
            Q_EMIT synchronizerDetailsChanged(id, strategies, tolerance);
        },
        [this](const QString &id, const microseconds_t &currentOffset) {
            Q_EMIT synchronizerOffsetChanged(id, currentOffset);
        });

    return synchronizer;
}

uint AbstractModule::potentialNoaffinityCPUCount() const
{
    return d->potentialNoaffinityCPUCount;
}

int AbstractModule::defaultRealtimePriority() const
{
    return d->defaultRealtimePriority;
}

bool AbstractModule::isEphemeralRun() const
{
    return d->runIsEmphemeral;
}

void AbstractModule::usbHotplugEvent(UsbHotplugEventKind kind)
{
    /* do nothing */
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

        const auto mw = qobject_cast<QMainWindow *>(wp.first);
        if (mw != nullptr)
            info.insert("state", QString::fromUtf8(mw->saveState().toBase64()));

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
        else
            wp.first->hide();

        const auto b64Geometry = winfo.value("geometry").toString();
        wp.first->restoreGeometry(QByteArray::fromBase64(b64Geometry.toUtf8()));

        const auto mw = qobject_cast<QMainWindow *>(wp.first);
        if (mw != nullptr) {
            const auto b64State = winfo.value("state").toString();
            mw->restoreState(QByteArray::fromBase64(b64State.toUtf8()));
        }
    }
}

void AbstractModule::setState(ModuleState state)
{
    d->state = state;
    emit stateChanged(state);
}

void AbstractModule::raiseError(const QString &message)
{
    // if there is multiple errors emitted, likely caused by the first one, we only bubble up the first one
    if (d->state == ModuleState::ERROR) {
        qCritical().noquote().nospace() << "Not escalating subsequent error from module '" << name()
                                        << "': " << message;
        return;
    }

    d->lastError = message;
    setState(ModuleState::ERROR);
    qCritical().noquote().nospace() << "Error raised by module '" << name() << "': " << message;
    emit error(message);
}

void AbstractModule::setId(const QString &id)
{
    d->id = id;
}

void AbstractModule::setIndex(int index)
{
    d->modIndex = index;
}

void AbstractModule::setSimpleStorageNames(bool enabled)
{
    d->simpleStorageNames = enabled;
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

void AbstractModule::setPotentialNoaffinityCPUCount(uint coreN)
{
    d->potentialNoaffinityCPUCount = coreN;
}

void AbstractModule::setDefaultRTPriority(int prio)
{
    d->defaultRealtimePriority = prio;
}

void AbstractModule::setEphemeralRun(bool isEphemeral)
{
    d->runIsEmphemeral = isEphemeral;
}

void AbstractModule::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}

void AbstractModule::setTimer(std::shared_ptr<SyncTimer> timer)
{
    m_syTimer = timer;
}
