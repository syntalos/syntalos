/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "engine.h"

// When compiled with -fsanitize=address or -fsanitize=leak, we modify behavior
// a bit. E.g. we may trigger an explicit LSan leak check after every run, so
// leaks are surfaced between runs, not only at process exit.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SY_HAS_LSAN 1
#endif
#endif
#if !defined(SY_HAS_LSAN) && defined(__SANITIZE_ADDRESS__)
#define SY_HAS_LSAN 1
#endif
#if !defined(SY_HAS_LSAN) && defined(__SANITIZE_LEAK__)
#define SY_HAS_LSAN 1
#endif
#ifdef SY_HAS_LSAN
#include <sanitizer/lsan_interface.h>
#endif

#include <QCoreApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <memory>
#include <functional>
#include <filesystem>
#include <libusb.h>
#include <pthread.h>

#include "cpuaffinity.h"
#include "globalconfig.h"
#include "meminfo.h"
#include "moduleeventthread.h"
#include "modulelibrary.h"
#include "mlinkmodule.h"
#include "rtkit.h"
#include "sysinfo.h"
#include "datactl/syclock.h"
#include "datactl/edlstorage.h"
#include "utils/misc.h"
#include "utils/tomlutils.h"

namespace Syntalos
{
Q_LOGGING_CATEGORY(logEngine, "engine")
}

static_assert(
    std::is_same<std::thread::native_handle_type, pthread_t>::value,
    "Native thread implementation for std::thread must be pthread");

using namespace Syntalos;

static int engineUsbHotplugDispatchCB(
    struct libusb_context *ctx,
    struct libusb_device *dev,
    libusb_hotplug_event event,
    void *enginePtr);

class ThreadDetails
{
public:
    explicit ThreadDetails()
        : name(createRandomString(8)),
          niceness(0),
          allowedRTPriority(0)
    {
    }

    QString name;
    int niceness;
    int allowedRTPriority;
    std::vector<uint> cpuAffinity;
};

/**
 * @brief Simple pthread wrapper for module-dedicated Syntalos threads.
 */
class SyThread
{
public:
    enum Backend {
        BackendDefault,
        BackendQThread
    };

    explicit SyThread(
        ThreadDetails &details,
        AbstractModule *module,
        OptionalWaitCondition *waitCondition,
        Backend threadBackend = BackendDefault)
        : m_created(false),
          m_threadBackend(BackendDefault),
          m_pThread(0),
          m_joined(false),
          m_td(details),
          m_mod(module),
          m_waitCond(waitCondition)
    {
        if (threadBackend == BackendQThread) {
            m_threadBackend = BackendQThread;
            m_qThread = std::unique_ptr<QThread>(QThread::create(&SyThread::executeModuleThread, this));
            m_qThread->start();
        } else {
            auto r = pthread_create(&m_pThread, nullptr, &SyThread::executeModuleThread, this);
            if (r != 0)
                throw std::runtime_error{strerror(r)};
        }
        m_created = true;
    }

    ~SyThread()
    {
        if (m_created)
            join();
    }

    SyThread(const SyThread &other) = delete;
    SyThread &operator=(const SyThread &other) = delete;
    SyThread(SyThread &&other) = delete;

    void join()
    {
        if (m_joined || !m_created)
            return;

        if (m_threadBackend == BackendQThread) {
            m_qThread->quit();
            m_qThread->wait();
        } else {
            auto r = pthread_join(m_pThread, nullptr);
            if (r != 0)
                throw std::runtime_error{strerror(r)};
        }

        m_joined = true;
    }

    bool joinTimeout(uint seconds)
    {
        if (m_threadBackend == BackendQThread) {
            return m_qThread->wait(seconds * MS_PER_S);
        } else {
            struct timespec ts = {0};

            if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
                qCCritical(logEngine).noquote() << "Unable to obtain absolute time since epoch!";
                join();
                return true;
            }

            ts.tv_sec += seconds;
            if (pthread_timedjoin_np(m_pThread, nullptr, &ts) == 0) {
                m_joined = true;
                return true;
            }

            return false;
        }
    }

private:
    bool m_created;
    Backend m_threadBackend;
    pthread_t m_pThread;
    std::unique_ptr<QThread> m_qThread;

    bool m_joined;
    ThreadDetails m_td;
    AbstractModule *m_mod;
    OptionalWaitCondition *m_waitCond;

    /**
     * @brief Main entry point for engine-managed module threads.
     */
    static void *executeModuleThread(void *udata)
    {
        auto self = static_cast<SyThread *>(udata);
        pthread_setname_np(pthread_self(), qPrintable(self->m_td.name.mid(0, 15)));

        // set higher niceness for this thread
        if (self->m_td.niceness != 0)
            setCurrentThreadNiceness(self->m_td.niceness);

        // set CPU affinity
        if (!self->m_td.cpuAffinity.empty())
            thread_set_affinity_from_vec(pthread_self(), self->m_td.cpuAffinity);

        if (self->m_mod->features().testFlag(ModuleFeature::REALTIME)) {
            if (setCurrentThreadRealtime(self->m_td.allowedRTPriority))
                qCDebug(logEngine).noquote().nospace()
                    << "Module thread for '" << self->m_mod->name() << "' set to realtime mode.";
        }

        self->m_mod->runThread(self->m_waitCond);

        if (self->m_threadBackend != BackendQThread)
            pthread_exit(nullptr);
        return nullptr;
    }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"

class EngineResourceMonitorData
{
public:
    struct SubscriptionBufferWatchData {
        VariantStreamSubscription *sub;
        VarStreamInputPort *port;
        ConnectionHeatLevel heat;
    };

    std::vector<SubscriptionBufferWatchData> monitoredSubscriptions;
    QString exportDirPath;

    bool diskSpaceWarningEmitted;
    bool memoryWarningEmitted;
    bool subBufferWarningEmitted;
    double prevMemAvailablePercent;
    bool emergencyOOMStop;

    QTimer diskSpaceCheckTimer;
    QTimer memCheckTimer;
    QTimer subBufferCheckTimer;
};

class Engine::Private
{
public:
    Private()
        : initialized(false),
          alwaysOverrideExportDir(false),
          monitoring(new EngineResourceMonitorData),
          usbCtx(nullptr)
    {
    }
    ~Private() {}

    bool initialized;
    SysInfo *sysInfo;
    GlobalConfig *gconf;
    QWidget *parentWidget;
    QList<AbstractModule *> presentModules;
    ModuleLibrary *modLibrary;
    std::shared_ptr<SyncTimer> timer;
    std::vector<uint> mainThreadCoreAffinity;

    QString exportBaseDir;
    QString exportDir;
    QString exportName;
    bool exportDirIsTempDir;
    bool exportDirIsValid;
    bool alwaysOverrideExportDir;

    EDLAuthor experimenter;
    TestSubject testSubject;
    QString experimentIdTmpl;
    QString experimentIdFinal;
    bool simpleStorageNames;
    bool clockTimeInExportDir;
    bool flatExportDirName;
    QList<ExportPathComponent> exportDirLayout;

    std::atomic_bool active;
    std::atomic_bool running;
    std::atomic_bool failed;
    QString runFailedReason;
    bool runIsEphemeral;

    QString lastRunExportDir;
    QString nextRunComment;

    QList<QPair<AbstractModule *, QString>> pendingErrors;

    bool saveInternal;
    std::shared_ptr<EDLGroup> edlInternalData;
    QHash<std::string, std::shared_ptr<TimeSyncFileWriter>> internalTSyncWriters;

    std::unique_ptr<EngineResourceMonitorData> monitoring;
    int runCount;
    int runCountPadding;

    libusb_context *usbCtx;
    libusb_hotplug_callback_handle usbHotplugCBHandle;
    QTimer *usbEventsTimer;
};
#pragma GCC diagnostic pop

Engine::Engine(QWidget *parentWidget)
    : QObject(parentWidget),
      d(new Engine::Private)
{
    d->gconf = new GlobalConfig(this);
    d->saveInternal = false;
    d->sysInfo = SysInfo::get();
    d->exportDirIsValid = false;
    d->alwaysOverrideExportDir = false;
    d->active = false;
    d->running = false;
    d->simpleStorageNames = true;
    d->clockTimeInExportDir = false;
    d->flatExportDirName = false;
    d->exportDirLayout = {
        ExportPathComponent::SubjectId,
        ExportPathComponent::Time,
        ExportPathComponent::ExperimentId,
    };
    d->modLibrary = new ModuleLibrary(d->gconf, this);
    d->parentWidget = parentWidget;
    d->timer = std::make_shared<SyncTimer>();
    d->runIsEphemeral = false;
    d->mainThreadCoreAffinity.clear();
    d->runCount = 0;
    d->runCountPadding = 1;
    d->monitoring->emergencyOOMStop = d->gconf->emergencyOOMStop();

#if defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x0100010A)
    if (libusb_init_context(&d->usbCtx, nullptr, 0) != 0)
        qCCritical(logEngine).noquote() << "Unable to initialize libusb context!";
#else
    if (libusb_init(&d.usbCtx) != 0)
        qCCritical(logEngine).noquote() << "Unable to initialize libusb context!";
#endif

    qCDebug(logEngine, "Application data directory: %s", qPrintable(d->gconf->appDataLocation()));

    // allow sending states via Qt queued connections,
    // and also register all other transmittable data types
    // with the meta object system
    registerStreamMetaTypes();

    // for time synchronizers - this metatype can not be registered in the
    // synchronizers, as they may run in arbitrary threads and some may even
    // try to register simultaneously
    qRegisterMetaType<TimeSyncStrategies>();

    // register dispatch callback for USB hotplug events
    d->usbEventsTimer = new QTimer;
    d->usbEventsTimer->setInterval(10);
    int rc = libusb_hotplug_register_callback(
        d->usbCtx,
        (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
        LIBUSB_HOTPLUG_NO_FLAGS,
        LIBUSB_HOTPLUG_MATCH_ANY,
        LIBUSB_HOTPLUG_MATCH_ANY,
        LIBUSB_HOTPLUG_MATCH_ANY,
        engineUsbHotplugDispatchCB,
        this,
        &d->usbHotplugCBHandle);
    if (rc != LIBUSB_SUCCESS) {
        qCWarning(logEngine).noquote()
            << "Unable to register USB hotplug callback: Can not notify modules of USB hotplug events.";
        d->usbHotplugCBHandle = -1;
    } else {
        connect(d->usbEventsTimer, &QTimer::timeout, [this]() {
            struct timeval tv{0, 0};
            libusb_handle_events_timeout_completed(d->usbCtx, &tv, nullptr);
        });
        d->usbEventsTimer->start();
    }
}

Engine::~Engine()
{
    delete d->usbEventsTimer;
    if (d->usbHotplugCBHandle != -1)
        libusb_hotplug_deregister_callback(d->usbCtx, d->usbHotplugCBHandle);

    // delete libusb context
    libusb_exit(d->usbCtx);
}

static int engineUsbHotplugDispatchCB(
    struct libusb_context *ctx,
    struct libusb_device *dev,
    libusb_hotplug_event event,
    void *enginePtr)
{
    Q_UNUSED(ctx)
    Q_UNUSED(dev)

    auto engine = static_cast<Engine *>(enginePtr);
    UsbHotplugEventKind kind = UsbHotplugEventKind::NONE;
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        kind = UsbHotplugEventKind::DEVICE_ARRIVED;
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        kind = UsbHotplugEventKind::DEVICE_LEFT;
    } else {
        qCDebug(logEngine).noquote() << "Unhandled USB hotplug event:" << event;
        return 0;
    }

    engine->notifyUsbHotplugEvent(kind);
    return 0;
}

bool Engine::initialize()
{
    if (d->initialized) {
        qCCritical(logEngine).noquote() << "Tried to initialize engine twice! This is an error.";
        return true;
    }

    if (d->modLibrary->load()) {
        d->initialized = true;
        return true;
    }

    return false;
}

ModuleLibrary *Engine::library() const
{
    return d->modLibrary;
}

SysInfo *Engine::sysInfo() const
{
    return d->sysInfo;
}

QString Engine::exportBaseDir() const
{
    return d->exportBaseDir;
}

void Engine::setExportBaseDir(const QString &dataDir)
{
    d->exportBaseDir = dataDir;

    if (dataDir.isEmpty()) {
        d->exportDir.clear();
        d->exportDirIsValid = false;
        d->exportDirIsTempDir = false;
        return;
    }

    d->exportDirIsValid = QDir().exists(d->exportBaseDir);
    d->exportDirIsTempDir = false;
    if (d->exportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        || d->exportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))) {
        d->exportDirIsTempDir = true;
    }

    // update the actual export directory location, using the test subject data and the
    // current date
    refreshExportDirPath();
}

bool Engine::exportDirIsTempDir() const
{
    return d->exportDirIsTempDir;
}

bool Engine::exportDirIsValid() const
{
    return d->exportDirIsValid;
}

void Engine::setAlwaysOverrideExportDir(bool alwaysOverride)
{
    d->alwaysOverrideExportDir = alwaysOverride;
}

bool Engine::alwaysOverrideExportDir() const
{
    return d->alwaysOverrideExportDir;
}

TestSubject Engine::testSubject() const
{
    return d->testSubject;
}

void Engine::setTestSubject(const TestSubject &ts)
{
    d->testSubject = ts;
    d->testSubject.id = d->testSubject.id.trimmed();
    refreshExportDirPath();
}

QString Engine::experimentId() const
{
    return d->experimentIdTmpl;
}

void Engine::setExperimentId(const QString &id)
{
    d->experimentIdTmpl = id.trimmed();
    refreshExportDirPath();
}

bool Engine::hasExperimentIdReplaceables() const
{
    return d->experimentIdTmpl.contains("{n}") || d->experimentIdTmpl.contains("{time}");
}

EDLAuthor Engine::experimenter() const
{
    return d->experimenter;
}

void Engine::setExperimenter(const EDLAuthor &person)
{
    d->experimenter = person;
}

bool Engine::simpleStorageNames() const
{
    return d->simpleStorageNames;
}

void Engine::setSimpleStorageNames(bool enabled)
{
    d->simpleStorageNames = enabled;
}

bool Engine::clockTimeInExportDir() const
{
    return d->clockTimeInExportDir;
}

void Engine::setClockTimeInExportDir(bool enabled)
{
    bool update = d->clockTimeInExportDir != enabled;
    d->clockTimeInExportDir = enabled;
    if (update)
        refreshExportDirPath();
}

bool Engine::flatExportDir() const
{
    return d->flatExportDirName;
}

void Engine::setFlatExportDir(bool enabled)
{
    bool update = d->flatExportDirName != enabled;
    d->flatExportDirName = enabled;
    if (update)
        refreshExportDirPath();
}

QList<ExportPathComponent> Engine::exportDirLayout() const
{
    return d->exportDirLayout;
}

void Engine::setExportDirLayout(const QList<ExportPathComponent> &layout)
{
    d->exportDirLayout = normalizeExportDirLayout(layout);
    refreshExportDirPath();
}

QString Engine::exportDir() const
{
    return d->exportDir;
}

bool Engine::isRunning() const
{
    return d->running;
}

bool Engine::isActive() const
{
    return d->active;
}

bool Engine::hasFailed() const
{
    return d->failed;
}

milliseconds_t Engine::currentRunElapsedTime() const
{
    if (!d->running)
        return milliseconds_t(0);
    return d->timer->timeSinceStartMsec();
}

void Engine::resetSuccessRunsCounter()
{
    d->runCount = 0;
}

int Engine::successRunsCount()
{
    return d->runCount;
}

void Engine::setRunCountExpectedMax(int maxValue)
{
    // configure zero padding for "n" replaceable
    d->runCountPadding = 1;
    if (maxValue >= 10)
        d->runCountPadding = 2;
    if (maxValue >= 100)
        d->runCountPadding = 3;
}

AbstractModule *Engine::createModule(const QString &id, const QString &name)
{
    auto modInfo = d->modLibrary->moduleInfo(id);
    if (modInfo == nullptr)
        return nullptr;

    // Ensure we don't register a module twice that should only exist once
    if (modInfo->singleton()) {
        for (auto &emod : d->presentModules) {
            if (emod->id() == id)
                return nullptr;
        }
    }

    // notify others that we started to create & initialize a module
    emit moduleInitStarted();

    auto mod = modInfo->createModule();
    assert(mod);
    mod->setId(modInfo->id());
    mod->setIndex(modInfo->count() + 1);
    modInfo->setCount(mod->index());
    if (name.isEmpty()) {
        if (modInfo->count() > 1)
            mod->setName(QStringLiteral("%1 %2").arg(modInfo->name()).arg(modInfo->count()));
        else
            mod->setName(simplifyStrForModuleName(modInfo->name()));
    } else {
        mod->setName(simplifyStrForModuleName(name));
    }

    d->presentModules.append(mod);
    emit moduleCreated(modInfo.get(), mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    mod->setState(ModuleState::INITIALIZING);
    qApp->processEvents();
    if (!mod->initialize()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Module initialization failed"),
            QStringLiteral("Failed to initialize module '%1', it can not be added. %2")
                .arg(mod->id(), mod->lastError()),
            QMessageBox::Ok);
        removeModule(mod);
        emit moduleInitDone();
        return nullptr;
    }
    // Ensure modules are marked as initialized at this point, to make the initialize() guards
    // work. This call is inert if the module has already set it by itself.
    mod->setInitialized();

    // now listen to errors emitted by this module
    connect(mod, &AbstractModule::error, this, &Engine::receiveModuleError);

    // connect synchronizer details callbacks
    connect(
        mod,
        &AbstractModule::synchronizerDetailsChanged,
        this,
        &Engine::onSynchronizerDetailsChanged,
        Qt::QueuedConnection);
    connect(
        mod,
        &AbstractModule::synchronizerOffsetChanged,
        this,
        &Engine::onSynchronizerOffsetChanged,
        Qt::QueuedConnection);

    mod->setState(ModuleState::IDLE);

    // module initialization is done
    emit moduleInitDone();
    return mod;
}

bool Engine::removeModule(AbstractModule *mod)
{
    auto id = mod->id();
    if (d->presentModules.removeOne(mod)) {
        // Update module info
        auto modInfo = d->modLibrary->moduleInfo(id);
        modInfo->setCount(modInfo->count() - 1);

        emit modulePreRemove(mod);
        delete mod;
        return true;
    }

    return false;
}

void Engine::removeAllModules()
{
    if (d->running)
        stop();
    if (d->active) {
        qCInfo(logEngine).noquote()
            << "Requested to remove all modules, but engine is still active. Waiting for it to shut down.";
        for (int i = 0; i < 800; ++i) {
            if (!d->active)
                break;
            QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
            QThread::msleep(50);
        };
        if (d->active) {
            qFatal(
                "Requested to remove all modules on an active engine that did not manage to shut down in time. This "
                "must not happen.");
            assert(0);
        }
    }

    foreach (auto mod, d->presentModules)
        removeModule(mod);
}

QList<AbstractModule *> Engine::presentModules() const
{
    return d->presentModules;
}

AbstractModule *Engine::moduleByName(const QString &name) const
{
    // FIXME: In case we grow projects with huge module counts, we
    // will actually want a QHash index of modules here, so speed up
    // this function (which may otherwise slow down project loading times)
    for (const auto &mod : d->presentModules) {
        if (mod->name() == name)
            return mod;
    }
    return nullptr;
}

QString Engine::lastRunExportDir() const
{
    return d->lastRunExportDir;
}

QString Engine::readRunComment(const QString &runExportDir) const
{
    if (runExportDir.isEmpty())
        return d->nextRunComment;

    QString parseError;
    auto attrs = parseTomlFile(QStringLiteral("%1/attributes.toml").arg(runExportDir), parseError);
    if (!parseError.isEmpty()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Can not read comment"),
            QStringLiteral("Unable to parse EDL metadata in %1:\n%2").arg(runExportDir, parseError));
        return nullptr;
    }

    return attrs["user_comment"].toString();
}

void Engine::setRunComment(const QString &comment, const QString &runExportDir)
{
    if (runExportDir.isEmpty()) {
        d->nextRunComment = comment;
        return;
    }

    QString parseError;
    const auto attrsFname = QStringLiteral("%1/attributes.toml").arg(runExportDir);
    auto attrs = parseTomlFile(attrsFname, parseError);
    if (!parseError.isEmpty()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Can not save comment"),
            QStringLiteral("Unable to parse EDL metadata in %1:\n%2").arg(runExportDir, parseError));
        return;
    }

    attrs["user_comment"] = comment;

    auto document = qVariantHashToTomlTable(attrs);
    std::ofstream file;
    file.open(attrsFname.toStdString());
    file << document << "\n";
    file.close();
}

bool Engine::saveInternalDiagnostics() const
{
    return d->saveInternal;
}

void Engine::setSaveInternalDiagnostics(bool save)
{
    d->saveInternal = save;
}

int Engine::obtainSleepShutdownIdleInhibitor()
{
    QDBusInterface iface(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QDBusConnection::systemBus());
    if (!iface.isValid()) {
        qCDebug(logEngine).noquote() << "Unable to connect to logind DBus interface";
        return -1;
    }

    QDBusReply<QDBusUnixFileDescriptor> reply;
    reply = iface.call(
        QStringLiteral("Inhibit"),
        QStringLiteral("sleep:shutdown:idle"),
        QCoreApplication::applicationName(),
        QStringLiteral("Experiment run in progress"),
        QStringLiteral("block"));
    if (!reply.isValid()) {
        qCDebug(logEngine).noquote() << "Unable to request sleep/shutdown/idle inhibitor from logind.";
        return -1;
    }

    return ::dup(reply.value().fileDescriptor());
}

bool Engine::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        const auto message = QStringLiteral("Unable to create directory '%1'.").arg(dir);
        QMessageBox::critical(d->parentWidget, QStringLiteral("Error"), message);
        emitStatusMessage("OS error.");
        return false;
    }

    return true;
}

void Engine::makeFinalExperimentId()
{
    // replace substitution variables (create copy of template string first!)
    d->experimentIdFinal =
        QString(d->experimentIdTmpl)
            .replace(
                "{n}",
                QStringLiteral("%1").arg(
                    d->runCount + 1, d->runCountPadding > 1 ? d->runCountPadding : 1, 10, QLatin1Char('0')))
            .replace("{time}", QTime::currentTime().toString("hhmmss"));
}

void Engine::refreshExportDirPath()
{
    auto time = QDateTime::currentDateTime();
    auto currentDate = time.date().toString("yyyy-MM-dd");
    if (d->clockTimeInExportDir)
        currentDate = QStringLiteral("%1T%2").arg(currentDate, time.toString("hhmm"));
    makeFinalExperimentId();

    const auto result = arrangeExportDirName(
        d->exportDirLayout, d->testSubject.id, currentDate, d->experimentIdFinal, d->flatExportDirName);
    d->exportDir = QDir::cleanPath(QStringLiteral("%1/%2").arg(d->exportBaseDir, result.dirName));
    d->exportName = result.flatId;
}

void Engine::emitStatusMessage(const QString &message)
{
    qCDebug(logEngine).noquote() << message;
    emit statusMessage(message);
}

/**
 * @brief Core topological sort (Kahn's algorithm) shared by start and stop ordering.
 *
 * @param mods               Closed set of modules to order.
 * @param categoryPriorityFn Returns a numeric priority used only when all remaining
 *                           modules form a cycle. Lower value = placed earlier.
 *                           In the acyclic parts of the graph the standard topological
 *                           order (source-first, sink-last) is always used instead.
 * @param context            Label used in the cycle-resolution debug log ("start" / "stop").
 */
static QList<AbstractModule *> computeModuleOrder(
    const QList<AbstractModule *> &mods,
    const std::function<int(const AbstractModule *)> &categoryPriorityFn,
    const char *context)
{
    const auto modCount = mods.size();

    QList<AbstractModule *> result;
    result.reserve(modCount);

    if (modCount == 0)
        return result;

    // Build directed edges upstream->downstream, deduplicating parallel edges
    // (a module may have several input ports all fed by the same upstream).
    QHash<AbstractModule *, QSet<AbstractModule *>> downstreams;
    QHash<AbstractModule *, int> remainingInDeg;
    downstreams.reserve(modCount);
    remainingInDeg.reserve(modCount);
    for (auto mod : mods) {
        downstreams.insert(mod, {});
        remainingInDeg.insert(mod, 0);
    }

    for (auto mod : mods) {
        QSet<AbstractModule *> seenUpstream;
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;
            auto up = iport->outPort()->owner();
            // Skip self-loops and modules outside the active set.
            if (up == mod || !downstreams.contains(up))
                continue;
            // Count each unique upstream module only once per downstream module.
            if (seenUpstream.contains(up))
                continue;
            seenUpstream.insert(up);
            downstreams[up].insert(mod);
            remainingInDeg[mod]++;
        }
    }

    // Seed the ready queue with source modules (in-degree 0), in original list order.
    QList<AbstractModule *> queue;
    for (auto mod : mods) {
        if (remainingInDeg[mod] == 0)
            queue.append(mod);
    }

    QSet<AbstractModule *> placed;
    placed.reserve(modCount);

    while (result.size() < modCount) {
        if (queue.isEmpty()) {
            // All remaining modules are in cycle(s).
            // We break each cycle greedily by selecting the not-yet-placed module that
            // is most "source-like":
            //   1. Fewest remaining unresolved upstream edges (closest to a source).
            //   2. Category priority function.
            //   3. Most outgoing edges (unblocks the most successors).
            //   4. Alphabetical module name (for fully deterministic output).
            AbstractModule *best = nullptr;
            int bestIn = INT_MAX;
            int bestCatPri = INT_MAX;
            int bestOut = -1;
            for (auto mod : mods) {
                if (placed.contains(mod))
                    continue;
                const int inDeg = remainingInDeg[mod];
                const int catPri = categoryPriorityFn(mod);
                const int outDeg = static_cast<int>(downstreams[mod].size());
                if (!best || inDeg < bestIn || (inDeg == bestIn && catPri < bestCatPri)
                    || (inDeg == bestIn && catPri == bestCatPri && outDeg > bestOut)
                    || (inDeg == bestIn && catPri == bestCatPri && outDeg == bestOut && mod->name() < best->name())) {
                    best = mod;
                    bestIn = inDeg;
                    bestCatPri = catPri;
                    bestOut = outDeg;
                }
            }
            qCDebug(logEngine).noquote().nospace()
                << "Resolving module graph cycle for " << context << "-order by placing module '" << best->name()
                << "' first (remaining unresolved upstream edges: " << bestIn << ", category priority: " << bestCatPri
                << ")";
            queue.append(best);
        }

        auto mod = queue.takeFirst();
        // Guard: a module may be queued as a normal zero-in-degree candidate and
        // again as a cycle-breaker - place it only once.
        if (placed.contains(mod))
            continue;

        result.append(mod);
        placed.insert(mod);

        // Enqueue newly-unblocked downstreams in original list order.
        for (auto down : mods) {
            if (placed.contains(down) || !downstreams[mod].contains(down))
                continue;
            if (--remainingInDeg[down] == 0)
                queue.append(down);
        }
    }

    if (result.size() != modCount)
        qCCritical(logEngine).noquote() << "Invalid count of ordered modules:" << result.size() << "!=" << modCount;
    assert(result.size() == modCount);

    return result;
}

/**
 * @brief Compute both the start order and stop order for all active modules.
 *
 * Both lists are derived from the same topological sort (Kahn's algorithm) so
 * that in the acyclic parts of the graph sources always come first and sinks
 * last. Cycle-breaking uses different category priorities for start vs. stop:
 *
 *   Start: DEVICES > GENERATORS > others > SCRIPTING
 *          Hardware is ready before scripting modules begin to interact with it.
 *
 *   Stop:  SCRIPTING > others > GENERATORS > DEVICES
 *          Scripts stop issuing commands before the hardware they talk to shuts
 *          down; sources stop before sinks so recorders can drain their buffers.
 *
 * Inactive modules not considered for this run are collected as "inactive".
 */
Engine::ModuleRunOrder Engine::createModuleRunOrder() const
{
    auto categoryStartPriority = [this](const AbstractModule *mod) -> int {
        const auto info = d->modLibrary->moduleInfo(mod->id());
        if (!info)
            return 3;
        const auto cats = info->categories();
        if (cats.testFlag(ModuleCategory::DEVICES))
            return 1;
        if (cats.testFlag(ModuleCategory::GENERATORS))
            return 2;
        if (cats.testFlag(ModuleCategory::SCRIPTING))
            return 4;
        return 3;
    };

    // Stop priorities are the exact inverse of start priorities.
    auto categoryStopPriority = [this](const AbstractModule *mod) -> int {
        const auto info = d->modLibrary->moduleInfo(mod->id());
        if (!info)
            return 2;
        const auto cats = info->categories();
        if (cats.testFlag(ModuleCategory::SCRIPTING))
            return 1;
        if (cats.testFlag(ModuleCategory::GENERATORS))
            return 3;
        if (cats.testFlag(ModuleCategory::DEVICES))
            return 4;
        return 2;
    };

    ModuleRunOrder order;
    QList<AbstractModule *> activeMods;
    for (auto &mod : d->presentModules) {
        if (mod->modifiers().testFlag(ModuleModifier::ENABLED))
            activeMods.append(mod);
        else
            order.inactive.append(mod);
    }

    order.start = computeModuleOrder(activeMods, categoryStartPriority, "start");
    order.stop = computeModuleOrder(activeMods, categoryStopPriority, "stop");

    QStringList startNames;
    startNames.reserve(order.start.size());
    for (auto mod : order.start)
        startNames.append(mod->name());
    qCDebug(logEngine).noquote()
        << QStringLiteral("Module start order: %1").arg(startNames.join(QStringLiteral(" ⇢ ")));

    QStringList stopNames;
    stopNames.reserve(order.stop.size());
    for (auto mod : order.stop)
        stopNames.append(mod->name());
    qCDebug(logEngine).noquote() << QStringLiteral("Module stop order:  %1").arg(stopNames.join(QStringLiteral(" ⇢ ")));

    return order;
}

void Engine::notifyUsbHotplugEvent(UsbHotplugEventKind kind) const
{
    if (kind == UsbHotplugEventKind::NONE)
        return;
    if (d->active || d->running) {
        // we are running, we must not dispatch the event
        return;
    }

    // Notify modules via a queued invocation so the libusb hotplug/timer callback
    // returns before any module runs its handler. Without this deferral, a module
    // could open a modal dialog (e.g. QMessageBox) from within the callback, which
    // creates a nested event loop and can crash GLX context creation.
    for (auto mod : d->presentModules) {
        QMetaObject::invokeMethod(
            mod,
            [mod, kind] {
                mod->usbHotplugEvent(kind);
            },
            Qt::QueuedConnection);
    }
}

bool Engine::run()
{
    if (d->running)
        return false;

    d->failed = true;          // if we exit before this is reset, initialization has failed
    d->runIsEphemeral = false; // not a volatile run

    if (d->presentModules.isEmpty()) {
        QMessageBox::warning(
            d->parentWidget,
            QStringLiteral("Configuration error"),
            QStringLiteral(
                "You did not add a single module to be run.\nPlease add a module to the board to continue."));
        return false;
    }

    if (!exportDirIsValid() || d->exportBaseDir.isEmpty() || d->exportDir.isEmpty()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Configuration error"),
            QStringLiteral("Data export directory was not properly set. Can not continue."));
        return false;
    }

    // persistent data recording can be initialized!
    qCDebug(logEngine, "Initializing new persistent recording run");

    // test for available disk space and readyness of device
    QStorageInfo storageInfo(d->exportBaseDir);
    if (storageInfo.isValid() && storageInfo.isReady()) {
        auto mbAvailable = storageInfo.bytesAvailable() / 1000 / 1000;
        qCDebug(logEngine).noquote() << mbAvailable << "MB available in data export location";
        // TODO: Make the warning level configurable in global settings
        if (mbAvailable < 8000) {
            auto reply = QMessageBox::question(
                d->parentWidget,
                QStringLiteral("Disk is almost full - Continue anyway?"),
                QStringLiteral(
                    "The disk '%1' is located on has low amounts of space available (< 8 GB). "
                    "If this run generates more data than we have space for, it will fail (possibly "
                    "corrupting data). Continue anyway?")
                    .arg(d->exportBaseDir),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
        }
    } else {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Disk not ready"),
            QStringLiteral(
                "The disk device at '%1' is either invalid (not mounted) or not ready for operation. Can not continue.")
                .arg(d->exportBaseDir));
        return false;
    }

    // update paths and IDs
    makeFinalExperimentId();
    refreshExportDirPath();

    // safeguard against accidental data removals
    QDir deDir(d->exportDir);
    if (deDir.exists()) {
        if (!d->alwaysOverrideExportDir) {
            auto reply = QMessageBox::question(
                d->parentWidget,
                QStringLiteral("Existing data found - Continue anyway?"),
                QStringLiteral(
                    "The directory '%1' already contains data (likely from a previous run). "
                    "If you continue, the old data will be deleted. Continue and delete data?")
                    .arg(d->exportDir),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
        } else {
            qWarning().noquote().nospace() << "Overriding existing export directory \"" << d->exportDir
                                           << "\" without asking, because 'always override' setting is enabled.";
        }

        emitStatusMessage(QStringLiteral("Removing data from an old run..."));
        deDir.removeRecursively();
    }

    // perform the actual run, now that all error checking is done
    return runInternal(d->exportDir);
}

bool Engine::runEphemeral()
{
    if (d->running)
        return false;

    d->failed = true; // if we exit before this is reset, initialization has failed
    if (d->presentModules.isEmpty()) {
        QMessageBox::warning(
            d->parentWidget,
            QStringLiteral("Configuration error"),
            QStringLiteral(
                "You did not add a single module to be run.\nPlease add a module to the board to continue."));
        return false;
    }

    QTemporaryDir tempDir(QStringLiteral("%1/syntalos-tmprun-XXXXXX").arg(tempDirLargeRoot()));
    qCDebug(logEngine).noquote() << "Storing temporary data in:" << tempDir.path();
    if (!tempDir.isValid()) {
        QMessageBox::warning(
            d->parentWidget,
            QStringLiteral("Unable to run"),
            QStringLiteral("Unable to perform ephemeral run: Temporary data storage could not be created. %s")
                .arg(tempDir.errorString()));
        return false;
    }

    qCDebug(logEngine, "Initializing new ephemeral recording run");

    auto tempExportDir = tempDir.filePath("edl");

    // mark run as volatile
    d->runIsEphemeral = true;

    // update paths and IDs
    makeFinalExperimentId();
    refreshExportDirPath();

    // perform the actual run, in a temporary directory
    auto ret = runInternal(tempExportDir);

    qCDebug(logEngine, "Removing temporary storage directory");
    if (!tempDir.remove())
        qCDebug(logEngine, "Unable to remove temporary directory: %s", qPrintable(tempDir.errorString()));

    if (ret)
        qCDebug(logEngine, "Ephemeral run completed (result: success)");
    else
        qCDebug(logEngine, "Ephemeral run completed (result: failure)");
    return ret;
}

QHash<AbstractModule *, std::vector<uint>> Engine::setupCoreAffinityConfig(
    const QList<AbstractModule *> &threadedModules)
{
    // prepare pinning threads to CPU cores
    QHash<AbstractModule *, std::vector<uint>> modCPUMap;
    d->mainThreadCoreAffinity.clear();

    auto availableCores = get_online_cores_count() - 1; // all cores minus the one our main thread is running on

    // give modules which explicitly want to be tied to a CPU core their CPU affinity
    // setting, independent of whether the "explicitCoreAffinities" use setting is set
    for (auto &mod : threadedModules) {
        if (!mod->features().testFlag(ModuleFeature::REQUEST_CPU_AFFINITY))
            continue;
        if (availableCores > 0) {
            modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
            availableCores--;
        } else
            break;
    }

    // we are done here if the "explicit core affinities" setting wasn't set by the user
    if (!d->gconf->explicitCoreAffinities())
        return modCPUMap;

    // tie main thread to first CPU by default
    d->mainThreadCoreAffinity.push_back(0);

    // we try to give each thread to a dedicated core, to (ideally) prevent
    // the scheduler from moving them around between CPUs too much once they go idle

    // all modules which explicitly requested an own core already got one, so
    // now give a CPU core to all other modules, unless they explicitly don't
    // want that and override the user's selection
    for (auto &mod : threadedModules) {
        if (mod->features().testFlag(ModuleFeature::PROHIBIT_CPU_AFFINITY))
            continue;
        if (availableCores > 0) {
            modCPUMap[mod] = std::vector<uint>{(uint)availableCores};
            availableCores--;
        } else
            break;
    }

    // give the remaining cores to other modules
    std::vector<uint> remainingCores;
    for (uint i = availableCores; i > 0; i--)
        remainingCores.push_back(i);

    if (!remainingCores.empty()) {
        // give remaining cores to main thread
        // NOTE: A lot of threads & tasks will still fork off the main thread,
        // so this is well-invested
        remainingCores.push_back(0);
        d->mainThreadCoreAffinity = remainingCores;
    }

    return modCPUMap;
}

void Engine::onDiskspaceMonitorEvent()
{
    std::filesystem::space_info ssi;
    try {
        ssi = std::filesystem::space(d->monitoring->exportDirPath.toStdString());
    } catch (const std::filesystem::filesystem_error &e) {
        qCWarning(logEngine).noquote() << "Could not determine remaining free disk space:" << e.what();
        return;
    }

    const double mibAvailable = ssi.available / 1024.0 / 1024.0;
    if (mibAvailable < 8192) {
        Q_EMIT resourceWarningUpdate(
            StorageSpace,
            false,
            QStringLiteral("Disk space is very low. Less than %1 GiB remaining.")
                .arg(mibAvailable / 1024.0, 0, 'f', 1));
        d->monitoring->diskSpaceWarningEmitted = true;
    } else {
        if (d->monitoring->diskSpaceWarningEmitted) {
            Q_EMIT resourceWarningUpdate(
                StorageSpace,
                true,
                QStringLiteral("%1 GiB of disk space remaining.").arg(mibAvailable / 1024.0, 0, 'f', 1));
            d->monitoring->diskSpaceWarningEmitted = false;
        }
    }
}

void Engine::onMemoryMonitorEvent()
{
    const auto memInfo = readMemInfo();

    if (memInfo.memAvailablePercent < d->monitoring->prevMemAvailablePercent && memInfo.memAvailablePercent < 1.6
        && d->monitoring->emergencyOOMStop) {
        qCInfo(logEngine).noquote()
            << "Less than 2% of system memory available and shrinking, commencing emergency stop.";
        receiveModuleError(QStringLiteral(
            "Emergency stop: We are low on system memory, and it is continuing to shrink rapidly.\n"
            "To prevent Syntalos from being killed by the system and loosing data, this run has been stopped.\n"
            "Please check your module setup to ensure modules are able to process incoming data fast enough.\n"
            "Slow connections are currently highlighted in red. Depending on the setup complexity, upgrading the "
            "system may also be a viable solution"));
        d->runFailedReason = QStringLiteral("engine: Emergency stop due to low system memory.");
    } else if (memInfo.memAvailablePercent < 5) {
        // when we have less than 5% memory remaining, there usually still is (slower) swap space available,
        // this is why 5% is relatively low.
        // TODO: Be more clever here in future and check available swap space in advance for this warning?
        Q_EMIT resourceWarningUpdate(
            Memory,
            false,
            QStringLiteral("System memory is low. Only %1% remaining.").arg(memInfo.memAvailablePercent, 0, 'f', 1));
        d->monitoring->memoryWarningEmitted = true;
    } else {
        if (d->monitoring->memoryWarningEmitted) {
            Q_EMIT resourceWarningUpdate(
                Memory,
                true,
                QStringLiteral("%1% of system memory remaining.").arg(memInfo.memAvailablePercent, 0, 'f', 1));
            d->monitoring->memoryWarningEmitted = true;
        }
    }

    d->monitoring->prevMemAvailablePercent = memInfo.memAvailablePercent;
}

void Engine::onBufferMonitorEvent()
{
    bool issueFound = false;
    bool subBufferWarningEmitted = d->monitoring->subBufferWarningEmitted;

    for (auto &msd : d->monitoring->monitoredSubscriptions) {
        const auto approxPendingCount = msd.sub->approxPendingCount();

        // less than 100 pending items is arbitrarily considered "okay"
        if (approxPendingCount < 100) {
            if (msd.heat != ConnectionHeatLevel::NONE) {
                Q_EMIT connectionHeatChangedAtPort(msd.port, ConnectionHeatLevel::NONE);
                msd.heat = ConnectionHeatLevel::NONE;
                qCDebug(logEngine).noquote()
                    << "Connection heat removed from"
                    << QString("%1:%2[◁%3]")
                           .arg(msd.port->owner()->name(), msd.port->title(), msd.port->dataTypeName());
            }
            continue;
        }

        // determine connection "heat" level
        ConnectionHeatLevel heat;
        if (approxPendingCount > 300)
            heat = ConnectionHeatLevel::HIGH;
        else if (approxPendingCount > 200)
            heat = ConnectionHeatLevel::MEDIUM;
        else
            heat = ConnectionHeatLevel::LOW;
        if (heat != msd.heat) {
            msd.heat = heat;
            Q_EMIT connectionHeatChangedAtPort(msd.port, msd.heat);
            qCDebug(logEngine).noquote().nospace()
                << "Connection heat changed to \"" << connectionHeatToHumanString(msd.heat) << "\" for "
                << QString("%1:%2[◁%3]").arg(msd.port->owner()->name(), msd.port->title(), msd.port->dataTypeName())
                << " (level: " << approxPendingCount << ")";
        }

        if (heat > ConnectionHeatLevel::LOW) {
            issueFound = true;
            if (!subBufferWarningEmitted) {
                Q_EMIT resourceWarningUpdate(
                    StreamBuffers,
                    false,
                    QStringLiteral("A module is overwhelmed with its input and not fast enough."));
                subBufferWarningEmitted = true;
            }
        }
    }

    if (!issueFound && subBufferWarningEmitted) {
        Q_EMIT resourceWarningUpdate(
            StreamBuffers, true, QStringLiteral("All modules appear to be running fast enough."));
        subBufferWarningEmitted = false;
    }

    d->monitoring->subBufferWarningEmitted = subBufferWarningEmitted;
}

void Engine::startResourceMonitoring(QList<AbstractModule *> activeModules, const QString &exportDirPath)
{
    // watcher for disk space
    d->monitoring->exportDirPath = exportDirPath;
    d->monitoring->diskSpaceWarningEmitted = false;
    d->monitoring->diskSpaceCheckTimer.setInterval(60 * MS_PER_S); // check every 60sec
    connect(&d->monitoring->diskSpaceCheckTimer, &QTimer::timeout, this, &Engine::onDiskspaceMonitorEvent);

    // watcher for remaining system memory
    d->monitoring->prevMemAvailablePercent = 100;
    d->monitoring->emergencyOOMStop = d->gconf->emergencyOOMStop();
    d->monitoring->memoryWarningEmitted = false;
    d->monitoring->memCheckTimer.setInterval(5 * MS_PER_S); // check every 5sec
    connect(&d->monitoring->memCheckTimer, &QTimer::timeout, this, &Engine::onMemoryMonitorEvent);

    // watcher for subscription buffer
    d->monitoring->monitoredSubscriptions.clear();
    for (auto &mod : activeModules) {
        for (auto &port : mod->inPorts()) {
            if (!port->hasSubscription())
                continue;
            EngineResourceMonitorData::SubscriptionBufferWatchData data;
            data.sub = port->subscriptionVar().get();
            data.port = port.get();
            data.heat = ConnectionHeatLevel::NONE;
            d->monitoring->monitoredSubscriptions.push_back(data);

            // reset all connection heat levels
            Q_EMIT connectionHeatChangedAtPort(port.get(), ConnectionHeatLevel::NONE);
        }
    }

    d->monitoring->subBufferWarningEmitted = false;
    d->monitoring->subBufferCheckTimer.setInterval(5 * MS_PER_S); // check every 5sec
    connect(&d->monitoring->subBufferCheckTimer, &QTimer::timeout, this, &Engine::onBufferMonitorEvent);

    // start resource watchers
    d->monitoring->diskSpaceCheckTimer.start();
    d->monitoring->memCheckTimer.start();
    d->monitoring->subBufferCheckTimer.start();
    qCDebug(logEngine).noquote().nospace() << "Started system resource monitoring.";
}

void Engine::stopResourceMonitoring()
{
    d->monitoring->diskSpaceCheckTimer.stop();
    d->monitoring->diskSpaceCheckTimer.disconnect(this);

    d->monitoring->memCheckTimer.stop();
    d->monitoring->memCheckTimer.disconnect(this);

    d->monitoring->subBufferCheckTimer.stop();
    d->monitoring->subBufferCheckTimer.disconnect(this);

    d->monitoring->monitoredSubscriptions.clear();
    d->monitoring->exportDirPath = QString();

    qCDebug(logEngine).noquote().nospace() << "Stopped monitoring system resources.";
}

bool Engine::finalizeExperimentMetadata(
    std::shared_ptr<EDLCollection> storageCollection,
    qint64 finishTimestamp,
    const QList<AbstractModule *> &activeModules)
{
    emitStatusMessage(QStringLiteral("Writing experiment metadata..."));

    // write collection metadata with information about this experiment
    storageCollection->setTimeCreated(QDateTime::currentDateTime());

    storageCollection->setGeneratorId(
        QStringLiteral("%1 %2").arg(QCoreApplication::applicationName()).arg(syntalosVersionFull()));
    if (d->experimenter.isValid())
        storageCollection->addAuthor(d->experimenter);

    QVariantHash extraData;
    extraData.insert("collection_name", d->exportName);
    extraData.insert("subject_id", d->testSubject.id.isEmpty() ? QVariant() : d->testSubject.id);
    extraData.insert("subject_group", d->testSubject.group.isEmpty() ? QVariant() : d->testSubject.group);
    extraData.insert("subject_comment", d->testSubject.comment.isEmpty() ? QVariant() : d->testSubject.comment);
    extraData.insert("recording_length_msec", finishTimestamp);

    extraData.insert("success", !d->failed);
    if (d->failed && !d->runFailedReason.isEmpty())
        extraData.insert("failure_reason", d->runFailedReason);
    if (!d->failed && !d->pendingErrors.isEmpty()) {
        QString errorsText = QStringLiteral("Run succeeded, but the following errors have been reported:\n");
        for (const auto &errorDetail : d->pendingErrors)
            errorsText.append(QStringLiteral("* %1: %2\n").arg(errorDetail.first->name(), errorDetail.second));
        extraData.insert("error_messages", errorsText);
    }

    extraData.insert(
        "machine_node",
        QStringLiteral("%1 [%2 %3]")
            .arg(d->sysInfo->machineHostName())
            .arg(d->sysInfo->osId())
            .arg(d->sysInfo->osVersion()));

    if (!d->nextRunComment.isEmpty() && !d->runIsEphemeral) {
        // add user comment
        extraData.insert("user_comment", d->nextRunComment);

        // we only remove the comment if the run was a success, as it is very likely
        // the the user will remove the comment and will try again
        if (!d->failed)
            d->nextRunComment.clear();
    }
    // update last run directory
    if (!d->runIsEphemeral)
        d->lastRunExportDir = storageCollection->path();

    QVariantList attrModList;
    for (auto &mod : activeModules) {
        QVariantHash info;
        info.insert(QStringLiteral("id"), mod->id());
        info.insert(QStringLiteral("name"), mod->name());
        attrModList.append(info);
    }
    extraData.insert("modules", attrModList);
    storageCollection->setAttributes(extraData);

    qCDebug(logEngine) << "Saving experiment metadata in:" << storageCollection->path();

    if (!storageCollection->save()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Unable to finish recording"),
            QStringLiteral("Unable to save experiment metadata: %1").arg(storageCollection->lastError()));
        d->failed = true;
    }

    emitStatusMessage(QStringLiteral("Experiment metadata written."));
    return true;
}

/**
 * Ensure all module input ports are resumed, but suspend input to input
 * ports of any disabled modules if @param suspended is set to true.
 *
 * @param suspended Whether to suspend input to disabled modules.
 */
void Engine::setInactiveModuleInputPortsSuspended(bool suspended)
{
    for (auto &mod : d->presentModules) {
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;
            auto sub = iport->subscriptionVar();

            // make sure that the input port is active, to ensure *only* inactive modules have
            // their inputs suspended (this sets all modules to a known state)
            sub->clearPending();
            sub->resume();

            // if we are only activating inputs, not suspending, we can stop here
            if (!suspended)
                continue;

            // suspend inputs on dormant or disabled modules
            if (!mod->modifiers().testFlag(ModuleModifier::ENABLED) || mod->state() == ModuleState::DORMANT)
                sub->suspend();
        }
    }
}

/**
 * @brief Distribute niceness elevation slots among all module threads for a run.
 *
 * RtKit enforces a per-user limit across all elevated threads that we do not know,
 * but guess to be around 16 (it is hardcoded at 25, but other processes have elevated
 * priorities as well).
 * This function pre-assigns which threads get elevation, so that:
 *   a) we never make D-Bus calls that will likely be rejected, and
 *   b) the most important threads - those whose modules appear earliest in the topological
 *      run order - are prioritized across both  in-process dedicated threads and
 *      out-of-process workers.
 *
 * One slot is always reserved for the main engine thread (it is never in the returned set).
 * The remaining slots are distributed in topological order across all module types.
 *
 * @param modOrder          The pre-computed module run order for this run.
 * @param maxRtThreads      Maximum amount of concurrent RT threads.
 * @param defaultThreadNice The configured default thread niceness (0 = no elevation wanted).
 */
void Engine::allocateNicenessBudget(const ModuleRunOrder &modOrder, uint maxRtThreads, int defaultThreadNice)
{
    // Initialize all modules to default priority (0). We then assign elevated
    // niceness to the selected eligible modules later.
    for (auto &mod : modOrder.start)
        mod->setDefaultThreadNiceness(0);

    // If elevation is not wanted at all, we are done.
    if (defaultThreadNice == 0 || maxRtThreads == 0)
        return;

    QList<AbstractModule *> deviceEligibleMods;
    QList<AbstractModule *> otherEligibleMods;
    for (auto &mod : modOrder.start) {
        const bool isMLink = qobject_cast<MLinkModule *>(mod) != nullptr;
        const bool isDedicated = mod->driver() == ModuleDriverKind::THREAD_DEDICATED;
        if (!isMLink && !isDedicated)
            continue;

        const auto info = d->modLibrary->moduleInfo(mod->id());
        const bool isDevice = info && info->categories().testFlag(ModuleCategory::DEVICES);
        if (isDevice)
            deviceEligibleMods.append(mod);
        else
            otherEligibleMods.append(mod);
    }

    if (deviceEligibleMods.isEmpty() && otherEligibleMods.isEmpty())
        return;

    // Reserve 1 slot for the main engine thread (always elevated, starts first).
    // Distribute the remaining slots in topological order across all module types.
    const size_t totalWanting = deviceEligibleMods.length() + otherEligibleMods.length();
    const size_t slotsForModules = std::max(0LL, maxRtThreads - 1LL);

    if (slotsForModules >= totalWanting) {
        for (auto &mod : deviceEligibleMods)
            mod->setDefaultThreadNiceness(defaultThreadNice);
        for (auto &mod : otherEligibleMods)
            mod->setDefaultThreadNiceness(defaultThreadNice);
        return; // everything fits - elevated all eligible modules
    }

    qCWarning(logEngine).noquote().nospace()
        << "Concurrent RT thread limit is " << maxRtThreads << " (1 main + " << totalWanting << " total requested). "
        << "Only the first " << slotsForModules << " module threads will be elevated to nice " << defaultThreadNice
        << "; the remaining " << (totalWanting - slotsForModules) << " will run at default priority. ";

    // Generally walk in topological order, but prioritize DEVICE modules first.
    auto remaining = slotsForModules;
    for (auto &mod : deviceEligibleMods) {
        if (remaining <= 0)
            break;
        mod->setDefaultThreadNiceness(defaultThreadNice);
        --remaining;
    }

    for (auto &mod : otherEligibleMods) {
        if (remaining <= 0)
            break;
        mod->setDefaultThreadNiceness(defaultThreadNice);
        --remaining;
    }
}

/**
 * @brief Actually run an experiment module board
 * @return true on succees
 *
 * This function runs an experiment with the given path,
 * doing *no* error checking on the data export path anymore.
 * It may never be called from anything but internal engine functions.
 */
bool Engine::runInternal(const QString &exportDirPath)
{
    QDir edlDir(exportDirPath);
    if (edlDir.exists()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Internal Error"),
            QStringLiteral(
                "Directory '%1' was expected to be nonexistent, but the directory exists. "
                "Stopped run to prevent potential data loss. This condition should never happen.")
                .arg(exportDirPath));
        return false;
    }

    if (!makeDirectory(exportDirPath))
        return false;

    // ensure error queue is clean
    d->pendingErrors.clear();

    // the engine is actively doing stuff with modules now
    d->active = true;
    d->usbEventsTimer->stop();

    // reset failure reason, in case one was set from a previous run
    d->runFailedReason = QString();

    // tell listeners that we are preparing a run
    emit preRunStart();

    // cache default thread RT and niceness values
    const auto defaultThreadNice = d->gconf->defaultThreadNice();
    const auto defaultRTPriority = d->gconf->defaultRTThreadPriority();

    // cache number of online CPUs
    const auto cpuCoreCount = get_online_cores_count();

    // set main thread default niceness for the current run
    setCurrentThreadNiceness(defaultThreadNice);

    // set CPU core affinities base setting
    if (d->gconf->explicitCoreAffinities())
        qCDebug(logEngine).noquote().nospace() << "Explicit CPU core affinity is enabled.";
    else
        qCDebug(logEngine).noquote().nospace() << "Explicit CPU core affinity is disabled.";

    // create new experiment directory layout (EDL) collection to store
    // all data modules generate in
    auto storageCollection = std::make_shared<EDLCollection>(d->exportName);
    storageCollection->setPath(exportDirPath);

    // if we should save internal diagnostic data, create a group for it!
    if (d->saveInternal) {
        d->edlInternalData = std::make_shared<EDLGroup>();
        d->edlInternalData->setName("syntalos_internal");
        storageCollection->addChild(d->edlInternalData);
        qCDebug(logEngine).noquote().nospace() << "Writing some internal data to datasets for debugging and analysis";
    }
    d->internalTSyncWriters.clear();

    // compute start and stop orders for all active modules
    const auto modOrder = createModuleRunOrder();

    // create a new master timer for synchronization
    d->timer.reset(new SyncTimer);

    auto lastPhaseTimepoint = currentTimePoint();
    // assume success until a module actually fails
    bool initSuccessful = true;
    d->failed = false;

    // perform module name sanity check
    {
        QSet<QString> modNameSet;
        for (auto &mod : modOrder.start) {
            const auto expectedName = simplifyStrForModuleName(mod->name());
            if (mod->name() != expectedName) {
                qCWarning(logEngine).noquote()
                    << "Module" << mod->name() << "has invalid name. Expected:" << expectedName
                    << "(The module has been renamed)";
                mod->setName(expectedName);
            }

            const auto uniqName = simplifyStrForFileBasenameLower(mod->name());
            if (modNameSet.contains(uniqName)) {
                QMessageBox::critical(
                    d->parentWidget,
                    QStringLiteral("Can not run this board"),
                    QStringLiteral(
                        "A module with the name '%1' exists twice in this board, or another module has a "
                        "very similar name. "
                        "Please give the duplicate a unique name in order to execute this board.")
                        .arg(mod->name()));
                d->active = false;
                d->failed = true;
                d->usbEventsTimer->start();
                return false;
            }
            modNameSet.insert(uniqName);
        }
    }

    // prevent the system from sleeping or shutdown
    const int sleepInhibitorLockFd = obtainSleepShutdownIdleInhibitor();
    if (sleepInhibitorLockFd < 0)
        qCWarning(logEngine).noquote() << "Could not inhibit system sleep/idle/shutdown for this run.";
    else
        qCDebug(logEngine).noquote() << "Obtained system sleep/idle/shutdown inhibitor for this run.";

    // the dedicated threads our modules run in, references owned by the vector
    std::vector<std::unique_ptr<SyThread>> dThreads;
    QList<AbstractModule *> threadedModules;

    // special event threads and their assigned modules, with a specific identifier string as hash key
    QHash<QString, QList<AbstractModule *>> eventModules;
    QHash<QString, std::shared_ptr<ModuleEventThread>> evThreads;

    // filter out dedicated-thread modules, those get special treatment
    for (auto &mod : modOrder.start) {
        mod->setDefaultRTPriority(defaultRTPriority);
        if (mod->driver() == ModuleDriverKind::THREAD_DEDICATED)
            threadedModules.append(mod);
    }
    const auto threadedModulesTotalN = threadedModules.size();

    // give modules a hint as to how many CPU cores they themselves may use additionally
    uint potentialNoaffinityCPUCount = 0;
    if (threadedModulesTotalN <= (cpuCoreCount - 1))
        potentialNoaffinityCPUCount = cpuCoreCount - threadedModulesTotalN - 1;
    qCDebug(logEngine).noquote() << "Predicted amount of CPU cores with no (explicitly known) occupation:"
                                 << potentialNoaffinityCPUCount;
    for (auto &mod : modOrder.start)
        mod->setPotentialNoaffinityCPUCount(potentialNoaffinityCPUCount);

    qApp->processEvents();

    // distribute niceness slots across all module threads before preparing any module
    allocateNicenessBudget(modOrder, d->gconf->defaultRtKitThreadsMax(), defaultThreadNice);

    // prepare modules
    for (auto &mod : modOrder.start) {
        // Prepare module. At this point it should have a timer,
        // the location where data is saved and be in the PREPARING state.
        emitStatusMessage(QStringLiteral("Preparing '%1'...").arg(mod->name()));
        lastPhaseTimepoint = currentTimePoint();

        const auto modInfo = d->modLibrary->moduleInfo(mod->id());

        mod->setStatusMessage(QString());
        mod->setTimer(d->timer);
        mod->setState(ModuleState::PREPARING);
        mod->setEphemeralRun(d->runIsEphemeral);

        mod->setSimpleStorageNames(d->simpleStorageNames);
        if ((modInfo != nullptr) && (!modInfo->storageGroupName().isEmpty())) {
            auto storageGroup = storageCollection->groupByName(
                modInfo->storageGroupName(), EDLCreateFlag::CREATE_OR_OPEN);
            if (storageGroup == nullptr) {
                qCCritical(logEngine) << "Unable to create data storage group with name" << modInfo->storageGroupName();
                mod->setStorageGroup(storageCollection);
            } else {
                mod->setStorageGroup(storageGroup);
            }
        } else {
            mod->setStorageGroup(storageCollection);
        }

        if (!mod->prepare(d->testSubject)) {
            initSuccessful = false;
            d->failed = true;
            d->runFailedReason = QStringLiteral("Prepare step failed for: %1(%2)").arg(mod->id(), mod->name());
            emitStatusMessage(QStringLiteral("Module '%1' failed to prepare.").arg(mod->name()));
            break;
        }
        // If the module hasn't set itself to ready yet and is idle or preparing,
        // assume it is actually ready. Otherwise flag it as dormant.
        if (mod->state() == ModuleState::IDLE || mod->state() == ModuleState::PREPARING)
            mod->setState(ModuleState::READY);
        else if (mod->state() != ModuleState::READY)
            mod->setState(ModuleState::DORMANT);

        qCDebug(logEngine).noquote().nospace()
            << "Module '" << mod->name() << "' prepared in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // mark all inactive modules as dormant
    for (auto &mod : modOrder.inactive) {
        mod->setState(ModuleState::DORMANT);
    }

    // suspend input to all inactive modules
    setInactiveModuleInputPortsSuspended(true);

    // exporter for streams so out-of-process mlink modules can access them
    auto streamExporter = std::make_unique<StreamExporter>();
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Exporting streams for external modules..."));
        for (auto &mod : modOrder.start) {
            auto mlinkMod = qobject_cast<MLinkModule *>(mod);
            if (mlinkMod != nullptr)
                mlinkMod->markIncomingForExport(streamExporter.get());
        }
    }

    // wait condition for all threads to block them until we have actually started (or not block them, in case
    // the thread was really slow to initialize and we are already running)
    std::unique_ptr<OptionalWaitCondition> startWaitCondition(new OptionalWaitCondition());

    // Only actually launch if preparation didn't fail.
    // we still call stop() on all modules afterwards though,
    // as some might need a stop call to clean up resources ther were
    // set up during preparations.
    // Modules are expected to deal with multiple calls to stop().
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Initializing launch..."));
        lastPhaseTimepoint = currentTimePoint();

        // create CPU core affinity configuration, and apply it to the main thread if feasible
        const auto modCPUMap = setupCoreAffinityConfig(threadedModules);

        // only emit a resource warning if we are using way more threads than we probably should
        if (threadedModulesTotalN > (cpuCoreCount + (cpuCoreCount / 2)))
            Q_EMIT resourceWarningUpdate(
                CpuCores, false, QStringLiteral("Likely not enough CPU cores available for optimal operation."));

        // launch threads for threaded modules, except for out out-of-process
        // modules - they get special treatment
        for (int i = 0; i < threadedModules.size(); i++) {
            auto mod = threadedModules[i];

            // we are preparing again, this time for threading!
            // this is important, as we will only start when the module
            // signalled that it is ready now.
            mod->setState(ModuleState::PREPARING);

            ThreadDetails td;
            td.niceness = mod->defaultThreadNiceness();
            td.allowedRTPriority = defaultRTPriority;

            if (modCPUMap.contains(mod)) {
                td.cpuAffinity = modCPUMap[mod];
                std::ostringstream oss;
                std::copy(td.cpuAffinity.begin(), td.cpuAffinity.end() - 1, std::ostream_iterator<uint>(oss, ","));
                oss << td.cpuAffinity.back();

                qCDebug(logEngine).noquote().nospace()
                    << "Module '" << mod->name() << "' thread will prefer CPU core(s) "
                    << QString::fromStdString(oss.str());
            }

            // the thread name shouldn't be longer than 16 chars (inlcuding NULL)
            td.name = QStringLiteral("%1-%2").arg(QStringView{mod->id()}.mid(0, 12)).arg(i);

            // We create the dedicated module threads as QThread, as it is a bit more convenient and
            // sometimes efficients for modules to have access to an Qt event loop.
            // (SyThread can either be a pure pthread, or a QThread)
            auto modThread = std::make_unique<SyThread>(td, mod, startWaitCondition.get());
            dThreads.push_back(std::move(modThread));
        }
        assert(dThreads.size() == (size_t)threadedModules.size());

        // collect all modules which do some kind of event-based execution
        {
            QHash<QString, int> remainingEvModCountById;
            for (auto &mod : modOrder.start) {
                if ((mod->driver() == ModuleDriverKind::EVENTS_SHARED)
                    || (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED)) {
                    if (!remainingEvModCountById.contains(mod->id()))
                        remainingEvModCountById[mod->id()] = 0;
                    remainingEvModCountById[mod->id()] += 1;
                }
            }

            // assign modules to their threads and give the groups an ID
            for (auto &mod : modOrder.start) {
                QString evGroupId;
                if (mod->driver() == ModuleDriverKind::EVENTS_SHARED) {
                    evGroupId = QStringLiteral("shared_0");
                } else if (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED) {
                    if (mod->eventsMaxModulesPerThread() <= 0) {
                        evGroupId = QStringLiteral("m:%1").arg(mod->id());
                        remainingEvModCountById[mod->id()] -= 1;
                    } else {
                        // reduce number first to get "0 / eventsMaxModulesPerThread" last
                        remainingEvModCountById[mod->id()] -= 1;
                        int evGroupPerModIdx = static_cast<int>(
                            remainingEvModCountById[mod->id()] / mod->eventsMaxModulesPerThread());
                        evGroupId = QStringLiteral("m:%1_%2").arg(mod->id(), evGroupPerModIdx);
                    }
                } else {
                    // not an event-driven module
                    continue;
                }

                // add module to hash map
                if (!eventModules.contains(evGroupId))
                    eventModules[evGroupId] = QList<AbstractModule *>();
                eventModules[evGroupId].append(mod);
            }
        }

        // give modules which roll their own threads their own start-wait-conditions at this point
        // (right before the PREPARE stage is reached)
        for (auto &mod : modOrder.start)
            mod->updateStartWaitCondition(startWaitCondition.get());

        // run special threads with built-in event loops for modules that selected an event-based driver
        for (auto it = eventModules.constBegin(); it != eventModules.constEnd(); ++it) {
            const auto &evThreadKey = it.key();

            std::shared_ptr<ModuleEventThread> evThread(new ModuleEventThread(evThreadKey));
            evThread->run(eventModules[evThreadKey], startWaitCondition.get());
            evThreads[evThreadKey] = evThread;
            qCDebug(logEngine).noquote().nospace() << "Started event thread '" << evThreadKey << "' with "
                                                   << eventModules[evThreadKey].length() << " participating modules";
        }

        qCDebug(logEngine).noquote().nospace()
            << "Module and engine threads created in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
        lastPhaseTimepoint = currentTimePoint();

        // ensure all modules are in the READY state
        // (modules may take a bit of time to prepare their threads)
        // FIXME: Maybe add a timeout on this, in case a module doesn't
        // behave and never ever leaves its preparation phase?
        for (auto &mod : modOrder.start) {
            if (mod->state() == ModuleState::READY)
                continue;
            // DORMANT is also a valid state at this point, the module may not
            // have had additional setup to do
            if (mod->state() == ModuleState::DORMANT)
                continue;
            emitStatusMessage(QStringLiteral("Waiting for '%1' to get ready...").arg(mod->name()));
            while (mod->state() != ModuleState::READY) {
                QThread::msleep(250);
                qApp->processEvents();
                if (mod->state() == ModuleState::ERROR) {
                    emitStatusMessage(QStringLiteral("Module '%1' failed to initialize.").arg(mod->name()));
                    initSuccessful = false;
                    break;
                }
                if (d->failed) {
                    // we failed elsewhere
                    initSuccessful = false;
                    break;
                }
            }
        }

        qCDebug(logEngine).noquote().nospace()
            << "Waited for modules to get ready for " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // Meanwhile, threaded modules may have failed, so let's check again if we are still
    // good on initialization
    if (initSuccessful) {
        emitStatusMessage(QStringLiteral("Launch setup completed."));

        // collect modules which have an explicit UI callback method
        std::vector<AbstractModule *> callUiEventModules;
        for (auto &mod : modOrder.start) {
            if (mod->features().testFlag(ModuleFeature::CALL_UI_EVENTS)) {
                // we do not call UI events for modules that have marked themselves as dormant
                if (mod->state() != ModuleState::DORMANT)
                    callUiEventModules.push_back(mod);
            }
        }

        // start monitoring resource issues during this run
        startResourceMonitoring(modOrder.start, exportDirPath);

        // we officially start now, launch the timer
        d->timer->start();
        d->running = true;

        // first, launch all threaded and evented modules
        for (auto &mod : modOrder.start) {
            if ((mod->driver() != ModuleDriverKind::THREAD_DEDICATED)
                && (mod->driver() != ModuleDriverKind::EVENTS_DEDICATED)
                && (mod->driver() != ModuleDriverKind::EVENTS_SHARED))
                continue;

            if (mod->state() != ModuleState::DORMANT)
                mod->start();

            // ensure modules are in their "running" state now, or
            // have themselves declared "dormant" (meaning they won't be used at all)
            if (mod->state() != ModuleState::ERROR) {
                mod->m_running = true;
                if (mod->state() != ModuleState::DORMANT)
                    mod->setState(ModuleState::RUNNING);
            }
        }

        // make stream exporter resume its work
        streamExporter->run(startWaitCondition.get());

        // wake that thundering herd and hope all threaded modules awoken by the
        // start signal behave properly
        // (Threads *must* only be unlocked after we've sent start() to the modules, as they
        // may prepare stuff in start() that the threads need, like timestamp syncs)
        startWaitCondition->wakeAll();

        qCDebug(logEngine).noquote().nospace()
            << "Threaded/evented module startup completed, took " << d->timer->timeSinceStartMsec().count() << "msec";
        lastPhaseTimepoint = d->timer->currentTimePoint();

        // tell all non-threaded modules individually now that we started
        for (auto &mod : modOrder.start) {
            // ignore threaded & evented
            if ((mod->driver() == ModuleDriverKind::THREAD_DEDICATED)
                || (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED)
                || (mod->driver() == ModuleDriverKind::EVENTS_SHARED))
                continue;

            if (mod->state() != ModuleState::DORMANT)
                mod->start();

            // work around bad modules which don't set this on their own in start()
            if (mod->state() != ModuleState::ERROR) {
                mod->m_running = true;
                if (mod->state() != ModuleState::DORMANT)
                    mod->setState(ModuleState::RUNNING);
            }
        }

        qCDebug(logEngine).noquote().nospace() << "Startup phase completed, all modules are running. Took additional "
                                               << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";

        // tell listeners that we are running now
        emit runStarted();

        emitStatusMessage(QStringLiteral("Running..."));
        qApp->processEvents();

        // apply main thread core affinity now
        // (this must not be done earlier, as otherwise external module threads may inherit
        //  the main thread's affinity settings)
        if (!d->mainThreadCoreAffinity.empty())
            thread_set_affinity_from_vec(pthread_self(), d->mainThreadCoreAffinity);

        // run the main loop and process UI events
        // modules may have injected themselves into the UI event loop
        // as well via QTimer callbacks, in case they need to modify UI elements.
        while (d->running) {
            // process modules which want to be explicitly called to process UI events
            for (auto &mod : callUiEventModules)
                mod->processUiEvents();

            // process application GUI events
            qApp->processEvents(QEventLoop::WaitForMoreEvents);

            // bail in case something has failed
            if (d->failed)
                break;
        }

        // stop resource watcher timers
        stopResourceMonitoring();
    }

    auto finishTimestamp = static_cast<long long>(d->timer->timeSinceStartMsec().count());
    emitStatusMessage(QStringLiteral("Run stopped, finalizing..."));

    // clear any thread affinity of the main process, so anything the stop() actions
    // of modules do isn't confined to the main UI threads
    thread_clear_affinity(pthread_self());

    // Wake all threads again if we have failed, because some module may have
    // failed so early that other modules may not even have made it through their
    // startup phase, and in this case are stuck waiting.
    // We wake threads again later shortly before joining them (just in case),
    // so you may thing this early wakeup call isn't necessary.
    // Some modules though may actually wait for the thread to go down first
    // (by setting m_running to false) and wait on that event in their stop() function.
    // And this won't ever happen in case the thread is still idling on the start wait condition.
    // So we set every module that has its own thread to "not running" and then ring the wakeup bell
    if (d->failed) {
        for (auto &mod : threadedModules)
            mod->m_running = false;
        startWaitCondition->wakeAll();
    }

    // join all threads running evented modules, therefore stop
    // processing any new events
    lastPhaseTimepoint = d->timer->currentTimePoint();
    for (const auto &evThread : evThreads.values()) {
        emitStatusMessage(QStringLiteral("Waiting for event thread `%1`...").arg(evThread->threadName()));
        evThread->stop();
    }
    qCDebug(logEngine).noquote().nospace()
        << "Waited " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec for event threads to stop.";

    // send stop command to all active modules in their designated stop order
    for (auto &mod : modOrder.stop) {
        emitStatusMessage(QStringLiteral("Stopping '%1'...").arg(mod->name()));
        lastPhaseTimepoint = d->timer->currentTimePoint();

        // wait a little bit for modules to process remaining data from their
        // stream subscriptions - we don't wait too long here, simply because
        // the upstream module may still be generating data (and in that case
        // we would never be able to stop, especially if there are cycles in
        // the module graph
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;

            // give the module 1.2sec to clear pending elements for this subscription
            const auto startPortWaitTS = d->timer->timeSinceStartMsec();
            size_t remainingElements = 0;
            do {
                remainingElements = iport->subscriptionVar()->approxPendingCount();
                // sometimes the approx. pending count is never 0, so we accept loosing one element in any case
                // (in many cases, the last entity will be a nullopt anyway, so we can ignore it)
                if (remainingElements <= 1)
                    break;
                qApp->processEvents();
            } while ((d->timer->timeSinceStartMsec() - startPortWaitTS).count() <= 1600);

            if (remainingElements > 1)
                qCDebug(logEngine).noquote().nospace() << "Module '" << mod->name() << "' "
                                                       << "subscription `" << iport->id() << "` "
                                                       << "possibly lost " << remainingElements << " element(s)";

            // drop all remaining elements to save some memory when idle
            iport->subscriptionVar()->clearPending();
        }

        // send the stop command
        mod->stop();
        QCoreApplication::processEvents();

        // safeguard against bad modules which don't stop running their
        // thread loops on their own
        mod->m_running = false;

        // ensure modules really have terminated all their outgoing streams,
        // because if they didn't do that, connected modules may not be able to exit
        for (auto const &port : mod->outPorts())
            port->stopStream();

        // ensure modules display the correct state after we stopped a run
        if (mod->state() != ModuleState::IDLE && mod->state() != ModuleState::ERROR)
            mod->setState(ModuleState::IDLE);

        qCDebug(logEngine).noquote().nospace()
            << "Module '" << mod->name() << "' stopped in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // Wake up all threads again, just in case one is still stuck waiting
    // for the start condition.
    // This can happen to module threads, as well as to the stream exporter that we
    // are about to stop next.
    startWaitCondition->wakeAll();

    // stop exporting streams to external modules
    emitStatusMessage(QStringLiteral("Stopping IPC stream exporter..."));
    streamExporter->stop();
    streamExporter.reset();

    lastPhaseTimepoint = d->timer->currentTimePoint();

    // join all dedicated module threads with the main thread again, waiting for them to terminate
    emitStatusMessage(QStringLiteral("Joining remaining threads."));
    for (size_t i = 0; i < dThreads.size(); i++) {
        auto &thread = dThreads[i];
        auto mod = threadedModules[i];
        emitStatusMessage(QStringLiteral("Waiting for '%1'...").arg(mod->name()));
        qApp->processEvents();

        // Some modules may be stuck in their threads, so we try our absolute best to not
        // make Syntalos hang while failing to join a misbehaving thread.
        // Ultimately though we must join the thread, so we will just give up eventually.

        // wait ~20sec for the thread to join
        bool threadJoined = false;
        for (int tc = 0; tc < 20; tc++) {
            qApp->processEvents();
            threadJoined = thread->joinTimeout(1);
            if (threadJoined)
                break;
        }
        if (threadJoined)
            continue;

        // if we are here, we failed to join the thread
        qCWarning(logEngine).noquote() << "Failed to join thread for" << mod->name()
                                       << "in time, trying to break deadlock...";
        emitStatusMessage(QStringLiteral("Waiting for '%1' (⚠️ possibly dead / unrecoverable)...").arg(mod->name()));
        qApp->processEvents();

        // let's try to send its inputs a nullopt
        for (auto inPort : mod->inPorts()) {
            if (!inPort->hasSubscription())
                continue;
            auto sub = inPort->subscriptionVar();
            if (!sub->hasPending())
                sub->forcePushNullopt();
        }

        if (!thread->joinTimeout(15)) {
            qCCritical(logEngine).noquote().nospace()
                << "Failed to join thread for " << mod->name() << ". Application may deadlock now.";

            // perform emergency save of metadata
            const auto stallErrorMsg = QStringLiteral(
                                           "During this run, \"%1\" stalled on shutdown and all attempts to recover "
                                           "and quit gracefully failed.")
                                           .arg(mod->name());
            if (d->failed && !d->runFailedReason.isEmpty()) {
                d->runFailedReason += QStringLiteral("\n") + stallErrorMsg;
            } else {
                d->failed = true;
                d->runFailedReason = stallErrorMsg;
            }

            if (initSuccessful)
                finalizeExperimentMetadata(storageCollection, finishTimestamp, modOrder.start);

            emitStatusMessage(QStringLiteral("Unable to recover stalled module '%1' (⚠️ application may deadlock now)")
                                  .arg(mod->name()));
            QMessageBox::critical(
                d->parentWidget,
                QStringLiteral("Critical failure"),
                QStringLiteral(
                    "While stopping this experiment, module \"%1\" stalled and any attempts to wake it up "
                    "and return the application to a safe state have failed.\n"
                    "We tried to save all experiment metadata in emergency mode, so with luck no data "
                    "should have been lost.\n"
                    "If this happens again, please check your hardware for defects and if there are no "
                    "issues, consider filing a bug against Syntalos so we can look into this issue.\n"
                    "We will still try to enter a safe state, but this will likely fail and the application "
                    "may lock up now and might need to be terminated forcefully by you.")
                    .arg(mod->name()));
        }

        qApp->processEvents();
        thread->join();
    }

    qCDebug(logEngine).noquote().nospace()
        << "All (non-event) engine threads joined in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    lastPhaseTimepoint = d->timer->currentTimePoint();

    // All module data must be written by this point, so we "steal" its storage group,
    // so the module will trigger an error message if is still tries to access the final
    // data. We mast do this in a separate loop, as some modules may share an EDL group
    // to store data in.
    for (auto &mod : modOrder.start)
        mod->setStorageGroup(nullptr);

    if (d->saveInternal) {
        emitStatusMessage(QStringLiteral("Finalizing internal dataset..."));
        for (auto &tsw : d->internalTSyncWriters.values())
            tsw->close();
    }

    if (!initSuccessful) {
        // if we failed to prepare this run, don't save the manifest and also
        // remove any data that we might have already created, as well as the
        // export directory.
        emitStatusMessage(QStringLiteral("Removing broken data..."));
        edlDir.removeRecursively();
    } else {
        finalizeExperimentMetadata(storageCollection, finishTimestamp, modOrder.start);
        qCDebug(logEngine).noquote().nospace()
            << "Manifest and additional data saved in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // unsuspend input ports on modules that were inactive this run
    setInactiveModuleInputPortsSuspended(false);

    // mark inactive modules as idle again
    for (auto &mod : modOrder.inactive) {
        if (mod->state() != ModuleState::IDLE)
            mod->setState(ModuleState::IDLE);
    }

    // release system sleep inhibitor lock
    if (sleepInhibitorLockFd != -1)
        ::close(sleepInhibitorLockFd);

    // reset main thread niceness, we are not important anymore of no experiment is running
    setCurrentThreadNiceness(0);

    // ensure main thread CPU affinity is cleared
    thread_clear_affinity(pthread_self());

    // it is safe now to send any error message that we previously deferred
    // to external, possibly blocking, error handlers
    for (const auto &errorDetail : d->pendingErrors)
        emit runFailed(errorDetail.first, errorDetail.second);
    d->pendingErrors.clear();

    // increase counter of successful runs
    if (!d->failed)
        d->runCount++;

    // we have stopped doing things with modules
    d->active = false;

    // notify modules about any deferred USB events again
    d->usbEventsTimer->start();

#ifdef SY_HAS_LSAN
    // Trigger an explicit LSan report now that all module threads are joined and
    // all data structures have been cleaned up.  Running this here (rather than
    // relying on the at-exit check) surfaces inter-run leaks before Python / Qt
    // global state pollution makes them impossible to distinguish.
    __lsan_do_recoverable_leak_check();
#endif

    // tell listeners that we are stopped now
    emit runStopped();

    emitStatusMessage(QStringLiteral("Ready."));
    return true;
}

void Engine::stop()
{
    d->running = false;
}

void Engine::receiveModuleError(const QString &message)
{
    auto mod = qobject_cast<AbstractModule *>(sender());
    mod->setState(ModuleState::ERROR);
    bool stopOnFailure = true;
    if (mod != nullptr) {
        d->runFailedReason = QStringLiteral("%1(%2): %3").arg(mod->id(), mod->name(), message);
        stopOnFailure = mod->modifiers().testFlag(ModuleModifier::STOP_ON_FAILURE);
    } else {
        d->runFailedReason = QStringLiteral("?(?): %1").arg(message);
    }

    const bool wasRunning = d->running;
    if (stopOnFailure) {
        d->failed = true;
        d->running = false;
    }

    // If we were running, defer message emission: We first need to terminate all modules
    // to ensure that a modal error message (which may be created when this event is received)
    // doesn't lock up the main thread and any other module's drawing buffers run full.
    // If we are not supposed to stop on this module failing, we also just add the error
    // to the pending errors list.
    if (wasRunning || !stopOnFailure)
        d->pendingErrors.append(qMakePair(mod, message));
    else
        emit runFailed(mod, message);
}

void Engine::onSynchronizerDetailsChanged(const std::string &id, const TimeSyncStrategies &, const microseconds_t &)
{
    if (!d->saveInternal)
        return;
    if (d->internalTSyncWriters.value(id).get() != nullptr)
        return;
    QString modId;
    auto mod = qobject_cast<AbstractModule *>(sender());
    if (mod != nullptr)
        modId = mod->id();

    auto ds = std::make_shared<EDLDataset>();
    ds->setName(QStringLiteral("%1-%2").arg(modId, qstr(id)));
    d->edlInternalData->addChild(ds);

    auto tsw = std::make_shared<TimeSyncFileWriter>();
    tsw->setFileName(ds->setDataFile("offsets.tsync").toStdString());
    tsw->setTimeUnits(TSyncFileTimeUnit::MICROSECONDS, TSyncFileTimeUnit::MICROSECONDS);
    tsw->setTimeDataTypes(TSyncFileDataType::INT64, TSyncFileDataType::INT64);
    tsw->setTimeNames("approx-master-time", "sync-offset");
    tsw->open(
        QStringLiteral("SyntalosInternal::%1_%2").arg(modId, mod->name().simplified()).toStdString(),
        ds->collectionId());
    d->internalTSyncWriters[id] = tsw;
}

void Engine::onSynchronizerOffsetChanged(const std::string &id, const microseconds_t &currentOffset)
{
    if (!d->saveInternal)
        return;

    auto tsw = d->internalTSyncWriters.value(id);
    if (tsw == nullptr)
        return;

    tsw->writeTimes(d->timer->timeSinceStartUsec(), currentOffset);
}
