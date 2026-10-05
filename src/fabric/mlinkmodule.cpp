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

#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <iox2/iceoryx2.hpp>
#include <unistd.h>

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

enum class IpcCallFlag {
    None = 0x0,
    TimeoutIsError = 0x1, /// Treat timeouts as an error
    SkipWaitOnError = 0x2 /// Do not wait for a response if the module is in an error state
};
Q_DECLARE_FLAGS(IpcCallFlags, IpcCallFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(IpcCallFlags)

/**
 * Acknowledge a request of the worker process.
 */
static void replyDoneSlice(QuillLogger *log, SliceActiveRequest &req, bool success)
{
    auto maybeResponse = req.loan_uninit();
    if (!maybeResponse.has_value()) {
        LOG_ERROR(
            log,
            "Failed to loan response for port change reply: {}",
            iox2::bb::into<const char *>(maybeResponse.error()));
        return;
    }
    if (auto res = iox2::send(std::move(maybeResponse).value().write_payload(DoneResponse{success})); !res.has_value())
        LOG_ERROR(log, "Failed to send port change reply to worker: {}", iox2::bb::into<const char *>(res.error()));
}

static void replySlice(QuillLogger *log, SliceBiDiActiveRequest &req, const ByteVector &data)
{
    auto maybeResponse = req.loan_slice_uninit(data.size());
    if (!maybeResponse.has_value()) {
        LOG_WARNING(
            log,
            "Failed to loan response slice ({} bytes): {}",
            data.size(),
            iox2::bb::into<const char *>(maybeResponse.error()));
        return;
    }
    auto rawResponse = std::move(maybeResponse).value();
    std::memcpy(rawResponse.payload_mut().data(), data.data(), data.size());
    if (auto res = iox2::send(iox2::assume_init(std::move(rawResponse))); !res.has_value())
        LOG_ERROR(log, "Failed to send response slice to worker: {}", iox2::bb::into<const char *>(res.error()));
}

namespace Syntalos
{

/**
 * @brief Receiver of the control messages a worker process sends to us.
 *
 * Control messages are handled by the main thread while no run is active, and by
 * the module's thread during a run. Both act on them through this interface.
 */
class MLinkControlSink
{
public:
    virtual ~MLinkControlSink() = default;

    virtual void error(const QString &message) = 0;
    virtual void statusMessage(const QString &message) = 0;
    virtual void stateChangeRequested(ModuleState newState) = 0;
    virtual void syncDetailsChanged(
        const std::string &id,
        const TimeSyncStrategies &strategies,
        const microseconds_t &tolerance) = 0;
    virtual void syncOffsetChanged(const std::string &id, const microseconds_t &offset) = 0;
    virtual void inputPortChangeRequested(const InputPortChangeRequest &req) = 0;
    virtual void outputPortChangeRequested(const OutputPortChangeRequest &req) = 0;

    /// The storage group the worker process may reserve names in
    virtual std::shared_ptr<EDLGroup> storageGroup() = 0;
};

/**
 * @brief The IPC endpoints on which we receive control messages from a worker process.
 *
 * iceoryx2 endpoints must not be used by two threads at once. So this object is owned by
 * exactly one thread at a time: By the main thread while no run is active, and by the
 * module's thread during a run. It is handed over when a run is started, and returned
 * once the module's thread has finished.
 */
class MLinkControlChannel
{
public:
    // Subscribers to receive information from module processes
    std::optional<IoxSubscriber<ErrorEvent>> subError;
    std::optional<IoxSubscriber<StateChangeEvent>> subStateChange;
    std::optional<IoxSubscriber<StatusMessageEvent>> subStatusMsg;
    std::optional<IoxSubscriber<SyncDetailsEvent>> subSyncDetails;
    std::optional<IoxSubscriber<SyncOffsetEvent>> subSyncOffset;
    std::optional<IoxUntypedReqServer> srvInPortChange;
    std::optional<IoxUntypedReqServer> srvOutPortChange;
    std::optional<IoxUntypedReqResServer> srvEdlReserve;

    // Listener to react to worker control events
    std::optional<IoxListener> workerCtlEventListener;

    [[nodiscard]] bool isConnected() const
    {
        return subError.has_value();
    }

    void disconnect()
    {
        subError.reset();
        subStateChange.reset();
        subStatusMsg.reset();
        subSyncDetails.reset();
        subSyncOffset.reset();
        srvInPortChange.reset();
        srvOutPortChange.reset();
        srvEdlReserve.reset();
        workerCtlEventListener.reset();
    }

    void drainListener()
    {
        if (workerCtlEventListener.has_value())
            drainListenerEvents(*workerCtlEventListener);
    }

    /**
     * Forward errors the worker process has reported.
     */
    void processErrors(MLinkControlSink &sink)
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
                sink.error(msg);
            else
                sink.error(QStringLiteral("<html><b>%1</b><br/>%2").arg(title, msg));
        }
    }

    /**
     * Process all incoming IPC data on the control channels and forward it.
     *
     * This does *not* handle the high-volume data-plane channels, which have
     * dedicated event/listener pairs for more efficient communication.
     */
    void process(MLinkControlSink &sink, QuillLogger *log)
    {
        // Drain the control event listener to keep its socket buffer clear.
        // We *must* drain at the start to immediately consume the notification that triggered this call,
        // and to prevent race conditions with new events arriving while we process the previous one.
        drainListener();

        // Error events
        processErrors(sink);

        // State changes
        if (subStateChange.has_value()) {
            while (true) {
                auto sample = safeReceive(*subStateChange);
                if (!sample.has_value())
                    break;
                const auto newState = sample->payload().state;

                // the error state must only be set by raiseError(), never directly
                if (newState == ModuleState::ERROR)
                    continue;
                sink.stateChangeRequested(newState);
            }
        }

        // Status messages
        if (subStatusMsg.has_value()) {
            while (true) {
                auto sample = safeReceive(*subStatusMsg);
                if (!sample.has_value())
                    break;
                sink.statusMessage(QString::fromUtf8(sample->payload().text.unchecked_access().c_str()));
            }
        }

        // Synchronizer notifications
        if (subSyncDetails.has_value()) {
            while (true) {
                auto sample = safeReceive(*subSyncDetails);
                if (!sample.has_value())
                    break;
                const auto &ev = sample->payload();
                const std::string id(ev.id.unchecked_access().c_str());
                const TimeSyncStrategies strategies(static_cast<int>(ev.strategies));
                const microseconds_t tolerance(ev.toleranceUsec);
                sink.syncDetailsChanged(id, strategies, tolerance);
            }
        }
        if (subSyncOffset.has_value()) {
            while (true) {
                auto sample = safeReceive(*subSyncOffset);
                if (!sample.has_value())
                    break;
                const auto &ev = sample->payload();
                const std::string id(ev.id.unchecked_access().c_str());
                sink.syncOffsetChanged(id, microseconds_t(ev.offsetUsec));
            }
        }

        // Input port change requests
        if (srvInPortChange.has_value()) {
            while (true) {
                auto req = safeReceive(*srvInPortChange);
                if (!req.has_value())
                    break;

                // deserialize
                const auto pl = req->payload();
                sink.inputPortChangeRequested(InputPortChangeRequest::fromMemory(pl.data(), pl.number_of_bytes()));

                // always acknowledge so the worker never waits too long
                replyDoneSlice(log, *req, true);
            }
        }

        // Output port change requests
        if (srvOutPortChange.has_value()) {
            while (true) {
                auto req = safeReceive(*srvOutPortChange);
                if (!req.has_value())
                    break;

                // deserialize
                const auto pl = req->payload();
                sink.outputPortChangeRequested(OutputPortChangeRequest::fromMemory(pl.data(), pl.number_of_bytes()));

                // always acknowledge so the worker never blocks indefinitely
                replyDoneSlice(log, *req, true);
            }
        }

        // EDL name-reservation requests from the worker
        if (srvEdlReserve.has_value()) {
            const auto sg = sink.storageGroup();
            while (true) {
                auto req = safeReceive(*srvEdlReserve);
                if (!req.has_value())
                    break;

                const auto pl = req->payload();
                const auto reserveReq = EdlReserveRequest::fromMemory(pl.data(), pl.number_of_bytes());

                EdlReserveReply rep;
                if (!sg) {
                    rep.success = false;
                    rep.errorMessage = "Module has no storage group assigned.";
                } else {
                    // Walk parentRelPath (slash-separated sub-group names) from the assigned root.
                    std::shared_ptr<EDLGroup> parent = sg;
                    bool walkOk = true;
                    const auto &relPath = reserveReq.parentRelPath;
                    if (!relPath.empty() && relPath != "/") {
                        std::istringstream ss(relPath);
                        std::string segment;
                        while (std::getline(ss, segment, '/')) {
                            if (segment.empty())
                                continue;
                            auto maybeGroup = parent->groupByName(segment, EDLCreateFlag::CREATE_OR_OPEN);
                            if (!maybeGroup.has_value()) {
                                rep.success = false;
                                rep.errorMessage = maybeGroup.error();
                                walkOk = false;
                                break;
                            }
                            maybeGroup.value()->setDetached(true);
                            parent = maybeGroup.value();
                        }
                    }

                    if (walkOk) {
                        if (reserveReq.kind == EdlReserveRequest::Kind::Group) {
                            auto maybeGroup = parent->groupByName(reserveReq.name, EDLCreateFlag::CREATE_OR_OPEN);
                            if (maybeGroup.has_value()) {
                                maybeGroup.value()->setDetached(true);
                                rep.success = true;
                                rep.absolutePath = maybeGroup.value()->path().string();
                            } else {
                                rep.success = false;
                                rep.errorMessage = maybeGroup.error();
                            }
                        } else {
                            auto maybeDataset = parent->datasetByName(reserveReq.name, EDLCreateFlag::MUST_CREATE);
                            if (maybeDataset.has_value()) {
                                maybeDataset.value()->setDetached(true);
                                rep.success = true;
                                rep.absolutePath = maybeDataset.value()->path().string();
                            } else {
                                rep.success = false;
                                rep.errorMessage = maybeDataset.error();
                            }
                        }
                    }
                }

                replySlice(log, *req, rep.toBytes());
            }
        }
    }
};

/**
 * @brief Forwards the data of one output port from the worker process into our data stream.
 */
struct MLinkOutPortSub {
    std::optional<SySubscriber> sub;
    std::shared_ptr<StreamOutputPort> oport;
    std::optional<IoxWaitSetGuard> guard;
};

/**
 * @brief An output stream of the module, as the thread needs to know it during a run.
 */
struct MLinkOutStreamInfo {
    std::shared_ptr<VariantDataStream> stream;
    QString portTitle;
};

/**
 * @brief Relays data and control messages of the worker process while a run is active.
 */
struct MLinkRunWorker {
    WorkerContext mod{};

    // the main thread deposits the control channel here when it starts the module,
    // and we put it back once we are done
    Guarded<std::unique_ptr<MLinkControlChannel>> ctlSlot;
    std::vector<MLinkOutPortSub> outPortSubs;
    QHash<std::string, MLinkOutStreamInfo> outStreams;
    std::shared_ptr<EDLGroup> storageGroup;
    QString moduleId;
    MainCallback<std::string, TimeSyncStrategies, microseconds_t> onSyncDetailsChanged;
    MainCallback<std::string, microseconds_t> onSyncOffsetChanged;

    void run();
};

/**
 * @brief Acts on control messages in the module's thread, while a run is active.
 *
 * The module can not be changed during a run, so requests of the worker
 * process to change its ports are refused here.
 */
class MLinkWorkerControlSink final : public MLinkControlSink
{
public:
    explicit MLinkWorkerControlSink(MLinkRunWorker &worker)
        : w(worker)
    {
    }

    void error(const QString &message) override
    {
        w.mod.raiseError(message);
    }

    void statusMessage(const QString &message) override
    {
        w.mod.setStatusMessage(message);
    }

    void stateChangeRequested(ModuleState newState) override
    {
        MLinkModule::applyStateRequest(w.mod, newState);
    }

    void syncDetailsChanged(
        const std::string &id,
        const TimeSyncStrategies &strategies,
        const microseconds_t &tolerance) override
    {
        w.onSyncDetailsChanged(id, strategies, tolerance);
    }

    void syncOffsetChanged(const std::string &id, const microseconds_t &offset) override
    {
        w.onSyncOffsetChanged(id, offset);
    }

    void inputPortChangeRequested(const InputPortChangeRequest &) override
    {
        LOG_WARNING(w.mod.log, "Input port change request ignored: No changes are allowed.");
    }

    void outputPortChangeRequested(const OutputPortChangeRequest &opc) override
    {
        if (opc.action == PortAction::ADD) {
            LOG_WARNING(w.mod.log, "Output port addition ignored: No changes are allowed.");
        } else if (opc.action == PortAction::REMOVE) {
            LOG_WARNING(w.mod.log, "Output port removal ignored: No changes are allowed.");
        } else if (opc.action == PortAction::CHANGE) {
            const auto it = w.outStreams.constFind(opc.id);
            if (it == w.outStreams.constEnd())
                return;
            it->stream->setMetadata(opc.metadata);
            it->stream->setCommonMetadata(w.moduleId, w.mod.moduleName(), it->portTitle);
        }
    }

    std::shared_ptr<EDLGroup> storageGroup() override
    {
        return w.storageGroup;
    }

private:
    MLinkRunWorker &w;
};

void MLinkRunWorker::run()
{
    // Setting up the IPC wait set needs file descriptors, which can run out with many
    // workers. Fail the module instead of the whole application, and still park on the
    // start barrier, which the engine releases once it sees the failure.
    const auto failSetup = [&](const std::string &what, const char *reason) {
        mod.raiseError(
            std::format(
                "Failed to set up the IPC event loop ({}): {}. If many modules are running, the file descriptor limit "
                "(ulimit -n) may be too low.",
                what,
                reason));
        mod.waitForStart();
        for (auto &ps : outPortSubs)
            ps.guard.reset();
    };

    // create waitset
    auto maybeWaitSet = iox2::WaitSetBuilder()
                            .signal_handling_mode(iox2::SignalHandlingMode::Disabled)
                            .create<iox2::ServiceType::Ipc>();
    if (!maybeWaitSet.has_value()) {
        failSetup("wait set", iox2::bb::into<const char *>(maybeWaitSet.error()));
        return;
    }
    auto waitSet = std::move(maybeWaitSet).value();

    // prepare guards for output port forwarding
    for (auto &ps : outPortSubs) {
        if (!ps.sub.has_value())
            continue;
        auto maybeGuard = waitSet.attach_notification(*ps.sub);
        if (!maybeGuard.has_value()) {
            failSetup(
                std::format("output port {}", ps.oport->id().toStdString()),
                iox2::bb::into<const char *>(maybeGuard.error()));
            return;
        }
        ps.guard.emplace(std::move(maybeGuard).value());
    }

    mod.waitForStart();

    // The main thread has handled the control messages until now, and gave the control channel
    // to us when it started the module. If there is none, the module was never started.
    auto ctl = ctlSlot.take();
    if (ctl != nullptr) {
        MLinkWorkerControlSink ctlSink(*this);

        // attach control guard
        std::optional<IoxWaitSetGuard> waitSetCtlGuard;
        auto maybeCtlGuard = waitSet.attach_notification(*ctl->workerCtlEventListener);
        if (maybeCtlGuard.has_value()) {
            waitSetCtlGuard.emplace(std::move(maybeCtlGuard).value());
        } else {
            mod.raiseError(
                std::format(
                    "Failed to set up the IPC event loop (control channel): {}. If many modules are running, the file "
                    "descriptor limit (ulimit -n) may be too low.",
                    iox2::bb::into<const char *>(maybeCtlGuard.error())));
        }

        auto onEvent =
            [&](const iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> &attachmentId) -> iox2::CallbackProgression {
            // handle control messages
            if (attachmentId.has_event_from(*waitSetCtlGuard)) {
                ctl->process(ctlSink, mod.log);
            } else {
                for (auto &ps : outPortSubs) {
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

        while (mod.running() && waitSetCtlGuard.has_value()) {
            // wait for data - we need to time out every once in a while to check if we are still running
            const auto res = waitSet.wait_and_process_once_with_timeout(onEvent, iox2::bb::Duration::from_millis(50));
            if (!res.has_value()) {
                mod.raiseError(std::format("IPC event loop failed: {}", iox2::bb::into<const char *>(res.error())));
                break;
            }
        }

        // MUST reset all guards before the local WaitSet goes out of scope.
        // iceoryx2 contract: "WaitSetGuard must live at most as long as the WaitSet."
        waitSetCtlGuard.reset();
        for (auto &ps : outPortSubs)
            ps.guard.reset();

        // we finished - drain incoming control messages from the module process one more time
        ctl->process(ctlSink, mod.log);

        // Drain any data the OOP process published before responding to the stop signal
        // but that the WaitSet loop hadn't yet forwarded.
        for (auto &ps : outPortSubs) {
            if (!ps.sub.has_value())
                continue;
            ps.sub->handleEvents([&ps](const IoxImmutableByteSlice &pl) {
                ps.oport->streamVar()->pushRawData(ps.oport->dataTypeId(), pl.data(), pl.number_of_bytes());
            });
        }

        // the main thread handles the control messages again from here on
        ctlSlot.set(std::move(ctl));
    }

    // disconnect forwarders
    for (auto &ps : outPortSubs) {
        ps.guard.reset();
        ps.sub->drain();
    }
    outPortSubs.clear();
}

} // namespace Syntalos

class MLinkModule::Private : public MLinkControlSink
{
public:
    Private(MLinkModule *module, QuillLogger *logger)
        : q(module),
          log(logger) {};
    ~Private() = default;

    MLinkModule *q;
    QuillLogger *log = nullptr;
    QProcess *proc = nullptr;
    ModuleWorkerMode workerMode;
    std::optional<ThreadUsageStats> lastWorkerUsage;
    std::optional<ProcessSchedInfo> lastWorkerSchedInfo;
    bool outputCaptured = false;
    QString moduleDir;
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

    // The endpoints to receive information from the module process on. We only own them
    // while no run is active: When a run is started, they are deposited in the slot for the
    // module's thread to take, which returns them there when it is done.
    std::unique_ptr<MLinkControlChannel> ctl;
    Guarded<std::unique_ptr<MLinkControlChannel>> ctlSlot;

    // Output port forwarders for the upcoming run
    std::vector<MLinkOutPortSub> outPortSubs;

    // notifier to notify the worker if we send control events.
    std::optional<IoxNotifier> ctlEventNotifier;
    QTimer *ctlEventTimer = nullptr;

    /**
     * Take the control channel back from the module's thread, once it has finished.
     */
    void reclaimControlChannel()
    {
        if (ctl == nullptr)
            ctl = ctlSlot.take();
    }

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

    void error(const QString &message) override
    {
        q->raiseError(message);
    }

    void statusMessage(const QString &message) override
    {
        q->setStatusMessage(message);
    }

    void stateChangeRequested(ModuleState newState) override
    {
        MLinkModule::applyStateRequest(q, newState);
    }

    void syncDetailsChanged(
        const std::string &id,
        const TimeSyncStrategies &strategies,
        const microseconds_t &tolerance) override
    {
        Q_EMIT q->synchronizerDetailsChanged(id, strategies, tolerance);
    }

    void syncOffsetChanged(const std::string &id, const microseconds_t &offset) override
    {
        Q_EMIT q->synchronizerOffsetChanged(id, offset);
    }

    void inputPortChangeRequested(const InputPortChangeRequest &ipc) override
    {
        if (!portChangesAllowed) {
            LOG_WARNING(log, "Input port change request ignored: No changes are allowed.");
        } else if (ipc.action == PortAction::ADD) {
            const auto portId = QString::fromStdString(ipc.id);
            const auto portTitle = QString::fromStdString(ipc.title);
            auto iport = q->inPortById(portId);
            if (iport && iport->dataTypeId() != ipc.dataTypeId) {
                q->removeInPortById(portId);
                iport = nullptr;
            }
            if (!iport)
                iport = q->registerInputPortByTypeId(ipc.dataTypeId, portId, portTitle);
            inPortIdMap[ipc.id] = iport;
        } else if (ipc.action == PortAction::REMOVE) {
            q->removeInPortById(QString::fromStdString(ipc.id));
            inPortIdMap.remove(ipc.id);
        }
    }

    void outputPortChangeRequested(const OutputPortChangeRequest &opc) override
    {
        const auto action = opc.action;
        if (action == PortAction::ADD) {
            if (!portChangesAllowed) {
                LOG_WARNING(log, "Output port addition ignored: No changes are allowed.");
            } else {
                // only register a new output port if we don't have one with that ID already
                const auto portId = QString::fromStdString(opc.id);
                const auto portTitle = QString::fromStdString(opc.title);
                auto oport = q->outPortById(portId);
                std::shared_ptr<VariantDataStream> ostream;
                if (oport) {
                    if (oport->dataTypeId() != opc.dataTypeId) {
                        q->removeOutPortById(portId);
                        oport = nullptr;
                    } else {
                        ostream = oport->streamVar();
                    }
                }
                if (!ostream)
                    ostream = q->registerOutputPortByTypeId(opc.dataTypeId, portId, portTitle);
                ostream->setMetadata(opc.metadata);
                outPortIdMap[opc.id] = ostream;
            }
        } else if (action == PortAction::REMOVE) {
            if (!portChangesAllowed) {
                LOG_WARNING(log, "Output port removal ignored: No changes are allowed.");
            } else {
                q->removeOutPortById(QString::fromStdString(opc.id));
                outPortIdMap.remove(opc.id);
            }
        } else if (action == PortAction::CHANGE) {
            std::shared_ptr<VariantDataStream> ostream;
            if (outPortIdMap.contains(opc.id))
                ostream = outPortIdMap.value(opc.id);
            else if (auto oport = q->outPortById(qstr(opc.id)))
                ostream = oport->streamVar();
            if (ostream) {
                ostream->setMetadata(opc.metadata);
                q->updateCommonStreamMetadata();
            }
        }
    }

    std::shared_ptr<EDLGroup> storageGroup() override
    {
        return q->storageGroup();
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
        IpcCallFlags flags = IpcCallFlag::TimeoutIsError)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return std::nullopt;
        }

        auto client = makeTypedClient<Req, Res>(*node, svcName(channel));

        auto maybeReq = client.loan_uninit();
        if (!maybeReq.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to loan shared memory for request on channel '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(maybeReq.error())));
            return std::nullopt;
        }
        auto pendingReq = std::move(maybeReq).value();

        fillReqFn(pendingReq.payload_mut());
        auto sendRes = iox2::send(iox2::assume_init(std::move(pendingReq)));
        if (!sendRes.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to send request on channel '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(sendRes.error())));
            return std::nullopt;
        }
        auto pending = std::move(sendRes).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            // during a run, the module's thread owns the control channel and reports errors
            if (ctl != nullptr)
                ctl->processErrors(*this);

            auto maybeResponse = pending.receive();
            if (!maybeResponse.has_value()) {
                self->raiseError(
                    std::format(
                        "Failed to receive response on channel '{}': {}",
                        channel,
                        iox2::bb::into<const char *>(maybeResponse.error())));
                return std::nullopt;
            }
            auto response = std::move(maybeResponse).value();
            if (response.has_value())
                return response->payload();

            // quit immediately if an error was already emitted
            if (flags.testFlag(IpcCallFlag::SkipWaitOnError) && self->state() == ModuleState::ERROR)
                return std::nullopt;

            // if we stopped running (crashed or existed) we no longer need to wait
            if (!self->isProcessRunning())
                return std::nullopt;

            if (timer.elapsed() > timeoutSec * 1000) {
                if (flags.testFlag(IpcCallFlag::TimeoutIsError))
                    self->raiseError(std::format("Timeout while waiting for response on: {}", channel));
                return std::nullopt;
            }

            // Pump worker-initiated port-change requests from the main thread only while
            // we own the control channel. During a run, the module's thread has it and
            // handles these requests itself via its WaitSet.
            if (ctl != nullptr) {
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
        IpcCallFlags flags = IpcCallFlag::TimeoutIsError)
    {
        auto res = callClientSimple<Req, DoneResponse>(self, channel, fillReqFn, timeoutSec, flags);
        if (!res.has_value())
            return false;
        return res->success;
    }

    template<typename ReqData>
    bool callSliceClientSimple(
        MLinkModule *self,
        const std::string &channel,
        const ReqData &reqEntity,
        int timeoutSec = 5,
        IpcCallFlags flags = IpcCallFlag::TimeoutIsError)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return false;
        }

        auto client = makeSliceClient<DoneResponse>(*node, svcName(channel));

        const auto bytes = reqEntity.toBytes();
        auto maybeSlice = client.loan_slice_uninit(static_cast<uint64_t>(bytes.size()));
        if (!maybeSlice.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to loan shared memory for request on '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(maybeSlice.error())));
            return false;
        }
        auto rawSlice = std::move(maybeSlice).value();
        std::memmove(rawSlice.payload_mut().data(), bytes.data(), bytes.size());
        auto sendRes = iox2::send(iox2::assume_init(std::move(rawSlice)));
        if (!sendRes.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to send request on '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(sendRes.error())));
            return false;
        }
        auto pending = std::move(sendRes).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            // during a run, the module's thread owns the control channel and reports errors
            if (ctl != nullptr)
                ctl->processErrors(*this);
            if (ctl != nullptr) {
                self->handleIncomingControl();
                if (timeoutSec > 4)
                    qApp->processEvents();
            }
            auto maybeResponse = pending.receive();
            if (!maybeResponse.has_value()) {
                self->raiseError(
                    std::format(
                        "Failed to receive response on '{}': {}",
                        channel,
                        iox2::bb::into<const char *>(maybeResponse.error())));
                return false;
            }
            auto response = std::move(maybeResponse).value();
            if (response.has_value())
                return response->payload().success;

            // quit immediately if an error was already emitted
            if (flags.testFlag(IpcCallFlag::SkipWaitOnError) && self->state() == ModuleState::ERROR)
                return false;

            // if we stopped running (crashed or exited) we no longer need to wait
            if (!self->isProcessRunning())
                return false;

            if (timer.elapsed() > timeoutSec * 1000) {
                if (flags.testFlag(IpcCallFlag::TimeoutIsError))
                    self->raiseError(std::format("Timeout while waiting for response on: {}", channel));
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
        int timeoutSec = 5,
        IpcCallFlags flags = IpcCallFlag::TimeoutIsError)
    {
        if (!node.has_value()) {
            LOG_CRITICAL(log, "callClientSimple: IOX node not initialized, failing call on channel: {}", channel);
            return std::nullopt;
        }

        auto client = makeSliceClient<IoxByteSlice>(*node, svcName(channel));

        const auto bytes = reqEntity.toBytes();
        auto maybeSlice = client.loan_slice_uninit(static_cast<uint64_t>(bytes.size()));
        if (!maybeSlice.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to loan shared memory for request on '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(maybeSlice.error())));
            return std::nullopt;
        }
        auto rawSlice = std::move(maybeSlice).value();
        std::memmove(rawSlice.payload_mut().data(), bytes.data(), bytes.size());
        auto sendRes = iox2::send(iox2::assume_init(std::move(rawSlice)));
        if (!sendRes.has_value()) {
            self->raiseError(
                std::format(
                    "Failed to send request on '{}': {}",
                    channel,
                    iox2::bb::into<const char *>(sendRes.error())));
            return std::nullopt;
        }
        auto pending = std::move(sendRes).value();
        notifyClient();

        QElapsedTimer timer;
        timer.start();
        while (true) {
            // during a run, the module's thread owns the control channel and reports errors
            if (ctl != nullptr)
                ctl->processErrors(*this);
            if (ctl != nullptr) {
                self->handleIncomingControl();
                if (timeoutSec > 4)
                    qApp->processEvents();
            }

            auto maybeResponse = pending.receive();
            if (!maybeResponse.has_value()) {
                self->raiseError(
                    std::format(
                        "Failed to receive response on '{}': {}",
                        channel,
                        iox2::bb::into<const char *>(maybeResponse.error())));
                return std::nullopt;
            }
            auto response = std::move(maybeResponse).value();
            if (response.has_value()) {
                const auto pl = response->payload();
                return ResData::fromMemory(pl.data(), pl.number_of_bytes());
            }

            // quit immediately if an error was already emitted
            if (flags.testFlag(IpcCallFlag::SkipWaitOnError) && self->state() == ModuleState::ERROR)
                return std::nullopt;

            // if we stopped running (crashed or exited) we no longer need to wait
            if (!self->isProcessRunning())
                return std::nullopt;

            if (timer.elapsed() > timeoutSec * 1000) {
                if (flags.testFlag(IpcCallFlag::TimeoutIsError))
                    self->raiseError(std::format("Timeout while waiting for response on: {}", channel));
                return std::nullopt;
            }

            std::this_thread::sleep_for(microseconds_t(5 * timeoutSec));
        }
    }
};

MLinkModule::MLinkModule(QObject *parent)
    : AbstractModule(parent),
      d(new MLinkModule::Private(this, m_log))
{
    d->proc = new QProcess(this);
    // Run the worker in its own process group, so a Ctrl+C in the terminal only reaches Syntalos,
    // which then stops its workers in an orderly fashion. If Syntalos dies, PDEATHSIG ends them.
    d->proc->setChildProcessModifier([]() {
        setpgid(0, 0);
    });
    d->workerMode = ModuleWorkerMode::PERSISTENT;
    d->portChangesAllowed = true;

    // merge stdout/stderr of external process with ours by default
    setOutputCaptured(false);

    connect(d->proc, &QProcess::readyReadStandardOutput, this, [this]() {
        if (d->outputCaptured)
            Q_EMIT processOutputReceived(ChannelStdout, readProcessOutput(ChannelStdout));
    });
    connect(d->proc, &QProcess::readyReadStandardError, this, [this]() {
        if (d->outputCaptured)
            Q_EMIT processOutputReceived(ChannelStderr, readProcessOutput(ChannelStderr));
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

std::expected<void, QString> MLinkModule::initialize()
{
    // propagate the (potentially updated) logger
    d->log = m_log;

    if (moduleBinary().isEmpty()) {
        return std::unexpected(QStringLiteral("Unable to find module binary. Is the module installed correctly?"));
    }

    // Ensure the module process is running. This will also call resetConnection()
    // to initialize the connection at this point, since only now do we know the
    // module ID and index and can react to & recover from errors properly.
    if (!runProcess())
        return std::unexpected(QStringLiteral("Unable to start the worker process of this module."));

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
    if (!d->ctl || !d->ctl->isConnected())
        return false;

    auto modApiTagResponse = d->callClientSimple<ApiVersionRequest, ApiVersionResponse>(
        this,
        API_VERSION_CALL_ID,
        [&](auto &) {});
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
    // do nothing if the error channel does not exist yet, or if the module's
    // thread currently owns the control channel and handles the messages itself
    if (!d->ctl || !d->ctl->isConnected())
        return;

    d->ctl->process(*d, m_log);
}

void MLinkModule::applyStateRequest(AbstractModule *mod, ModuleState newState)
{
    // the error state must only be set by raiseError(), never directly
    if (newState == ModuleState::ERROR)
        return;

    // only the engine / user interaction is allowed to clear a module error state
    if (mod->state() == ModuleState::ERROR)
        return;

    // only some states are allowed to be set by the module
    if (newState == ModuleState::DORMANT || newState == ModuleState::READY || newState == ModuleState::INITIALIZING
        || newState == ModuleState::IDLE) {
        LOG_DEBUG(
            mod->m_log,
            "Client state change request granted: {} → {}",
            toString(mod->state()),
            toString(newState));
        mod->setState(newState);
    }
}

void MLinkModule::applyStateRequest(const WorkerContext &ctx, ModuleState newState)
{
    if (ctx.m_mod != nullptr)
        applyStateRequest(ctx.m_mod, newState);
}

void MLinkModule::resetConnection()
{
    d->clientId = QStringLiteral("%1_%2").arg(id()).arg(index()).toStdString();

    // create a fresh node for this module connection
    // SIGINT/SIGTERM are handled by Syntalos itself (see termsignalwatcher.h), not by iceoryx2
    d->node.emplace(makeIoxNode("syntalos-master-" + d->clientId, iox2::SignalHandlingMode::Disabled));

    // The connection is never reset while a run is active, so if we do not have the control
    // channel, the module's thread of a previous run has left it in the hand-over slot.
    d->reclaimControlChannel();
    if (d->ctl == nullptr)
        d->ctl = std::make_unique<MLinkControlChannel>();
    auto &ctl = *d->ctl;

    // ensure the old connections are gone before we are trying to create new ones
    ctl.disconnect();
    d->ctlEventNotifier.reset();

    // (re)create subscribers/servers for client -> master data channels
    ctl.subError.emplace(makeTypedSubscriber<ErrorEvent>(*d->node, d->svcName(ERROR_CHANNEL_ID)));
    ctl.subStateChange.emplace(makeTypedSubscriber<StateChangeEvent>(*d->node, d->svcName(STATE_CHANNEL_ID)));
    ctl.subStatusMsg.emplace(makeTypedSubscriber<StatusMessageEvent>(*d->node, d->svcName(STATUS_MESSAGE_CHANNEL_ID)));
    ctl.subSyncDetails.emplace(makeTypedSubscriber<SyncDetailsEvent>(*d->node, d->svcName(SYNC_DETAILS_CHANNEL_ID)));
    ctl.subSyncOffset.emplace(makeTypedSubscriber<SyncOffsetEvent>(*d->node, d->svcName(SYNC_OFFSET_CHANNEL_ID)));
    ctl.srvInPortChange.emplace(makeSliceServer(*d->node, d->svcName(IN_PORT_CHANGE_CHANNEL_ID)));
    ctl.srvOutPortChange.emplace(makeSliceServer(*d->node, d->svcName(OUT_PORT_CHANGE_CHANNEL_ID)));
    ctl.srvEdlReserve.emplace(makeSliceServer<IoxByteSlice>(*d->node, d->svcName(EDL_RESERVE_CALL_ID)));

    // control listener: Called when the client publishes a control command
    ctl.workerCtlEventListener.emplace(ipc::makeEventListener(*d->node, d->svcName(WORKER_CTL_EVENT_ID)));
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

QString MLinkModule::moduleDir() const
{
    return d->moduleDir;
}

void MLinkModule::setModuleDir(const QString &dir)
{
    d->moduleDir = dir;
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
        d->proc->setProcessChannelMode(QProcess::SeparateChannels);
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
        LOG_WARNING(m_log, "Tried to send settings data to dead module process.");
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
        this,
        SAVE_SETTINGS_CALL_ID,
        ssReq,
        15);
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
    if (!d->callClientSimple<ShowDisplayRequest>(this, SHOW_DISPLAY_CALL_ID, [](auto &) {})) {
        LOG_WARNING(m_log, "Show display request failed!");
        return;
    }

    // if a show-display request worked, we are allowed to clear the error state
    if (state() == ModuleState::ERROR)
        setState(ModuleState::IDLE);
}

void MLinkModule::showSettingsUi()
{
    // pick up any recently saved settings before we hand them back to the worker UI
    handleIncomingControl();

    if (!d->callClientSimple<ShowSettingsRequest>(this, SHOW_SETTINGS_CALL_ID, [](auto &) {})) {
        LOG_WARNING(m_log, "Request to show settings UI has failed!");
        handleIncomingControl();
        return;
    }

    // if a show-settings request worked, we are allowed to clear the error state
    if (state() == ModuleState::ERROR)
        setState(ModuleState::IDLE);

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
        5,                // timeout seconds
        IpcCallFlag::None // timeout is not an error and we do not fast-exit if the module is in an error-state
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
    if (d->ctl != nullptr)
        d->ctl->drainListener();
}

bool MLinkModule::runProcess()
{
    // ensure any existing process does not exist
    terminateProcess();

    // reset connection, just in case we changed our ID
    try {
        resetConnection();
    } catch (const std::exception &e) {
        raiseError(std::format("Failed to set up module IPC connection: {}", e.what()));
        return false;
    }

    if (d->proc->program().isEmpty()) {
        LOG_ERROR(m_log, "MLink module has not set a worker binary");
        return false;
    }

    auto penv = moduleBinaryEnv();
    penv.insert("SYNTALOS_VERSION", syntalosVersionFull());
    penv.insert("SYNTALOS_MODULE_ID", d->clientId.c_str());
    penv.insert("SYNTALOS_MODULE_TYPE_ID", id());
    if (!d->moduleDir.isEmpty())
        penv.insert("SYNTALOS_MODULE_DIR", d->moduleDir);
    if (!d->pyVenvDir.isEmpty()) {
        penv.remove("PYTHONHOME");
        penv.insert("VIRTUAL_ENV", d->pyVenvDir);
        penv.insert("PATH", QStringLiteral("%1/bin/:%2").arg(d->pyVenvDir, penv.value("PATH", "")));
    }

    // When launching the external process, we are back at initialization
    // If we are in an error state, we clear it and return to IDLE after a restart.
    auto prevState = (state() == ModuleState::ERROR) ? ModuleState::IDLE : state();
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
        handleIncomingControl();

        if (!workerFound && state() != prevState)
            workerFound = true;
        if (state() == ModuleState::IDLE)
            moduleInitDone = true;

        if (!workerFound || !moduleInitDone)
            std::this_thread::sleep_for(microseconds_t(1500));

        if (timer.elapsed() > 10 * 1000)
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

qint64 MLinkModule::workerProcessId() const
{
    if (d->proc == nullptr || d->proc->state() != QProcess::Running)
        return 0;
    return d->proc->processId();
}

std::optional<ThreadUsageStats> MLinkModule::lastWorkerUsage() const
{
    return d->lastWorkerUsage;
}

std::optional<ProcessSchedInfo> MLinkModule::lastWorkerSchedInfo() const
{
    return d->lastWorkerSchedInfo;
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

QString MLinkModule::readProcessOutput(OutChannelType channel)
{
    if (!d->outputCaptured)
        return {};
    if (channel == ChannelAll)
        return d->proc->readAll();
    if (channel == ChannelStdout)
        return d->proc->readAllStandardOutput();
    if (channel == ChannelStderr)
        return d->proc->readAllStandardError();
    return {};
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
            iport->id().toUtf8().constData(),
            iport->id().toUtf8().size());
        req.instanceId = IoxServiceNameString::from_utf8_null_terminated_unchecked_truncated(
            details->instanceId.toUtf8().constData(),
            details->instanceId.toUtf8().size());
        req.channelId = IoxServiceNameString::from_utf8_null_terminated_unchecked_truncated(
            details->channelId.toUtf8().constData(),
            details->channelId.toUtf8().size());
        req.topology = makeIpcServiceTopology(1, iport->outPort()->streamVar()->subscriberCount());
        // Tell the worker which native type the wire bytes are serialized as, so it
        // can deserialize-and-convert when its declared input type is a different
        // (but compatible) type.
        req.sourceTypeId = static_cast<int32_t>(iport->outPort()->streamVar()->dataTypeId());

        bool ret = d->callClientSimple<ConnectInputRequest>(this, CONNECT_INPUT_CALL_ID, [&req](auto &payload) {
            payload = req;
        });
        if (!ret)
            LOG_WARNING(m_log, "Failed to connect exported input port {}", iport->title());
    }
}

static void ipcLogDispatch(QuillLogger *logger, datactl::LogSeverity severity, const std::string &msg)
{
    LOG_DYNAMIC(logger, static_cast<quill::LogLevel>(severity), "{}", msg);
}

bool MLinkModule::registerOutPortForwarders()
{
    // ensure we are disconnected
    shutdownOutputPorts();

    // connect to external process streams
    for (auto &oport : outPorts()) {
        if (!oport->streamVar()->hasSubscribers())
            continue;

        // If every connected input port belongs to another MLink module, those workers
        // subscribe directly via ConnectInputRequest and the master-side forwarder is
        // not needed - skip creating it to avoid receiving and immediately discarding data.
        const auto &subPorts = oport->subscriberPorts();
        const bool allMLink = !subPorts.isEmpty()
                              && std::all_of(subPorts.cbegin(), subPorts.cend(), [](const VarStreamInputPort *ip) {
                                     return dynamic_cast<const MLinkModule *>(ip->owner()) != nullptr;
                                 });
        if (allMLink) {
            LOG_INFO(m_log, "All destinations on {} are MLink, skipping forwarder registration.", oport->id());
            continue;
        }

        // If every connected input port belongs to a module that will not run (disabled, or marked
        // dormant by the engine before PREPARE), nobody will ever consume the forwarded data - skip
        // the forwarder so we don't receive frames from the worker just to drop them again.
        const bool allDormant = !subPorts.isEmpty()
                                && std::all_of(subPorts.cbegin(), subPorts.cend(), [](const VarStreamInputPort *ip) {
                                       const auto *owner = ip->owner();
                                       return owner->state() == ModuleState::DORMANT
                                              || !owner->modifiers().testFlag(ModuleModifier::ENABLED);
                                   });
        if (allDormant) {
            LOG_INFO(m_log, "All destinations on {} are dormant, skipping forwarder registration.", oport->id());
            continue;
        }

        MLinkOutPortSub ps;
        const auto topology = makeIpcServiceTopology(1, oport->streamVar()->subscriberCount());
        try {
            ps.sub.reset(); // ensure the old subscription is gone before we try to create a new one
            ps.sub.emplace(
                SySubscriber::create(
                    *d->node,
                    d->clientId,
                    "o/" + oport->id().toStdString(),
                    topology,
                    [this](auto severity, auto &msg) {
                        ipcLogDispatch(m_log, severity, msg);
                    }));
            ps.oport = oport;
            d->outPortSubs.push_back(std::move(ps));
        } catch (const std::exception &e) {
            raiseError(QStringLiteral("Failed to connect output port '%1': %2").arg(oport->title(), e.what()));
            return false;
        }

        // NOTE: oport->startStream() is intentionally NOT called here.
        // It is called in MLinkModule::start() by iterating *all* output ports
        // (not just those with a forwarder). The pre-start metadata snapshot is
        // taken at the end of MLinkModule::prepare() the same way, over all
        // output ports, after all OutputPortChange messages from the worker's
        // prepare() have been processed by handleIncomingControl().
    }

    return true;
}

void MLinkModule::shutdownOutputPorts()
{
    // stop all output streams (regardless of whether they have a forwarder)
    for (auto &oport : outPorts())
        oport->stopStream();

    // drain the IPC forwarder subscribers
    for (auto &ps : d->outPortSubs)
        ps.sub->drain();
    d->outPortSubs.clear();
}

bool MLinkModule::prepare(const RunInfo &info)
{
    // usage data from a previous run must not leak into the statistics of this one
    d->lastWorkerUsage.reset();
    d->lastWorkerSchedInfo.reset();

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

    // set module process realtime priority, but only if the engine approved this
    // module for realtime scheduling within the shared RtKit budget (otherwise the
    // worker would self-elevate past the limit). A priority of 0 disables realtime.
    if (!d->callClientSimple<SetMaxRealtimePriority>(this, SET_MAX_RT_PRIORITY_CALL_ID, [&](auto &req) {
            req.priority = isRealtimeApproved() ? defaultRealtimePriority() : 0;
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
    const auto sg = storageGroup();
    PrepareRunRequest prepReq{
        .runUuid = info.uuid,
        .subjectId = info.subject.id.toStdString(),
        .subjectGroup = info.subject.group.toStdString(),
        .experimentId = info.experimentId.toStdString(),
        .isEphemeral = info.isEphemeral,
        .edlRootPath = sg ? sg->path().string() : std::string{},
        .moduleName = name().toStdString(),
    };
    if (!d->callSliceClientSimple(
            this,
            PREPARE_RUN_CALL_ID,
            prepReq,
            15,
            IpcCallFlag::TimeoutIsError | IpcCallFlag::SkipWaitOnError))
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

    d->portChangesAllowed = false;

    // Everything is set up, so we can create the worker that relays data and control
    // messages in our thread while the run is active.
    QHash<std::string, MLinkOutStreamInfo> outStreams;
    for (const auto &oport : outPorts())
        outStreams.insert(oport->id().toStdString(), MLinkOutStreamInfo{oport->streamVar(), oport->title()});
    for (auto it = d->outPortIdMap.constBegin(); it != d->outPortIdMap.constEnd(); ++it) {
        if (!outStreams.contains(it.key()))
            outStreams.insert(it.key(), MLinkOutStreamInfo{it.value(), QString()});
    }

    setWorker(
        MLinkRunWorker{
            .ctlSlot = d->ctlSlot,
            .outPortSubs = std::move(d->outPortSubs),
            .outStreams = std::move(outStreams),
            .storageGroup = storageGroup(),
            .moduleId = id(),
            .onSyncDetailsChanged = mainCallback([this](
                                                     const std::string &syncId,
                                                     const TimeSyncStrategies &strategies,
                                                     const microseconds_t &tolerance) {
                Q_EMIT synchronizerDetailsChanged(syncId, strategies, tolerance);
            }),
            .onSyncOffsetChanged = mainCallback([this](const std::string &syncId, const microseconds_t &offset) {
                Q_EMIT synchronizerOffsetChanged(syncId, offset);
            }),
        });
    d->outPortSubs.clear();

    return true;
}

void MLinkModule::start()
{
    d->portChangesAllowed = false;

    // timestamp when this module was started
    auto startTimestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                m_syTimer->currentTimePoint().time_since_epoch())
                                .count();

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
        if (!d->callSliceClientSimple(
                this,
                IN_PORT_UPDATE_METADATA_ID,
                req,
                5,
                IpcCallFlag::TimeoutIsError | IpcCallFlag::SkipWaitOnError))
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

    // Stop reading control events in the GUI thread, and hand the control channel over to
    // the module thread: It takes it as soon as the run is started, and handles all control
    // messages until the run has ended.
    d->ctlEventTimer->stop();
    if (d->ctl != nullptr)
        d->ctlSlot.set(std::move(d->ctl));

    // Start all output streams, regardless of whether they have a master-side IPC
    // forwarder. This re-snapshots the (now-final and immutable) metadata into all
    // output-port subscriptions, including pure MLink->MLink ports that have no
    // forwarder entry in outPortSubs. Destination MLink workers read metadata from
    // the in-process subscription via UpdateInputPortMetadataRequest and need it set.
    for (auto &oport : outPorts()) {
        if (oport->streamVar()->hasSubscribers())
            oport->startStream();
    }

    // call generic
    AbstractModule::start();
}

void MLinkModule::preStop()
{
    // Our thread is still running at this point and forwards any data the worker
    // still sends, until the worker has acknowledged the request to stop.
    if (isProcessRunning()) {
        // remember what the worker consumed during this run and which priority it ran at,
        // for the run statistics (transient workers exit on their own after the stop request)
        d->lastWorkerUsage = readProcessUsage(d->proc->processId());
        d->lastWorkerSchedInfo = readProcessSchedInfo(d->proc->processId());

        d->callClientSimple<StopRequest>(this, STOP_CALL_ID, [](auto &) {}, 15);
    }
}

void MLinkModule::stop()
{
    AbstractModule::stop();

    // our thread has finished, so we handle the control messages again
    d->reclaimControlChannel();

    d->sentMetadata.clear();
    d->portChangesAllowed = true;

    // start reading client responses in the GUI thread again
    d->ctlEventTimer->start();
}
