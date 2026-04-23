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
#include "mlinkmodule.h"

#include <QDebug>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <iox2/iceoryx2.hpp>

#include "mlink/ipc-types-private.h"
#include "mlink/ipc-iox-private.h"
#include "utils/misc.h"

using namespace Syntalos::ipc;

/**
 * Safely receive from an iceoryx subscriber.
 * Returns the inner optional (empty = no data available) and logs a warning
 * instead of crashing when the receive itself fails.
 */
template<typename Sub>
static auto safeReceive(Sub &sub) -> std::remove_cvref_t<decltype(sub.receive().value())>
{
    auto result = sub.receive();
    if (!result.has_value()) {
        LOG_WARNING(getLogger("mlink"), "IPC receive failed: {}", iox2::bb::into<const char *>(result.error()));
        return {};
    }
    return std::move(result).value();
}

class MLinkModule::Private
{
public:
    Private(QuillLogger *logger)
        : log(logger) {};
    ~Private() = default;

    QuillLogger *log = nullptr;
    QProcess *proc = nullptr;
    ModuleWorkerMode workerMode;
    bool outputCaptured = false;
    QString pyVenvDir;
    QString scriptWDir;
    QString scriptContent;
    QString scriptFname;
    QHash<std::string, MetaStringMap> sentMetadata;

    LoadSettingsRequest settingsReq;

    bool portChangesAllowed = true;
    QHash<std::string, std::shared_ptr<VarStreamInputPort>> inPortIdMap;
    QHash<std::string, std::shared_ptr<VariantDataStream>> outPortIdMap;

    std::string clientId;
    std::optional<iox2::Node<iox2::ServiceType::Ipc>> node;

    // Subscribers to receive information from module processes
    std::optional<IoxSubscriber<ErrorEvent>> subError;
    std::optional<IoxSubscriber<StateChangeEvent>> subStateChange;
    std::optional<IoxUntypedReqServer> srvInPortChange;
    std::optional<IoxUntypedReqServer> srvOutPortChange;

    // Output port forwarders
    struct OutPortSub {
        std::optional<SySubscriber> sub;
        std::shared_ptr<StreamOutputPort> oport;
        std::optional<IoxWaitSetGuard> guard;
    };
    std::vector<OutPortSub> outPortSubs;

    // Listener to react to worker control events, notifier
    // to notify the worker if we send control events.
    std::optional<IoxListener> workerCtlEventListener;
    std::optional<IoxNotifier> ctlEventNotifier;
    QTimer *ctlEventTimer = nullptr;
    std::atomic_bool threadStopped = true;
    std::atomic_bool threadHandlingEvents = false;

    /**
     * Construct service name for a channel on this module.
     */
    [[nodiscard]] std::string svcName(const std::string &channel) const
    {
        assert(!clientId.empty());
        return makeModuleServiceName(clientId, channel);
    }

    /**
     * Notify the client that we have sent something on a control channel.
     */
    void notifyClient() const
    {
        if (!ctlEventNotifier.has_value()) [[unlikely]] {
            LOG_CRITICAL(log, "notifyWorker: Notifier was not initialized, can not notify client!");
            return;
        }

        auto r = ctlEventNotifier->notify();
        if (!r.has_value())
            LOG_WARNING(log, "Failed to notify worker of control event: {}", iox2::bb::into<const char *>(r.error()));
    }

    void checkClientError(MLinkModule *self)
    {
        if (!subError.has_value())
            return;

        while (true) {
            auto mSample = subError->receive();
            if (!mSample.has_value())
                break;
            const auto &sample = mSample.value();
            if (!sample.has_value())
                break;
            const auto &ev = sample->payload();
            const auto title = QString::fromUtf8(ev.title.unchecked_access().c_str());
            const auto msg = QString::fromUtf8(ev.message.unchecked_access().c_str());
            if (title.isEmpty())
                self->raiseError(msg);
            else
                self->raiseError(QStringLiteral("<html><b>%1</b><br/>%2").arg(title, msg));
        }
    }

    void replyDoneSlice(SliceActiveRequest &req, bool success) const
    {
        auto maybeResponse = req.loan_uninit();
        if (!maybeResponse.has_value()) {
            LOG_WARNING(
                log,
                "Failed to loan response for port change reply: {}",
                iox2::bb::into<const char *>(maybeResponse.error()));
            return;
        }
        iox2::send(std::move(maybeResponse).value().write_payload(DoneResponse{success})).value();
    }

    void checkClientStateChange(MLinkModule *self)
    {
        if (!subStateChange.has_value())
            return;

        while (true) {
            auto sample = safeReceive(*subStateChange);
            if (!sample.has_value())
                break;
            const auto newState = sample->payload().state;

            // the error state must only be set by raiseError(), never directly
            if (newState == ModuleState::ERROR)
                continue;

            // only some states are allowed to be set by the module
            if (newState == ModuleState::DORMANT || newState == ModuleState::READY
                || newState == ModuleState::INITIALIZING || newState == ModuleState::IDLE)
                self->setState(newState);
        }
    }

    /**
     * Synchronously call the client and wait for a response or an error.
     */
    template<typename Req, typename Res, typename Func>
    std::optional<Res> callClientSimple(
        MLinkModule *self,
        const std::string &channel,
        Func fillReqFn,
        int timeoutSec = 5,
        bool timeoutIsError = true,
        bool skipIfModuleError = true)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return std::nullopt;
        }

        auto client = makeTypedClient<Req, Res>(*node, svcName(channel));

        auto maybeReq = client.loan_uninit();
        if (!maybeReq.has_value()) {
            self->raiseError(
                QStringLiteral("Failed to loan shared memory for request on channel '%1': %2")
                    .arg(qstr(channel), QString::fromUtf8(iox2::bb::into<const char *>(maybeReq.error()))));
            return std::nullopt;
        }
        auto pendingReq = std::move(maybeReq).value();

        fillReqFn(pendingReq.payload_mut());
        auto pending = iox2::send(iox2::assume_init(std::move(pendingReq))).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            checkClientError(self);

            auto response = pending.receive().value();
            if (response.has_value())
                return response->payload();

            // quit immediately if an error was already emitted
            if (skipIfModuleError && self->state() == ModuleState::ERROR)
                return std::nullopt;

            // if we stopped running (crashed or existed) we no longer need to wait
            if (!self->isProcessRunning())
                return std::nullopt;

            if (timer.elapsed() > timeoutSec * 1000) {
                if (timeoutIsError)
                    self->raiseError(QStringLiteral("Timeout while waiting for response on: %1").arg(qstr(channel)));
                return std::nullopt;
            }

            // Pump worker-initiated port-change requests from the main thread only when
            // the dedicated module thread is not active. When the thread is running, it
            // handles these requests itself via its WaitSet; calling handleIncomingControl()
            // from both threads simultaneously would race on the server objects.
            if (!threadHandlingEvents) {
                self->handleIncomingControl();
                if (timeoutSec > 4)
                    qApp->processEvents();
            }

            std::this_thread::sleep_for(microseconds_t(5 * timeoutSec));
        }
    }

    /**
     * Synchronously call the client and wait for a "Done" response or an error.
     */
    template<typename Req, typename Func>
    bool callClientSimple(
        MLinkModule *self,
        const std::string &channel,
        Func fillReqFn,
        int timeoutSec = 5,
        bool timeoutIsError = true,
        bool skipIfModuleError = true)
    {
        auto res = callClientSimple<Req, DoneResponse>(
            self, channel, fillReqFn, timeoutSec, timeoutIsError, skipIfModuleError);
        if (!res.has_value())
            return false;
        return res->success;
    }

    template<typename ReqData>
    bool callSliceClientSimple(
        MLinkModule *self,
        const std::string &channel,
        const ReqData &reqEntity,
        int timeoutSec = 5)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return false;
        }

        auto client = makeSliceClient<DoneResponse>(*node, svcName(channel));

        const auto bytes = reqEntity.toBytes();
        auto maybeSlice = client.loan_slice_uninit(static_cast<uint64_t>(bytes.size()));
        if (!maybeSlice.has_value()) {
            self->raiseError(QStringLiteral("Failed to loan shared memory for request on '%1': %2")
                                 .arg(qstr(channel), iox2::bb::into<const char *>(maybeSlice.error())));
            return false;
        }
        auto rawSlice = std::move(maybeSlice).value();
        std::memmove(rawSlice.payload_mut().data(), bytes.data(), bytes.size());
        auto pending = iox2::send(iox2::assume_init(std::move(rawSlice))).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            checkClientError(self);
            if (!threadHandlingEvents) {
                self->handleIncomingControl();
                if (timeoutSec > 4)
                    qApp->processEvents();
            }
            auto response = pending.receive().value();
            if (response.has_value())
                return response->payload().success;

            // quit immediately if an error was already emitted
            if (self->state() == ModuleState::ERROR)
                return false;

            // if we stopped running (crashed or exited) we no longer need to wait
            if (!self->isProcessRunning())
                return false;

            if (timer.elapsed() > timeoutSec * 1000) {
                self->raiseError(QStringLiteral("Timeout while waiting for response on: %1").arg(qstr(channel)));
                return false;
            }

            std::this_thread::sleep_for(microseconds_t(5 * timeoutSec));
        }
    }

    template<typename ReqData, typename ResData>
    std::optional<ResData> callSliceClient(
        MLinkModule *self,
        const std::string &channel,
        const ReqData &reqEntity,
        int timeoutSec = 5)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return std::nullopt;
        }

        auto client = makeSliceClient<IoxByteSlice>(*node, svcName(channel));

        const auto bytes = reqEntity.toBytes();
        auto maybeSlice = client.loan_slice_uninit(static_cast<uint64_t>(bytes.size()));
        if (!maybeSlice.has_value()) {
            self->raiseError(QStringLiteral("Failed to loan shared memory for request on '%1': %2")
                                 .arg(qstr(channel), iox2::bb::into<const char *>(maybeSlice.error())));
            return std::nullopt;
        }
        auto rawSlice = std::move(maybeSlice).value();
        std::memmove(rawSlice.payload_mut().data(), bytes.data(), bytes.size());
        auto pending = iox2::send(iox2::assume_init(std::move(rawSlice))).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            checkClientError(self);
            if (!threadHandlingEvents) {
                self->handleIncomingControl();
                if (timeoutSec > 4)
                    qApp->processEvents();
            }

            auto response = pending.receive().value();
            if (response.has_value()) {
                const auto pl = response->payload();
                return ResData::fromMemory(pl.data(), pl.number_of_bytes());
            }

            // quit immediately if an error was already emitted
            if (self->state() == ModuleState::ERROR)
                return std::nullopt;

            // if we stopped running (crashed or exited) we no longer need to wait
            if (!self->isProcessRunning())
                return std::nullopt;

            if (timer.elapsed() > timeoutSec * 1000) {
                self->raiseError(QStringLiteral("Timeout while waiting for response on: %1").arg(qstr(channel)));
                return std::nullopt;
            }

            std::this_thread::sleep_for(microseconds_t(5 * timeoutSec));
        }
    }
};

MLinkModule::MLinkModule(QObject *parent)
    : AbstractModule(parent),
      d(new MLinkModule::Private(m_log))
{
    d->proc = new QProcess(this);
    d->workerMode = ModuleWorkerMode::PERSISTENT;
    d->portChangesAllowed = true;

    // merge stdout/stderr of external process with ours by default
    setOutputCaptured(false);

    connect(d->proc, &QProcess::readyReadStandardOutput, this, [this]() {
        if (d->outputCaptured)
            Q_EMIT processOutputReceived(readProcessOutput());
    });
    connect(
        d->proc,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::CrashExit) {
                raiseError(QStringLiteral("Module process crashed with exit code %1! Check the log for details.")
                               .arg(exitCode));
            }
        });

    d->ctlEventTimer = new QTimer(this);
    d->ctlEventTimer->setInterval(100);
    connect(d->ctlEventTimer, &QTimer::timeout, this, [this]() {
        handleIncomingControl();
    });
}

bool MLinkModule::initialize()
{
    // propagate the (potentially updated) logger
    d->log = m_log;

    if (moduleBinary().isEmpty()) {
        raiseError("Unable to find module binary. Is the module installed correctly?");
        return false;
    }

    // Ensure the module process is running. This will also call resetConnection()
    // to initialize the connection at this point, since only now do we know the
    // module ID and index and can react to & recover from errors properly.
    if (!runProcess())
        return false;

    return AbstractModule::initialize();
}

MLinkModule::~MLinkModule()
{
    d->ctlEventTimer->stop();
    terminateProcess();
}

/**
 * Test if module communication works and the module has a suitable IPC API level.
 *
 * @param emitErrors If true, an error will be raised if the version is unsuitable or communication fails
 * @return True if the version is suitable, false if communication failed or API level is unsuitable.
 */
bool MLinkModule::testIpcApiVersion(bool emitErrors)
{
    // do nothing if the error channel does not exist yet
    if (!d->subError)
        return false;

    auto modApiTagResponse = d->callClientSimple<ApiVersionRequest, ApiVersionResponse>(
        this, API_VERSION_CALL_ID, [&](auto &) {});
    if (!modApiTagResponse.has_value()) {
        if (emitErrors)
            raiseError(QStringLiteral("Failed to communicate with module process to check API version!"));
        return false;
    }

    auto modApiVersion = QString::fromUtf8(modApiTagResponse->apiVersion.unchecked_access().c_str());
    const auto syApiVersion = QStringLiteral(SY_MODULE_API_TAG);

    if (modApiVersion != syApiVersion) {
        if (emitErrors)
            raiseError(QStringLiteral("Module API version mismatch: worker reports '%1', expected '%2'.")
                           .arg(modApiVersion, syApiVersion));
        return false;
    }

    return true;
}

/**
 * Process all incoming IPC data on the control channels and forward it.
 *
 * This does *not* handle the high-volume data-plane channels, which have
 * dedicated event/listener pairs for more efficient communication.
 */
void MLinkModule::handleIncomingControl()
{
    // do nothing if the error channel does not exist yet
    if (!d->subError)
        return;

    // Drain the control event listener to keep its socket buffer clear.
    // We *must* drain at the start to immediately consume the notification that triggered this call,
    // and to prevent race conditions with new events arriving while we process the previous one.
    if (d->workerCtlEventListener.has_value())
        drainListenerEvents(*d->workerCtlEventListener);

    // Error events
    d->checkClientError(this);

    // State changes
    d->checkClientStateChange(this);

    // Input port change requests
    if (d->srvInPortChange.has_value()) {
        while (true) {
            auto req = safeReceive(*d->srvInPortChange);
            if (!req.has_value())
                break;

            // deserialize
            const auto pl = req->payload();
            const auto ipc = InputPortChangeRequest::fromMemory(pl.data(), pl.number_of_bytes());
            const auto action = ipc.action;
            if (!d->portChangesAllowed) {
                LOG_WARNING(m_log, "Input port change request ignored: No changes are allowed.");
            } else if (action == PortAction::ADD) {
                const auto portId = QString::fromStdString(ipc.id);
                const auto portTitle = QString::fromStdString(ipc.title);
                auto iport = inPortById(portId);
                if (iport && iport->dataTypeId() != ipc.dataTypeId) {
                    removeInPortById(portId);
                    iport = nullptr;
                }
                if (!iport)
                    iport = registerInputPortByTypeId(ipc.dataTypeId, portId, portTitle);
                d->inPortIdMap[ipc.id] = iport;
            } else if (action == PortAction::REMOVE) {
                removeInPortById(QString::fromStdString(ipc.id));
                d->inPortIdMap.remove(ipc.id);
            }

            // always acknowledge so the worker never waits too long
            d->replyDoneSlice(*req, true);
        }
    }

    // Output port change requests
    if (d->srvOutPortChange.has_value()) {
        while (true) {
            auto req = safeReceive(*d->srvOutPortChange);
            if (!req.has_value())
                break;

            // deserialize
            const auto pl = req->payload();
            const auto opc = OutputPortChangeRequest::fromMemory(pl.data(), pl.number_of_bytes());
            const auto action = opc.action;
            if (action == PortAction::ADD) {
                if (!d->portChangesAllowed) {
                    LOG_WARNING(m_log, "Output port addition ignored: No changes are allowed.");
                } else {
                    // only register a new output port if we don't have one with that ID already
                    const auto portId = QString::fromStdString(opc.id);
                    const auto portTitle = QString::fromStdString(opc.title);
                    auto oport = outPortById(portId);
                    std::shared_ptr<VariantDataStream> ostream;
                    if (oport) {
                        if (oport->dataTypeId() != opc.dataTypeId) {
                            removeOutPortById(portId);
                            oport = nullptr;
                        } else {
                            ostream = oport->streamVar();
                        }
                    }
                    if (!ostream)
                        ostream = registerOutputPortByTypeId(opc.dataTypeId, portId, portTitle);
                    ostream->setMetadata(opc.metadata);
                    d->outPortIdMap[opc.id] = ostream;
                }
            } else if (action == PortAction::REMOVE) {
                if (!d->portChangesAllowed) {
                    LOG_WARNING(m_log, "Output port removal ignored: No changes are allowed.");
                } else {
                    removeOutPortById(QString::fromStdString(opc.id));
                    d->outPortIdMap.remove(opc.id);
                }
            } else if (action == PortAction::CHANGE) {
                std::shared_ptr<VariantDataStream> ostream;
                if (d->outPortIdMap.contains(opc.id))
                    ostream = d->outPortIdMap.value(opc.id);
                else if (auto oport = outPortById(qstr(opc.id)))
                    ostream = oport->streamVar();
                if (ostream) {
                    ostream->setMetadata(opc.metadata);
                    updateCommonStreamMetadata();
                }
            }

            // always acknowledge so the worker never blocks indefinitely
            d->replyDoneSlice(*req, true);
        }
    }
}

void MLinkModule::resetConnection()
{
    d->clientId = QStringLiteral("%1_%2").arg(id()).arg(index()).toStdString();

    // create a fresh node for this module connection
    d->node.emplace(makeIoxNode("syntalos-master-" + d->clientId));

    // ensure the old connections are gone before we are trying to create new ones
    d->subError.reset();
    d->subStateChange.reset();
    d->srvInPortChange.reset();
    d->srvOutPortChange.reset();
    d->workerCtlEventListener.reset();
    d->ctlEventNotifier.reset();

    // (re)create subscribers/servers for client -> master data channels
    d->subError.emplace(makeTypedSubscriber<ErrorEvent>(*d->node, d->svcName(ERROR_CHANNEL_ID)));
    d->subStateChange.emplace(makeTypedSubscriber<StateChangeEvent>(*d->node, d->svcName(STATE_CHANNEL_ID)));
    d->srvInPortChange.emplace(makeSliceServer(*d->node, d->svcName(IN_PORT_CHANGE_CHANNEL_ID)));
    d->srvOutPortChange.emplace(makeSliceServer(*d->node, d->svcName(OUT_PORT_CHANGE_CHANNEL_ID)));

    // control listener: Called when the client publishes a control command
    d->workerCtlEventListener.emplace(ipc::makeEventListener(*d->node, d->svcName(WORKER_CTL_EVENT_ID)));
    // control notifier: We use this to wake up the client when we made a request
    d->ctlEventNotifier.emplace(ipc::makeEventNotifier(*d->node, d->svcName(MASTER_CTL_EVENT_ID)));
}

ModuleDriverKind MLinkModule::driver() const
{
    return ModuleDriverKind::THREAD_DEDICATED;
}

ModuleFeatures MLinkModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY | ModuleFeature::SHOW_SETTINGS;
}

QString MLinkModule::moduleBinary() const
{
    return d->proc->program();
}

void MLinkModule::setModuleBinary(const QString &binaryPath)
{
    d->proc->setArguments(QStringList());
    d->proc->setProgram(binaryPath);
}

void MLinkModule::setModuleBinaryArgs(const QStringList &args)
{
    d->proc->setArguments(args);
}

void MLinkModule::setModuleBinaryWorkDir(const QString &wdir)
{
    d->proc->setWorkingDirectory(wdir);
}

QProcessEnvironment MLinkModule::moduleBinaryEnv() const
{
    const auto env = d->proc->processEnvironment();
    if (env.isEmpty())
        return QProcessEnvironment::systemEnvironment();
    return env;
}

void MLinkModule::setModuleBinaryEnv(const QProcessEnvironment &env)
{
    d->proc->setProcessEnvironment(env);
}

ModuleWorkerMode MLinkModule::workerMode() const
{
    return d->workerMode;
}

void MLinkModule::setWorkerMode(ModuleWorkerMode mode)
{
    d->workerMode = mode;
}

bool MLinkModule::outputCaptured() const
{
    return d->outputCaptured;
}

void MLinkModule::setOutputCaptured(bool capture)
{
    d->outputCaptured = capture;
    if (d->outputCaptured)
        d->proc->setProcessChannelMode(QProcess::MergedChannels);
    else
        d->proc->setProcessChannelMode(QProcess::ForwardedChannels);
}

void MLinkModule::setPythonVirtualEnv(const QString &venvDir)
{
    d->pyVenvDir = venvDir;
}

void MLinkModule::setScript(const QString &script, const QString &wdir)
{
    d->scriptWDir = wdir;
    d->scriptContent = script;
}

bool MLinkModule::setScriptFromFile(const QString &fname, const QString &wdir)
{
    QFile f(fname);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return false;

    QTextStream in(&f);
    setScript(in.readAll(), wdir);

    d->scriptFname = fname;
    return true;
}

bool MLinkModule::sendSettings()
{
    if (d->settingsReq.data.empty())
        return true;

    if (!isProcessRunning()) {
        qDebug() << "Tried to send settings data to dead module process.";
        return false;
    }

    if (!d->callSliceClientSimple(this, LOAD_SETTINGS_CALL_ID, d->settingsReq, 10))
        return false;
    return true;
}

static QByteArray byteVectorToQByteArray(const ByteVector &bv)
{
    return QByteArray(reinterpret_cast<const char *>(bv.data()), static_cast<qsizetype>(bv.size()));
}

void MLinkModule::serializeSettings(const QString &confBaseDir, QVariantHash &settings, QByteArray &extraData)
{
    Q_UNUSED(settings)

    d->settingsReq.baseDir = confBaseDir.toStdString();
    if (!isProcessRunning()) {
        LOG_WARNING(m_log, "Tried to save settings, but module process is dead. Reusing old settings.");
        extraData = byteVectorToQByteArray(d->settingsReq.data);
        return;
    }

    SaveSettingsRequest ssReq{
        .baseDir = confBaseDir.toStdString(),
    };

    auto response = d->callSliceClient<SaveSettingsRequest, SaveSettingsResponse>(
        this, SAVE_SETTINGS_CALL_ID, ssReq, 15);
    if (!response.has_value()) {
        LOG_WARNING(m_log, "Failed to save settings (issue communicating with the module). Reusing old settings.");
        extraData = byteVectorToQByteArray(d->settingsReq.data);
        return;
    }
    const auto &ssRes = response.value();
    if (!ssRes.success) {
        LOG_WARNING(m_log, "Module failed to serialize settings. Reusing old settings.");
        extraData = byteVectorToQByteArray(d->settingsReq.data);
        return;
    }

    // finally, we got proper settings
    extraData = byteVectorToQByteArray(ssRes.data);
    d->settingsReq.data = ssRes.data;
}

bool MLinkModule::loadSettings(const QString &confBaseDir, const QVariantHash &settings, const QByteArray &extraData)
{
    Q_UNUSED(settings)
    ByteVector bv(
        reinterpret_cast<const std::byte *>(extraData.constData()),
        reinterpret_cast<const std::byte *>(extraData.constData()) + extraData.size());

    d->settingsReq.baseDir = confBaseDir.toStdString();
    d->settingsReq.data = std::move(bv);

    // tell the module
    sendSettings();

    return true;
}

void MLinkModule::showDisplayUi()
{
    if (!d->callClientSimple<ShowDisplayRequest>(this, SHOW_DISPLAY_CALL_ID, [](auto &) {}))
        LOG_WARNING(m_log, "Show display request failed!");
}

void MLinkModule::showSettingsUi()
{
    // pick up any recently saved settings before we hand them back to the worker UI
    handleIncomingControl();

    if (!d->callClientSimple<ShowSettingsRequest>(this, SHOW_SETTINGS_CALL_ID, [](auto &) {}))
        LOG_WARNING(m_log, "Request to show settings UI has failed!");

    // drain immediate updates emitted while handling the request
    handleIncomingControl();
}

void MLinkModule::terminateProcess()
{
    // control polling in the GUI thread is only needed while the worker is alive
    d->ctlEventTimer->stop();

    if (!isProcessRunning())
        return;

    // request the module process to terminate itself
    d->callClientSimple<ShutdownRequest>(
        this,
        SHUTDOWN_CALL_ID,
        [](auto &) {},
        5,     // timeout seconds
        false, // timeout is not an error
        false  // we do not fast-exit if the module is in an error-state
    );

    // give the process some time to terminate
    d->proc->waitForFinished(5000);

    // ask nicely
    if (d->proc->state() == QProcess::Running) {
        LOG_INFO(m_log, "Module process {} did not terminate on request. Sending SIGTERM.", d->proc->program());
        d->proc->terminate();
        d->proc->waitForFinished(3000);
        d->proc->terminate();
        d->proc->waitForFinished(3000);
    }

    // no response? kill it!
    if (d->proc->state() == QProcess::Running) {
        LOG_WARNING(m_log, "Module process {} failed to quit. Killing it.", d->proc->program());
        d->proc->kill();
        d->proc->waitForFinished(5000);
    }

    // drain any now-stale events
    drainListenerEvents(*d->workerCtlEventListener);
}

bool MLinkModule::runProcess()
{
    // ensure any existing process does not exist
    terminateProcess();

    // reset connection, just in case we changed our ID
    resetConnection();

    if (d->proc->program().isEmpty()) {
        LOG_ERROR(m_log, "MLink module has not set a worker binary");
        return false;
    }

    auto penv = moduleBinaryEnv();
    penv.insert("SYNTALOS_VERSION", syntalosVersionFull());
    penv.insert("SYNTALOS_MODULE_ID", d->clientId.c_str());
    if (!d->pyVenvDir.isEmpty()) {
        penv.remove("PYTHONHOME");
        penv.insert("VIRTUAL_ENV", d->pyVenvDir);
        penv.insert("PATH", QStringLiteral("%1/bin/:%2").arg(d->pyVenvDir, penv.value("PATH", "")));
    }

    // when launching the external process, we are back at initialization
    auto prevState = state();
    setState(ModuleState::INITIALIZING);

    d->proc->setProcessEnvironment(penv);
    d->proc->start(d->proc->program(), d->proc->arguments());
    if (!d->proc->waitForStarted())
        return false;

    // wait for the service to show up & initialize
    bool workerFound = false;
    bool moduleInitDone = false;
    QElapsedTimer timer;
    timer.start();
    do {
        qApp->processEvents();
        handleIncomingControl();

        if (!workerFound && state() != prevState)
            workerFound = true;
        if (state() != ModuleState::INITIALIZING && state() != prevState)
            moduleInitDone = true;

        if (!workerFound || !moduleInitDone)
            std::this_thread::sleep_for(microseconds_t(1500));

        if (timer.elapsed() > 15 * 1000)
            break;
    } while (!workerFound || !moduleInitDone);

    if (!workerFound) {
        raiseError(
            "Module communication interface did not show up in time! The module might have crashed or may not be "
            "configured correctly.");
        d->proc->kill();
        return false;
    }

    if (!moduleInitDone) {
        raiseError("Module initialization failed! The module might have failed or was taking too long to initialize.");
        d->proc->kill();
        return false;
    }

    // verify IPC/API compatibility before issuing further control commands
    if (!testIpcApiVersion(true)) {
        d->proc->terminate();
        return false;
    }

    if (state() != ModuleState::ERROR)
        setState(prevState);

    // send settings to the worker, if we have any (might be the case if we are trying
    // to resurrect a crashed worker)
    sendSettings();

    // Keep control events flowing while the worker is alive and no module run thread is active.
    d->ctlEventTimer->start();

    return true;
}

bool MLinkModule::isProcessRunning() const
{
    return d->proc->state() == QProcess::Running;
}

bool MLinkModule::loadCurrentScript(bool resetPorts)
{
    if (d->scriptContent.isEmpty())
        return true;

    LoadScriptRequest req;
    req.workingDir = d->scriptWDir.toStdString();
    req.venvDir = d->pyVenvDir.toStdString();
    req.script = d->scriptContent.toStdString();
    req.resetPorts = resetPorts;

    // The worker executes the script before sending the Done reply, so we must
    // allow enough time for module-level code to finish (e.g., heavy imports).
    // 60 seconds is generous and should avoid false timeouts for slow environments.
    return d->callSliceClientSimple(this, LOAD_SCRIPT_CALL_ID, req, 60);
}

bool MLinkModule::sendPortInformation()
{
    // set the ports that are selected on this module
    {
        SetPortsPresetRequest req;

        for (const auto &iport : inPorts()) {
            InputPortChangeRequest ipc(PortAction::CHANGE);
            ipc.id = iport->id().toStdString();
            ipc.dataTypeId = iport->dataTypeId();
            ipc.title = iport->title().toStdString();
            req.inPorts.push_back(std::move(ipc));
        }

        for (const auto &oport : outPorts()) {
            OutputPortChangeRequest opc(PortAction::CHANGE);
            opc.id = oport->id().toStdString();
            opc.dataTypeId = oport->dataTypeId();
            opc.title = oport->title().toStdString();

            // topology for one publisher
            opc.topology = makeIpcServiceTopology(1, oport->streamVar()->subscriberCount());

            req.outPorts.push_back(std::move(opc));
        }

        if (!d->callSliceClientSimple(this, SET_PORTS_PRESET_CALL_ID, req))
            return false;
    }

    // update input port metadata
    for (const auto &iport : inPorts()) {
        if (!iport->hasSubscription())
            continue;

        UpdateInputPortMetadataRequest req;
        req.id = iport->id().toStdString();
        req.metadata = iport->subscriptionVar()->metadata();

        d->sentMetadata[req.id] = req.metadata;
        if (!d->callSliceClientSimple(this, IN_PORT_UPDATE_METADATA_ID, req))
            return false;
    }

    return true;
}

QString MLinkModule::readProcessOutput()
{
    if (!d->outputCaptured)
        return {};
    return d->proc->readAllStandardOutput();
}

void MLinkModule::markIncomingForExport(StreamExporter *exporter)
{
    for (auto &iport : inPorts()) {
        const auto res = exporter->publishStreamByPort(iport);
        if (!res.has_value()) {
            // there was an error!
            raiseError(res.error());
            continue;
        }
        const auto &details = res.value();
        if (!details.has_value())
            continue;

        ConnectInputRequest req;
        req.portId = IoxServiceNameString::from_utf8_null_terminated_unchecked_truncated(
            iport->id().toUtf8().constData(), iport->id().toUtf8().size());
        req.instanceId = IoxServiceNameString::from_utf8_null_terminated_unchecked_truncated(
            details->instanceId.toUtf8().constData(), details->instanceId.toUtf8().size());
        req.channelId = IoxServiceNameString::from_utf8_null_terminated_unchecked_truncated(
            details->channelId.toUtf8().constData(), details->channelId.toUtf8().size());
        req.topology = makeIpcServiceTopology(1, iport->outPort()->streamVar()->subscriberCount());

        bool ret = d->callClientSimple<ConnectInputRequest>(this, CONNECT_INPUT_CALL_ID, [&req](auto &payload) {
            payload = req;
        });
        if (!ret)
            LOG_WARNING(m_log, "Failed to connect exported input port {}", iport->title());
    }
}

bool MLinkModule::registerOutPortForwarders()
{
    // ensure we are disconnected
    disconnectOutPortForwarders();

    // connect to external process streams
    for (auto &oport : outPorts()) {
        if (!oport->streamVar()->hasSubscribers())
            continue;

        Private::OutPortSub ps;
        const auto topology = makeIpcServiceTopology(1, oport->streamVar()->subscriberCount());
        try {
            ps.sub.reset(); // ensure the old subscription is gone before we try to create a new one
            ps.sub.emplace(SySubscriber::create(*d->node, d->clientId, "o/" + oport->id().toStdString(), topology));
            ps.oport = oport;
            d->outPortSubs.push_back(std::move(ps));
        } catch (const std::exception &e) {
            raiseError(QStringLiteral("Failed to connect output port '%1': %2").arg(oport->title(), e.what()));
            return false;
        }

        // NOTE: oport->startStream() is intentionally NOT called here.
        // It is called at the end of MLinkModule::prepare(), after all
        // OutputPortChange messages from the worker's prepare() callback have
        // been processed by handleIncomingControl(). This ensures that
        // DataStream::start() snapshots the complete, final metadata into every
        // subscription so that downstream modules see correct values during
        // their own prepare() phase. The snapshot is repeated in start() to
        // pick up any last-minute changes from the worker's start() callback.
    }

    return true;
}

void MLinkModule::disconnectOutPortForwarders()
{
    // stop listening to messages from external process
    for (auto &ps : d->outPortSubs) {
        ps.oport->stopStream();
        ps.sub->drain();
    }
    d->outPortSubs.clear();
}

bool MLinkModule::prepare(const TestSubject &subject)
{
    // ensure we are reading any messages from the module process
    d->ctlEventTimer->start();

    // at this point, ensure the module process is actually running
    if (!isProcessRunning()) {
        if (!runProcess())
            return false;
    }

    // use version check as a "ping" to see if the worker is alive
    if (!testIpcApiVersion(false)) {
        raiseError("Failed to communicate with module process! Version check failed.");
        return false;
    }

    // set module process niceness
    if (!d->callClientSimple<SetNicenessRequest>(this, SET_NICENESS_CALL_ID, [&](auto &req) {
            req.nice = defaultThreadNiceness();
        }))
        return false;

    // set module process realtime priority
    if (!d->callClientSimple<SetMaxRealtimePriority>(this, SET_MAX_RT_PRIORITY_CALL_ID, [&](auto &req) {
            req.priority = defaultRealtimePriority();
        }))
        return false;

    // send all port information to the module (sets topology / metadata on worker side)
    if (!sendPortInformation())
        return false;

    // set the script to be run, if any exists and we are using a transient worker
    // (for persistent workers, the script has already been loaded in initialize())
    // resetPorts is NOT requested here: the worker keeps the ports that sendPortInformation()
    // just provided so the script can call get_input_port() / get_output_port() at module level.
    if (d->workerMode == ModuleWorkerMode::TRANSIENT) {
        if (!loadCurrentScript(false))
            return false;
    }

    // ensure we use the latest settings data received from the worker
    handleIncomingControl();

    // call the module's own startup preparations
    PrepareRunRequest prepReq{
        .subjectId = subject.id.toStdString(),
        .subjectGroup = subject.group.toStdString(),
    };
    if (!d->callSliceClientSimple(this, PREPARE_RUN_CALL_ID, prepReq, 15))
        return false;

    QElapsedTimer timer;
    timer.start();
    while (state() != ModuleState::READY) {
        handleIncomingControl();
        qApp->processEvents();
        if (state() == ModuleState::ERROR)
            return false;

        // we give modules 30sec to prepare, in case they are very slow
        if (timer.elapsed() > 30 * MS_PER_S) {
            raiseError("Timeout while waiting for module. Module did not transition to 'ready' state in 30 seconds.");
            return false;
        }
    }

    // Final drain to pick up any control events (e.g. settings changes) that arrived
    // concurrently with the READY state. Port changes are acknowledged synchronously
    // via request-response, so they are already applied by the time READY is seen.
    handleIncomingControl();

    // register output port forwarding from exported data streams to internal data transmission
    if (!registerOutPortForwarders())
        return false;
    if (state() == ModuleState::ERROR)
        return false;

    // ensure common metadata on the output ports is up-to-date
    updateCommonStreamMetadata();

    // Snapshot the now-final post-prepare() metadata into every output-port subscription.
    // This is done here - before start() - so that downstream modules can already read the
    // correct metadata from their input-port subscriptions during their own prepare() phase.
    // The engine prepares modules in graph order, so a downstream module's prepare() runs
    // after this point and will see the up-to-date values.
    // The snapshot is repeated implicitly in start() too, via startStream(), so we pick
    // up any last-minute changes as well.
    for (auto &ps : d->outPortSubs)
        ps.oport->streamVar()->commitMetadata();

    d->portChangesAllowed = false;
    return true;
}

void MLinkModule::start()
{
    d->portChangesAllowed = false;

    // timestamp when this module was started
    auto startTimestampUs =
        std::chrono::duration_cast<std::chrono::microseconds>(m_syTimer->currentTimePoint().time_since_epoch()).count();

    // update input port metadata if the metadata has changed - this may happen in case of circular module connections
    for (auto &iport : inPorts()) {
        if (!iport->hasSubscription())
            continue;

        const auto mdata = iport->subscriptionVar()->metadata();
        const auto portId = iport->id().toStdString();
        if (d->sentMetadata.value(portId, MetaStringMap()) == mdata)
            continue;

        UpdateInputPortMetadataRequest req;
        req.id = portId;
        req.metadata = mdata;
        if (!d->callSliceClientSimple(this, IN_PORT_UPDATE_METADATA_ID, req))
            return;
    }
    d->sentMetadata.clear();

    // tell the module to launch!
    d->callClientSimple<StartRequest>(
        this,
        START_CALL_ID,
        [&](auto &req) {
            req.startTimestampUsec = startTimestampUs;
        },
        2);

    // stop reading control events in the GUI thread - the module thread will do that for us soon
    d->ctlEventTimer->stop();

    // Start all streams. This re-snapshots the (now-final and immutable) metadata
    // into all output-port subscriptions.
    // The first snapshot was taken at the end of prepare(); this second pass picks
    // up any last-minute changes the worker's start() callback may have published,
    // as a last-minute safety net.
    for (auto &ps : d->outPortSubs)
        ps.oport->startStream();

    // call generic
    AbstractModule::start();
}

void MLinkModule::runThread(OptionalWaitCondition *startWaitCondition)
{
    d->threadStopped = false;

    // create waitset and attach control guard
    auto waitSet = iox2::WaitSetBuilder()
                       .signal_handling_mode(iox2::SignalHandlingMode::HandleTerminationRequests)
                       .create<iox2::ServiceType::Ipc>()
                       .value();
    auto waitSetCtlGuard = waitSet.attach_notification(*d->workerCtlEventListener).value();

    // prepare guards for output port forwarding
    for (auto &ps : d->outPortSubs) {
        if (!ps.sub.has_value())
            continue;
        ps.guard.emplace(waitSet.attach_notification(*ps.sub).value());
    }

    auto onEvent =
        [this, &waitSetCtlGuard](
            const iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> &attachmentId) -> iox2::CallbackProgression {
        // handle control messages
        if (attachmentId.has_event_from(waitSetCtlGuard)) {
            handleIncomingControl();
        } else {
            for (auto &ps : d->outPortSubs) {
                if (!ps.guard.has_value())
                    continue;
                if (!attachmentId.has_event_from(*ps.guard))
                    continue;

                // We have incoming data! - handle it, the break because the event
                // is per single attachment ID.
                ps.sub->handleEvents([&ps](const IoxImmutableByteSlice &pl) {
                    ps.oport->streamVar()->pushRawData(ps.oport->dataTypeId(), pl.data(), pl.number_of_bytes());
                });
                break;
            }
        }

        return iox2::CallbackProgression::Continue;
    };

    startWaitCondition->wait(this);

    d->threadHandlingEvents = true;
    while (m_running) {
        // wait for data - we need to time out every once in a while to check if we are still running
        waitSet.wait_and_process_once_with_timeout(onEvent, iox2::bb::Duration::from_millis(50)).value();
    }
    d->threadHandlingEvents = false;

    // MUST reset output-port guards before the local WaitSet goes out of scope.
    // iceoryx2 contract: "WaitSetGuard must live at most as long as the WaitSet."
    for (auto &ps : d->outPortSubs)
        ps.guard.reset();

    // we finished - drain incoming control messages from the module process one more time
    handleIncomingControl();

    // Drain any data the OOP process published before responding to the stop signal
    // but that the WaitSet loop hadn't yet forwarded.
    for (auto &ps : d->outPortSubs) {
        if (!ps.sub.has_value())
            continue;
        ps.sub->handleEvents([&ps](const IoxImmutableByteSlice &pl) {
            ps.oport->streamVar()->pushRawData(ps.oport->dataTypeId(), pl.data(), pl.number_of_bytes());
        });
    }

    // disconnect forwarders
    disconnectOutPortForwarders();

    d->threadStopped = true;
}

void MLinkModule::stop()
{
    if (isProcessRunning())
        d->callClientSimple<StopRequest>(this, STOP_CALL_ID, [](auto &) {}, 15);

    // stop the module thread first
    AbstractModule::stop();

    // wait for our thread to stop, so we do not access iox objects from two threads by accident
    // in the brief period while the thread isn't shut down yet but we still receive messages
    while (!d->threadStopped) {
        std::this_thread::sleep_for(milliseconds_t(5));
        processUiEvents();
    }

    d->sentMetadata.clear();
    d->portChangesAllowed = true;

    // start reading client responses in the GUI thread again
    d->ctlEventTimer->start();
}
