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
#include <filesystem>
#include <libusb.h>
#include <pthread.h>
#include <sched.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <iceoryx_posh/runtime/posh_runtime.hpp>
#include <iceoryx_hoofs/error_handling/error_handling.hpp>

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
private:
public:
    explicit SyThread(ThreadDetails &details, AbstractModule *module, OptionalWaitCondition *waitCondition)
        : m_created(false),
          m_joined(false),
          m_td(details),
          m_mod(module),
          m_waitCond(waitCondition)
    {
        auto r = pthread_create(&m_thread, nullptr, &SyThread::executeModuleThread, this);
        if (r != 0)
            throw std::runtime_error{strerror(r)};
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
        if (m_joined)
            return;
        auto r = pthread_join(m_thread, nullptr);
        if (r != 0)
            throw std::runtime_error{strerror(r)};
        m_joined = true;
    }

    bool joinTimeout(uint seconds)
    {
        struct timespec ts;

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
            qCCritical(logEngine).noquote() << "Unable to obtain absolute time since epoch!";
            join();
            return true;
        }

        ts.tv_sec += seconds;
        if (pthread_timedjoin_np(m_thread, nullptr, &ts) == 0) {
            m_joined = true;
            return true;
        }

        return false;
    }

    pthread_t handle() const
    {
        return m_thread;
    }

private:
    bool m_created;
    pthread_t m_thread;
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
          roudiPidFd(-1),
          monitoring(new EngineResourceMonitorData)
    {
    }
    ~Private() {}

    bool initialized;
    SysInfo *sysInfo;
    GlobalConfig *gconf;
    QWidget *parentWidget;
    QList<AbstractModule *> activeModules;
    ModuleLibrary *modLibrary;
    std::shared_ptr<SyncTimer> timer;
    std::vector<uint> mainThreadCoreAffinity;
    int roudiPidFd;

    QString exportBaseDir;
    QString exportDir;
    bool exportDirIsTempDir;
    bool exportDirIsValid;

    EDLAuthor experimenter;
    TestSubject testSubject;
    QString experimentIdTmpl;
    QString experimentIdFinal;
    bool simpleStorageNames;

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
    QHash<QString, std::shared_ptr<TimeSyncFileWriter>> internalTSyncWriters;

    QScopedPointer<EngineResourceMonitorData> monitoring;
    int runCount;
    int runCountPadding;

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
    d->active = false;
    d->running = false;
    d->simpleStorageNames = true;
    d->modLibrary = new ModuleLibrary(d->gconf, this);
    d->parentWidget = parentWidget;
    d->timer.reset(new SyncTimer);
    d->runIsEphemeral = false;
    d->mainThreadCoreAffinity.clear();
    d->runCount = 0;
    d->runCountPadding = 1;
    d->monitoring->emergencyOOMStop = d->gconf->emergencyOOMStop();

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
        nullptr,
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
        connect(d->usbEventsTimer, &QTimer::timeout, [=]() {
            struct timeval tv {
                0, 0
            };
            libusb_handle_events_timeout_completed(nullptr, &tv, nullptr);
        });
        d->usbEventsTimer->start();
    }
}

Engine::~Engine()
{
    delete d->usbEventsTimer;
    if (d->usbHotplugCBHandle != -1)
        libusb_hotplug_deregister_callback(nullptr, d->usbHotplugCBHandle);

    iox::runtime::PoshRuntime::getInstance().shutdown();

    if (d->roudiPidFd > 0)
        close(d->roudiPidFd);
}

/**
 * @brief Launch a program in a new process, and return the PID of the new process as pidfd.
 *
 * This function is used to launch a new program in a new process, and return the PID as pidfd.
 *
 * @param exePath The path to the executable to launch.
 * @param pidfd_out A pointer to an integer, which will be set to the PID of the new process.
 * @return true if the program was successfully launched, false otherwise.
 */
static bool launchProgram(const QString &exePath, int *pidfd_out)
{
    struct clone_args cl_args = {0};
    int pidfd;
    pid_t parent_tid = -1;

    cl_args.parent_tid = __u64((uintptr_t)&parent_tid);
    cl_args.pidfd = __u64((uintptr_t)&pidfd);
    cl_args.flags = CLONE_PIDFD | CLONE_PARENT_SETTID;
    cl_args.exit_signal = SIGCHLD;

    char *const argv[] = {const_cast<char *>(qPrintable(exePath)), nullptr};

    const auto pid = (pid_t)syscall(SYS_clone3, &cl_args, sizeof(cl_args));
    if (pid < 0)
        return false;

    if (pid == 0) { // Child process
        execvp(qPrintable(exePath), argv);
        perror("execvp"); // execvp only returns on error
        exit(EXIT_FAILURE);
    }

    if (pidfd_out)
        *pidfd_out = pidfd;

    return true;
}

/**
 * @brief Check if a process is still running using a pidfd.
 *
 * This function checks if a process is still running, by checking if the process
 * has exited or not.
 *
 * @param pidfd The PIDFD of the process to check.
 * @return true if the process is still running, false otherwise.
 */
static bool isProcessRunning(int pidfd)
{
    siginfo_t si;
    int result;

    si.si_pid = 0;

    result = waitid(P_PIDFD, pidfd, &si, WEXITED | WNOHANG);
    if (result == -1) {
        return false; // Assuming failure means the process is not running
    }

    return si.si_pid == 0;
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

    if (!ensureRoudi())
        return false;

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

    if (dataDir.isEmpty())
        return;

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
        for (auto &emod : d->activeModules) {
            if (emod->id() == id)
                return nullptr;
        }
    }

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

    d->activeModules.append(mod);
    emit moduleCreated(modInfo.get(), mod);

    // the module has been created and registered, we can
    // safely initialize it now.
    mod->setState(ModuleState::INITIALIZING);
    QCoreApplication::processEvents();
    if (!mod->initialize()) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("Module initialization failed"),
            QStringLiteral("Failed to initialize module '%1', it can not be added. %2")
                .arg(mod->id(), mod->lastError()),
            QMessageBox::Ok);
        removeModule(mod);
        return nullptr;
    }

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
    return mod;
}

bool Engine::removeModule(AbstractModule *mod)
{
    auto id = mod->id();
    if (d->activeModules.removeOne(mod)) {
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

    foreach (auto mod, d->activeModules)
        removeModule(mod);
}

QList<AbstractModule *> Engine::activeModules() const
{
    return d->activeModules;
}

AbstractModule *Engine::moduleByName(const QString &name) const
{
    // FIXME: In case we grow projects with huge module counts, we
    // will actually want a QHash index of modules here, so speed up
    // this function (which may otherwise slow down project loading times)
    for (const auto &mod : d->activeModules) {
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

    makeFinalExperimentId();
    d->exportDir = QDir::cleanPath(QStringLiteral("%1/%2/%3/%4")
                                       .arg(d->exportBaseDir)
                                       .arg(d->testSubject.id.trimmed())
                                       .arg(currentDate)
                                       .arg(d->experimentIdFinal.trimmed()));
}

void Engine::emitStatusMessage(const QString &message)
{
    qCDebug(logEngine).noquote() << message;
    emit statusMessage(message);
}

bool Syntalos::Engine::ensureRoudi()
{
    // find RouDi so communication with module processes works
    auto roudiBinary = QStringLiteral("%1/roudi/syntalos-roudi").arg(QCoreApplication::applicationDirPath());
    QFileInfo checkBin(roudiBinary);
    if (!checkBin.exists() || roudiBinary.startsWith("/usr/")) {
        roudiBinary = QStringLiteral("%1/syntalos-roudi").arg(SY_LIBDIR);
        QFileInfo fi(roudiBinary);
        roudiBinary = fi.canonicalFilePath();
    }

    if (d->roudiPidFd > 0) {
        // don't try to restart RouDi if it is already running
        if (isProcessRunning(d->roudiPidFd))
            return true;
        close(d->roudiPidFd);
    }

    qCDebug(logEngine).noquote() << "RouDi is not running, trying to restart it...";
    if (!launchProgram(roudiBinary, &d->roudiPidFd)) {
        QMessageBox::critical(
            d->parentWidget,
            QStringLiteral("System Error"),
            QStringLiteral("Unable to start the Syntalos IPC communication and shared-memory management daemon. "
                           "Something might be wrong with the system configuration. %1")
                .arg(std::strerror(errno)));
        return false;
    }

    bool fatalError = false;
    auto temporaryErrorHandler = iox::ErrorHandler::setTemporaryErrorHandler(
        [&](const iox::Error e, std::function<void()>, const iox::ErrorLevel level) {
            if (level == iox::ErrorLevel::FATAL) {
                QMessageBox::critical(
                    d->parentWidget,
                    QStringLiteral("System Error"),
                    QStringLiteral("Failed to set up IPC services: %1").arg(iox::ErrorHandler::toString(e)));
                qFatal("IOX runtime setup failed.");
            }
        });
    iox::runtime::PoshRuntime::initRuntime("syntalos");
    if (fatalError)
        return false;

    return true;
}

/**
 * @brief Return a list of active modules that have been sorted in the order they
 * should be prepared, run and overall be handled in (but not stopped in!).
 */
QList<AbstractModule *> Engine::createModuleExecOrderList()
{
    // While modules could in theory be initialized in arbitrary order,
    // it is more efficient and more predicatble if we initialize data-generating
    // modules and modules which do not receive input first, and the initialize
    // the ones which rely on data created by those modules.
    // Proper dependency resolution would be needed for a perfect solution,
    // but we only need one that's "good enough" here for now. So this algorithm
    // will not produce a perfect result, especially if there are cycles in the
    // module graph.
    QList<AbstractModule *> orderedActiveModules;
    QSet<AbstractModule *> assignedMods;

    const auto modCount = d->activeModules.length();
    assignedMods.reserve(modCount);
    orderedActiveModules.reserve(modCount);
    for (const auto &mod : d->activeModules) {
        if (assignedMods.contains(mod))
            continue;

        // modules with no input ports go first
        if (mod->inPorts().isEmpty()) {
            orderedActiveModules.prepend(mod);
            assignedMods.insert(mod);
            continue;
        }

        auto anySubscribed = false;
        for (const auto &iport : mod->inPorts()) {
            if (iport->hasSubscription()) {
                anySubscribed = true;
                const auto upstreamMod = iport->outPort()->owner();
                if (!assignedMods.contains(upstreamMod)) {
                    orderedActiveModules.append(upstreamMod);
                    assignedMods.insert(upstreamMod);
                }
            }
        }

        // just stop if all modules have been assigned
        if (assignedMods.size() == modCount)
            break;

        if (assignedMods.contains(mod))
            continue;

        if (!anySubscribed)
            orderedActiveModules.prepend(mod);
        else
            orderedActiveModules.append(mod);
        assignedMods.insert(mod);
    }

    if (orderedActiveModules.length() != modCount)
        qCCritical(logEngine).noquote() << "Invalid count of ordered modules:" << orderedActiveModules.length()
                                        << "!=" << modCount;
    assert(orderedActiveModules.length() == modCount);

    auto debugText = QStringLiteral("Running modules in order: ");
    for (auto &mod : orderedActiveModules)
        debugText.append(mod->name() + QStringLiteral("; "));
    qCDebug(logEngine).noquote() << debugText;

    return orderedActiveModules;
}

/**
 * @brief Create new module stop order from their exec order.
 */
QList<AbstractModule *> Engine::createModuleStopOrderFromExecOrder(const QList<AbstractModule *> &modExecList)
{
    QList<AbstractModule *> stopOrderMods;
    QSet<AbstractModule *> assignedMods;
    stopOrderMods.reserve(modExecList.length());

    for (auto &mod : modExecList) {
        if (assignedMods.contains(mod))
            continue;

        // FIXME: This is very ugly special-casing of a single module type, but we want to give users
        // a chance to still send Firmata commands when the system is terminating.
        // Possibly replace this with module-defined declarative StartupOrder/TerminateOrder later?
        if (mod->id() == QStringLiteral("firmata-io")) {
            for (const auto &iport : mod->inPorts()) {
                if (iport->hasSubscription()) {
                    const auto upstreamMod = iport->outPort()->owner();
                    if (upstreamMod->id() == QStringLiteral("pyscript")) {
                        if (!assignedMods.contains(upstreamMod)) {
                            stopOrderMods.append(upstreamMod);
                            assignedMods.insert(upstreamMod);

                            stopOrderMods.append(mod);
                            assignedMods.insert(mod);
                        } else {
                            // inefficiently search for the module we should stop after
                            for (ssize_t i = 0; i < stopOrderMods.length(); i++) {
                                if (upstreamMod == stopOrderMods[i]) {
                                    stopOrderMods.insert(i + 1, mod);
                                    assignedMods.insert(mod);
                                    break;
                                }
                            }
                        }

                        break;
                    }
                }
            }
        }

        // we need to check the set again here, as modules may have been
        // added while we were in this loop
        if (!assignedMods.contains(mod)) {
            stopOrderMods.append(mod);
            assignedMods.insert(mod);
        }
    }

    if (stopOrderMods.length() != modExecList.length())
        qCCritical(logEngine).noquote() << "Invalid count of stop-ordered modules:" << stopOrderMods.length()
                                        << "!=" << modExecList.length();
    assert(stopOrderMods.length() == modExecList.length());

    return stopOrderMods;
}

void Engine::notifyUsbHotplugEvent(UsbHotplugEventKind kind)
{
    if (kind == UsbHotplugEventKind::NONE)
        return;
    if (d->active || d->running) {
        // we are running, we must not dispatch the event
        return;
    }

    // notify our modules that something changed
    for (auto mod : d->activeModules)
        mod->usbHotplugEvent(kind);
}

bool Engine::run()
{
    if (d->running)
        return false;

    d->failed = true;          // if we exit before this is reset, initialization has failed
    d->runIsEphemeral = false; // not a volatile run

    if (d->activeModules.isEmpty()) {
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
                QStringLiteral("The disk '%1' is located on has low amounts of space available (< 8 GB). "
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
        auto reply = QMessageBox::question(
            d->parentWidget,
            QStringLiteral("Existing data found - Continue anyway?"),
            QStringLiteral("The directory '%1' already contains data (likely from a previous run). "
                           "If you continue, the old data will be deleted. Continue and delete data?")
                .arg(d->exportDir),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return false;

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
    if (d->activeModules.isEmpty()) {
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
    const auto memInfo = read_meminfo();

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
                    << QString("%1:%2[<%3]")
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
                << QString("%1:%2[<%3]").arg(msd.port->owner()->name(), msd.port->title(), msd.port->dataTypeName())
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
    d->monitoring->diskSpaceCheckTimer.setInterval(60 * 1000); // check every 60sec
    connect(&d->monitoring->diskSpaceCheckTimer, &QTimer::timeout, this, &Engine::onDiskspaceMonitorEvent);

    // watcher for remaining system memory
    d->monitoring->prevMemAvailablePercent = 100;
    d->monitoring->emergencyOOMStop = d->gconf->emergencyOOMStop();
    d->monitoring->memoryWarningEmitted = false;
    d->monitoring->memCheckTimer.setInterval(10 * 1000); // check every 10sec
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
    d->monitoring->subBufferCheckTimer.setInterval(10 * 1000); // check every 10sec
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
    extraData.insert("subject_id", d->testSubject.id.isEmpty() ? QVariant() : d->testSubject.id);
    extraData.insert("subject_group", d->testSubject.group.isEmpty() ? QVariant() : d->testSubject.group);
    extraData.insert("subject_comment", d->testSubject.comment.isEmpty() ? QVariant() : d->testSubject.comment);
    extraData.insert("recording_length_msec", finishTimestamp);
    extraData.insert("success", !d->failed);
    if (d->failed && !d->runFailedReason.isEmpty()) {
        extraData.insert("failure_reason", d->runFailedReason);
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
            QStringLiteral("Directory '%1' was expected to be nonexistent, but the directory exists. "
                           "Stopped run to prevent potential data loss. This condition should never happen.")
                .arg(exportDirPath));
        return false;
    }

    if (!makeDirectory(exportDirPath))
        return false;

    // ensure the RouDi daemon is running
    if (!ensureRoudi())
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
    std::shared_ptr<EDLCollection> storageCollection(
        new EDLCollection(QStringLiteral("%1_%2_%3")
                              .arg(d->testSubject.id)
                              .arg(d->experimentIdFinal)
                              .arg(QDateTime::currentDateTime().toString("yy-MM-dd hh:mm"))));
    storageCollection->setPath(exportDirPath);

    // if we should save internal diagnostic data, create a group for it!
    if (d->saveInternal) {
        d->edlInternalData = std::make_shared<EDLGroup>();
        d->edlInternalData->setName("syntalos_internal");
        storageCollection->addChild(d->edlInternalData);
        qCDebug(logEngine).noquote().nospace() << "Writing some internal data to datasets for debugging and analysis";
    }
    d->internalTSyncWriters.clear();

    // fetch list of modules in their activation order
    auto orderedActiveModules = createModuleExecOrderList();

    // create a new master timer for synchronization
    d->timer.reset(new SyncTimer);

    auto lastPhaseTimepoint = currentTimePoint();
    // assume success until a module actually fails
    bool initSuccessful = true;
    d->failed = false;

    // perform module name sanity check
    {
        QSet<QString> modNameSet;
        for (auto &mod : orderedActiveModules) {
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
                    QStringLiteral("A module with the name '%1' exists twice in this board, or another module has a "
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
    for (auto &mod : orderedActiveModules) {
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
    for (auto &mod : orderedActiveModules)
        mod->setPotentialNoaffinityCPUCount(potentialNoaffinityCPUCount);

    QCoreApplication::processEvents();

    // prepare modules
    for (auto &mod : orderedActiveModules) {
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
            auto storageGroup = storageCollection->groupByName(modInfo->storageGroupName(), true);
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
        // if the module hasn't set itself to ready yet, assume it is idle
        if (mod->state() != ModuleState::READY)
            mod->setState(ModuleState::IDLE);

        qCDebug(logEngine).noquote().nospace()
            << "Module '" << mod->name() << "' prepared in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    // exporter for streams so out-of-process mlink modules can access them
    emitStatusMessage(QStringLiteral("Exporting streams for external modules..."));
    auto streamExporter = std::make_unique<StreamExporter>();
    for (auto &mod : orderedActiveModules) {
        auto mlinkMod = qobject_cast<MLinkModule *>(mod);
        if (mlinkMod != nullptr)
            mlinkMod->markIncomingForExport(streamExporter.get());
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
            td.niceness = defaultThreadNice;
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
            td.name = QStringLiteral("%1-%2").arg(mod->id().midRef(0, 12)).arg(i);
            std::unique_ptr<SyThread> modThread(new SyThread(td, mod, startWaitCondition.get()));
            dThreads.push_back(std::move(modThread));
        }
        assert(dThreads.size() == (size_t)threadedModules.size());

        // collect all modules which do some kind of event-based execution
        {
            QHash<QString, int> remainingEvModCountById;
            for (auto &mod : orderedActiveModules) {
                if ((mod->driver() == ModuleDriverKind::EVENTS_SHARED)
                    || (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED)) {
                    if (!remainingEvModCountById.contains(mod->id()))
                        remainingEvModCountById[mod->id()] = 0;
                    remainingEvModCountById[mod->id()] += 1;
                }
            }

            // assign modules to their threads and give the groups an ID
            for (auto &mod : orderedActiveModules) {
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
        for (auto &mod : orderedActiveModules)
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
        for (auto &mod : orderedActiveModules) {
            if (mod->state() == ModuleState::READY)
                continue;
            // IDLE is also a valid state at this point, the module may not
            // have had additional setup to do
            if (mod->state() == ModuleState::IDLE)
                continue;
            emitStatusMessage(QStringLiteral("Waiting for '%1' to get ready...").arg(mod->name()));
            while (mod->state() != ModuleState::READY) {
                QThread::msleep(500);
                QCoreApplication::processEvents();
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
        for (auto &mod : orderedActiveModules) {
            if (mod->features().testFlag(ModuleFeature::CALL_UI_EVENTS))
                callUiEventModules.push_back(mod);
        }

        // start monitoring resource issues during this run
        startResourceMonitoring(orderedActiveModules, exportDirPath);

        // we officially start now, launch the timer
        d->timer->start();
        d->running = true;

        // first, launch all threaded and evented modules
        for (auto &mod : orderedActiveModules) {
            if ((mod->driver() != ModuleDriverKind::THREAD_DEDICATED)
                && (mod->driver() != ModuleDriverKind::EVENTS_DEDICATED)
                && (mod->driver() != ModuleDriverKind::EVENTS_SHARED))
                continue;

            mod->start();

            // ensure modules are in their "running" state now, or
            // have themselves declared "idle" (meaning they won't be used at all)
            mod->m_running = true;
            if (mod->state() != ModuleState::IDLE)
                mod->setState(ModuleState::RUNNING);
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

        // tell all non-threaded modules individuall now that we started
        for (auto &mod : orderedActiveModules) {
            // ignore threaded & evented
            if ((mod->driver() == ModuleDriverKind::THREAD_DEDICATED)
                || (mod->driver() == ModuleDriverKind::EVENTS_DEDICATED)
                || (mod->driver() == ModuleDriverKind::EVENTS_SHARED))
                continue;

            mod->start();

            // work around bad modules which don't set this on their own in start()
            mod->m_running = true;
            mod->setState(ModuleState::RUNNING);
        }

        qCDebug(logEngine).noquote().nospace() << "Startup phase completed, all modules are running. Took additional "
                                               << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";

        // tell listeners that we are running now
        emit runStarted();

        emitStatusMessage(QStringLiteral("Running..."));
        QCoreApplication::processEvents();

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

        // stop exporting streams to external modules
        streamExporter->stop();

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

    // send stop command to all modules
    for (auto &mod : createModuleStopOrderFromExecOrder(orderedActiveModules)) {
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
        if ((mod->state() != ModuleState::IDLE) && (mod->state() != ModuleState::ERROR))
            mod->setState(ModuleState::IDLE);

        qCDebug(logEngine).noquote().nospace()
            << "Module '" << mod->name() << "' stopped in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
    }

    lastPhaseTimepoint = d->timer->currentTimePoint();

    // join all dedicated module threads with the main thread again, waiting for them to terminate
    startWaitCondition->wakeAll(); // wake up all threads again, just in case one is stuck waiting
    for (size_t i = 0; i < dThreads.size(); i++) {
        auto &thread = dThreads[i];
        auto mod = threadedModules[i];
        emitStatusMessage(QStringLiteral("Waiting for '%1'...").arg(mod->name()));
        qApp->processEvents();

        // Some modules may be stuck in their threads, so we try our absolute best to not
        // make Syntalos hang while failing to join a misbehaving thread.
        // Ultimately though we must join the thread, so we will just give up eventually.

        // wait ~20sec for the thread to join
        if (thread->joinTimeout(20))
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
                finalizeExperimentMetadata(storageCollection, finishTimestamp, orderedActiveModules);

            emitStatusMessage(QStringLiteral("Unable to recover stalled module '%1' (⚠️ application may deadlock now)")
                                  .arg(mod->name()));
            QMessageBox::critical(
                d->parentWidget,
                QStringLiteral("Critical failure"),
                QStringLiteral("While stopping this experiment, module \"%1\" stalled and any attempts to wake it up "
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
    for (auto &mod : orderedActiveModules)
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
        finalizeExperimentMetadata(storageCollection, finishTimestamp, orderedActiveModules);
        qCDebug(logEngine).noquote().nospace()
            << "Manifest and additional data saved in " << timeDiffToNowMsec(lastPhaseTimepoint).count() << "msec";
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
    if (mod != nullptr)
        d->runFailedReason = QStringLiteral("%1(%2): %3").arg(mod->id(), mod->name(), message);
    else
        d->runFailedReason = QStringLiteral("?(?): %1").arg(message);

    const bool wasRunning = d->running;
    d->failed = true;
    d->running = false;

    // if we were running, defer message emission: We first need to terminate all modules
    // to ensure that a modal error message (which may be created when this event is received)
    // doesn't lock up the main thread and any other module's drawing buffers run full.
    if (wasRunning)
        d->pendingErrors.append(qMakePair(mod, message));
    else
        emit runFailed(mod, message);
}

void Engine::onSynchronizerDetailsChanged(const QString &id, const TimeSyncStrategies &, const microseconds_t &)
{
    if (!d->saveInternal)
        return;
    if (d->internalTSyncWriters.value(id).get() != nullptr)
        return;
    QString modId;
    auto mod = qobject_cast<AbstractModule *>(sender());
    if (mod != nullptr)
        modId = mod->id();

    std::shared_ptr<EDLDataset> ds(new EDLDataset);
    ds->setName(QStringLiteral("%1-%2").arg(modId).arg(id));
    d->edlInternalData->addChild(ds);

    std::shared_ptr<TimeSyncFileWriter> tsw(new TimeSyncFileWriter);
    tsw->setFileName(ds->setDataFile("offsets.tsync"));
    tsw->setTimeUnits(TSyncFileTimeUnit::MICROSECONDS, TSyncFileTimeUnit::MICROSECONDS);
    tsw->setTimeDataTypes(TSyncFileDataType::INT64, TSyncFileDataType::INT64);
    tsw->setTimeNames(QStringLiteral("approx-master-time"), QStringLiteral("sync-offset"));
    tsw->open(QStringLiteral("SyntalosInternal::%1_%2").arg(modId, mod->name().simplified()), ds->collectionId());
    d->internalTSyncWriters[id] = tsw;
}

void Engine::onSynchronizerOffsetChanged(const QString &id, const microseconds_t &currentOffset)
{
    if (!d->saveInternal)
        return;

    auto tsw = d->internalTSyncWriters.value(id);
    if (tsw.get() == nullptr)
        return;

    tsw->writeTimes(d->timer->timeSinceStartUsec(), currentOffset);
}
