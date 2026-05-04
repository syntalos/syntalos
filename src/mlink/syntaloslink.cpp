/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "syntaloslink.h"

#include "modconfig.h"
#include <glib.h>
#include <csignal>
#include <cstring>
#include <sys/prctl.h>
#include <iox2/iceoryx2.hpp>

#include "mlink/ipc-types-private.h"
#include "mlink/ipc-iox-private.h"
#include "datactl/priv/rtkit.h"
#include "datactl/priv/cpuaffinity.h"
#include "datactl/loginternal.h"

using namespace Syntalos;
using namespace Syntalos::ipc;

namespace
{

constexpr int MAIN_CONTEXT_MAX_ITER_PER_TICK = 32;

void iterateDefaultMainContextNonBlocking()
{
    // keep timers and other GLib sources responsive without starving IPC handling
    auto *const mainContext = g_main_context_default();
    if (!mainContext)
        return;

    for (int i = 0; i < MAIN_CONTEXT_MAX_ITER_PER_TICK; ++i) {
        if (!g_main_context_iteration(mainContext, FALSE))
            break;
    }
}

} // namespace

namespace Syntalos
{

SY_DEFINE_LOG_CATEGORY(logSyLink, "sylink");

/**
 * Safely receive from a subscriber on the client.
 * Returns the inner optional (empty = no data available) and logs a warning
 * instead of crashing when the receive itself fails.
 */
template<typename Sub>
static auto safeReceive(Sub &sub) -> std::remove_cvref_t<decltype(sub.receive().value())>
{
    auto result = sub.receive();
    if (!result.has_value()) {
        SY_LOG_ERROR(logSyLink, "Client IPC receive failed: {}", iox2::bb::into<const char *>(result.error()));
        return {};
    }
    return std::move(result).value();
}

static std::string getenvSafe(const char *name)
{
    if (const char *value = std::getenv(name))
        return value;
    return {};
}

std::unique_ptr<SyntalosLink> initSyntalosModuleLink(const ModuleInitOptions &optn)
{
    // we should obtain the PID of Syntalos here
    pid_t parentPid = getppid();

    std::string syModuleId = getenvSafe("SYNTALOS_MODULE_ID");
    if (syModuleId.empty() || syModuleId.length() < 2)
        throw std::runtime_error("This module was not run by Syntalos, can not continue!");

    // set the process name to the instance ID, to simplify identification in process trees
    if (optn.renameThread) {
        // PR_SET_NAME allows max 16 bytes including terminating NUL
        const auto procName = syModuleId.substr(0, 15);
        prctl(PR_SET_NAME, procName.c_str(), 0, 0, 0);
        std::ofstream("/proc/self/comm") << procName;
    }

    // set up stream data type mapping, if it hasn't been initialized yet
    registerStreamMetaTypes();

    // set IOX log level
    auto verboseLevel = getenvSafe("SY_VERBOSE");
    if (verboseLevel == "1")
        iox2::set_log_level(iox2::LogLevel::Debug);
    else
        iox2::set_log_level(iox2::LogLevel::Info);

    // ensure we (try to) die if Syntalos, our parent, dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // race check: parent may have died before prctl().
    if (getppid() != parentPid) {
        raise(SIGTERM);
    }

    return std::unique_ptr<SyntalosLink>(new SyntalosLink(syModuleId));
}

/**
 * Reference for a module input port
 */
class InputPortInfo::Private
{
public:
    explicit Private(const InputPortChangeRequest &pc)
        : index(0),
          connected(false),
          id(pc.id),
          title(pc.title),
          dataTypeId(pc.dataTypeId),
          metadata(pc.metadata),
          throttleItemsPerSec(0)
    {
    }

    int index;
    bool connected;
    std::optional<SySubscriber> ioxSub;
    std::optional<IoxWaitSetGuard> ioxGuard;

    std::string id;
    std::string title;
    int dataTypeId;
    MetaStringMap metadata;

    NewDataRawFn newDataCb;
    uint throttleItemsPerSec;
};

InputPortInfo::InputPortInfo(const InputPortChangeRequest &pc)
    : d(new InputPortInfo::Private(pc))
{
}

std::string InputPortInfo::id() const
{
    return d->id;
}

int InputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

std::string InputPortInfo::title() const
{
    return d->title;
}

void InputPortInfo::setNewDataRawCallback(NewDataRawFn callback)
{
    d->newDataCb = std::move(callback);
}

void InputPortInfo::setThrottleItemsPerSec(uint itemsPerSec)
{
    d->throttleItemsPerSec = itemsPerSec;
}

std::optional<MetaValue> InputPortInfo::metadataValue(const std::string &key) const
{
    return d->metadata.value(key);
}

MetaValue InputPortInfo::metadataValueOr(const std::string &key, const MetaValue &defaultVal) const
{
    const auto val = d->metadata.value(key);
    if (val.has_value())
        return *val;
    return defaultVal;
}

MetaStringMap InputPortInfo::metadata() const
{
    return d->metadata;
}

/**
 * Reference for a module output port
 */
class OutputPortInfo::Private
{
public:
    explicit Private(const OutputPortChangeRequest &pc)
        : index(0),
          connected(false),
          id(pc.id),
          title(pc.title),
          dataTypeId(pc.dataTypeId),
          metadata(pc.metadata)
    {
    }

    int index;
    bool connected;
    std::optional<SyPublisher> ioxPub;
    std::optional<IoxWaitSetGuard> ioxGuard;

    std::string id;
    std::string title;
    int dataTypeId;
    MetaStringMap metadata;

    // utilized to reuse allocated memory when sending data, to prevent fragmentation
    ByteVector outBuffer;

    [[nodiscard]] std::string ipcChannelId() const
    {
        return "o/" + id;
    }
};

OutputPortInfo::OutputPortInfo(const OutputPortChangeRequest &pc)
    : d(new OutputPortInfo::Private(pc))
{
}

std::string OutputPortInfo::id() const
{
    return d->id;
}

int OutputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

void OutputPortInfo::setMetadataVar(const std::string &key, const MetaValue &value)
{
    d->metadata[key] = value;
}

class SyntalosLink::Private
{
public:
    Private(const std::string &instanceId)
        : modId(instanceId),
          state(ModuleState::UNKNOWN),
          maxRTPriority(0),
          syTimer(nullptr),
          shutdownPending(false)
    {
        // make a new node for this module
        node.emplace(makeIoxNode(modId));

        // interfaces
        pubError.emplace(makeTypedPublisher<ErrorEvent>(*node, svcName(ERROR_CHANNEL_ID)));
        pubState.emplace(makeTypedPublisher<StateChangeEvent>(*node, svcName(STATE_CHANNEL_ID)));
        pubStatusMsg.emplace(makeTypedPublisher<StatusMessageEvent>(*node, svcName(STATUS_MESSAGE_CHANNEL_ID)));

        cltInPortChange.emplace(makeSliceClient(*node, svcName(IN_PORT_CHANGE_CHANNEL_ID)));
        cltOutPortChange.emplace(makeSliceClient(*node, svcName(OUT_PORT_CHANGE_CHANNEL_ID)));
        cltEdlReserve.emplace(makeSliceClient<IoxByteSlice>(*node, svcName(EDL_RESERVE_CALL_ID)));

        srvApiVersion.emplace(
            makeTypedServer<ApiVersionRequest, ApiVersionResponse>(*node, svcName(API_VERSION_CALL_ID)));
        srvSetNiceness.emplace(makeTypedServer<SetNicenessRequest, DoneResponse>(*node, svcName(SET_NICENESS_CALL_ID)));
        srvSetMaxRTPriority.emplace(
            makeTypedServer<SetMaxRealtimePriority, DoneResponse>(*node, svcName(SET_MAX_RT_PRIORITY_CALL_ID)));
        srvSetCPUAffinity.emplace(
            makeTypedServer<SetCPUAffinityRequest, DoneResponse>(*node, svcName(SET_CPU_AFFINITY_CALL_ID)));
        srvConnectIPort.emplace(
            makeTypedServer<ConnectInputRequest, DoneResponse>(*node, svcName(CONNECT_INPUT_CALL_ID)));
        srvStart.emplace(makeTypedServer<StartRequest, DoneResponse>(*node, svcName(START_CALL_ID)));
        srvStop.emplace(makeTypedServer<StopRequest, DoneResponse>(*node, svcName(STOP_CALL_ID)));
        srvShutdown.emplace(makeTypedServer<ShutdownRequest, DoneResponse>(*node, svcName(SHUTDOWN_CALL_ID)));
        srvShowDisplay.emplace(makeTypedServer<ShowDisplayRequest, DoneResponse>(*node, svcName(SHOW_DISPLAY_CALL_ID)));
        srvShowSettings.emplace(
            makeTypedServer<ShowSettingsRequest, DoneResponse>(*node, svcName(SHOW_SETTINGS_CALL_ID)));
        srvLoadScript.emplace(makeSliceServer(*node, svcName(LOAD_SCRIPT_CALL_ID)));
        srvSetPortsPreset.emplace(makeSliceServer(*node, svcName(SET_PORTS_PRESET_CALL_ID)));
        srvUpdateIPortMetadata.emplace(makeSliceServer(*node, svcName(IN_PORT_UPDATE_METADATA_ID)));
        srvSaveSettings.emplace(makeSliceServer<IoxByteSlice>(*node, svcName(SAVE_SETTINGS_CALL_ID)));
        srvLoadSettings.emplace(makeSliceServer(*node, svcName(LOAD_SETTINGS_CALL_ID)));
        srvPrepareRun.emplace(makeSliceServer(*node, svcName(PREPARE_RUN_CALL_ID)));

        // control event notifications
        masterCtlEventListener.emplace(makeEventListener(*node, svcName(MASTER_CTL_EVENT_ID)));
        ctlEventNotifier.emplace(makeEventNotifier(*node, svcName(WORKER_CTL_EVENT_ID)));
    }

    ~Private() = default;

    std::optional<iox2::Node<iox2::ServiceType::Ipc>> node;
    std::string modId;

    // Publishers: Module process -> Syntalos master
    std::optional<IoxPublisher<ErrorEvent>> pubError;
    std::optional<IoxPublisher<StateChangeEvent>> pubState;
    std::optional<IoxPublisher<StatusMessageEvent>> pubStatusMsg;

    // Clients: Module -> Syntalos master
    std::optional<IoxUntypedClient> cltInPortChange;
    std::optional<IoxUntypedClient> cltOutPortChange;
    std::optional<IoxUntypedReqResClient> cltEdlReserve;

    // Servers: Syntalos master -> Module process commands
    std::optional<IoxServer<ApiVersionRequest, ApiVersionResponse>> srvApiVersion;
    std::optional<IoxServer<SetNicenessRequest, DoneResponse>> srvSetNiceness;
    std::optional<IoxServer<SetMaxRealtimePriority, DoneResponse>> srvSetMaxRTPriority;
    std::optional<IoxServer<SetCPUAffinityRequest, DoneResponse>> srvSetCPUAffinity;
    std::optional<IoxServer<ConnectInputRequest, DoneResponse>> srvConnectIPort;
    std::optional<IoxServer<StartRequest, DoneResponse>> srvStart;
    std::optional<IoxServer<StopRequest, DoneResponse>> srvStop;
    std::optional<IoxServer<ShutdownRequest, DoneResponse>> srvShutdown;
    std::optional<IoxUntypedReqServer> srvLoadScript;
    std::optional<IoxUntypedReqServer> srvSetPortsPreset;
    std::optional<IoxUntypedReqServer> srvUpdateIPortMetadata;
    std::optional<IoxUntypedReqResServer> srvSaveSettings;
    std::optional<IoxUntypedReqServer> srvLoadSettings;
    std::optional<IoxUntypedReqServer> srvPrepareRun;

    std::optional<IoxServer<ShowDisplayRequest, DoneResponse>> srvShowDisplay;
    std::optional<IoxServer<ShowSettingsRequest, DoneResponse>> srvShowSettings;

    // Listens for messages from the server
    std::optional<IoxListener> masterCtlEventListener;

    // Used by us to ping master if we have a message
    std::optional<IoxNotifier> ctlEventNotifier;

    // WaitSet to efficiently wait for messages from master
    std::optional<IoxWaitSet> waitSet;
    std::optional<IoxWaitSetGuard> waitSetCtrlGuard;
    bool waitSetDirty = true;
    // Set to true when a Stop command has been processed but input-port subscribers have
    // not yet been dropped. The actual drop is deferred until the current WaitSet iteration
    // finishes so that data events that arrived in the same iteration as Stop are not lost.
    bool inputPortResetPending = false;

    ModuleState state;
    int maxRTPriority;
    std::vector<std::shared_ptr<InputPortInfo>> inPortInfo;
    std::vector<std::shared_ptr<OutputPortInfo>> outPortInfo;
    SyncTimer *syTimer;
    TestSubjectInfo testSubject;
    RunInfo runInfo;
    bool allowAsyncStart = true;

    LoadScriptFn loadScriptCb;
    SaveSettingsFn saveSettingsCb;
    LoadSettingsFn loadSettingsCb;
    PrepareRunFn prepareRunCb;
    StartFn startCb;
    StopFn stopCb;
    ShutdownFn shutdownCb;
    ShowSettingsFn showSettingsCb;
    ShowDisplayFn showDisplayCb;

    bool shutdownPending; /// Set to true if we received a shutdown request and are expected to handle no more events.

    [[nodiscard]] std::string svcName(const std::string &channel) const
    {
        assert(!modId.empty());
        return makeModuleServiceName(modId, channel);
    }

    /**
     * Notify the master that we have sent something on a control channel.
     */
    void notifyMaster() const
    {
        if (!ctlEventNotifier.has_value()) [[unlikely]] {
            SY_LOG_ERROR(logSyLink, "notifyMaster: Notifier was not initialized, can not notify master!");
            return;
        }

        auto r = ctlEventNotifier->notify();
        if (!r.has_value())
            SY_LOG_ERROR(
                logSyLink, "Failed to notify master of control event: {}", iox2::bb::into<const char *>(r.error()));
    }

    /**
     * Send a port change to master and block until acknowledged.
     */
    void sendPortChangeData(IoxUntypedClient &clt, const ByteVector &data)
    {
        auto maybeSlice = clt.loan_slice_uninit(static_cast<uint64_t>(data.size()));
        if (!maybeSlice.has_value()) {
            SY_LOG_ERROR(
                logSyLink,
                "Failed to loan memory for port change request: {}",
                iox2::bb::into<const char *>(maybeSlice.error()));
            return;
        }
        auto rawSlice = std::move(maybeSlice).value();
        std::memmove(rawSlice.payload_mut().data(), data.data(), data.size());
        auto pending = iox2::send(iox2::assume_init(std::move(rawSlice))).value();
        notifyMaster();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (true) {
            auto response = pending.receive().value();
            if (response.has_value())
                return;
            if (std::chrono::steady_clock::now() >= deadline) {
                SY_LOG_ERROR(
                    logSyLink, "Port change acknowledgment from master timed out after 60s - aborting worker.");
                std::abort();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(25));
        }
    }

    /**
     * Send a typed DoneResponse from a typed active request.
     */
    template<typename Req>
    static void replyDone(iox2::ActiveRequest<iox2::ServiceType::Ipc, Req, void, DoneResponse, void> &req, bool success)
    {
        auto maybeResponse = req.loan_uninit();
        if (!maybeResponse.has_value()) {
            SY_LOG_ERROR(
                logSyLink,
                "Failed to loan response for 'done' reply: {}",
                iox2::bb::into<const char *>(maybeResponse.error()));
            return;
        }
        iox2::send(std::move(maybeResponse).value().write_payload(DoneResponse{success})).value();
    }

    /**
     * Send a DoneResponse from a slice active request.
     */
    static void replyDoneSlice(SliceActiveRequest &req, bool success)
    {
        auto maybeResponse = req.loan_uninit();
        if (!maybeResponse.has_value()) {
            SY_LOG_ERROR(
                logSyLink,
                "Failed to loan response for 'done' reply: {}",
                iox2::bb::into<const char *>(maybeResponse.error()));
            return;
        }
        iox2::send(std::move(maybeResponse).value().write_payload(DoneResponse{success})).value();
    }

    /**
     * Reset the WaitSet to reflect current state.
     */
    void rebuildWaitSet()
    {
        // MUST drop ALL WaitSet guards before destroying the WaitSet itself.
        // iceoryx2 contract: "WaitSetGuard must live at most as long as the WaitSet."
        // Dropping a guard after the WaitSet is destroyed is use-after-free on
        // the Rust side and causes EBADF errors and corrupted event state on the
        // next run.
        waitSetCtrlGuard.reset();
        for (auto &iport : inPortInfo)
            iport->d->ioxGuard.reset();
        for (auto &oport : outPortInfo)
            oport->d->ioxGuard.reset();

        // Now safe to destroy the old WaitSet
        waitSet.reset();

        // Build a fresh WaitSet and re-attach everything
        waitSet.emplace(
            iox2::WaitSetBuilder()
                .signal_handling_mode(iox2::SignalHandlingMode::HandleTerminationRequests)
                .create<iox2::ServiceType::Ipc>()
                .value());

        // Control attachment: wakes for requests from the master
        if (masterCtlEventListener.has_value())
            waitSetCtrlGuard.emplace(waitSet->attach_notification(*masterCtlEventListener).value());

        // Per-input-port attachments
        for (auto &iport : inPortInfo) {
            if (!iport->d->connected || !iport->d->ioxSub.has_value())
                continue;
            iport->d->ioxGuard.emplace(waitSet->attach_notification(*iport->d->ioxSub).value());
        }

        // Per-output-port publisher attachments
        for (auto &oport : outPortInfo) {
            if (!oport->d->ioxPub.has_value())
                continue;
            // Proactively update connections for events that queued up while WaitSet was not active
            oport->d->ioxPub->handleEvents();
            oport->d->ioxGuard.emplace(waitSet->attach_notification(*oport->d->ioxPub).value());
        }

        waitSetDirty = false;
    }

    /**
     * Perform the deferred input-port subscriber drop that was requested by a Stop command.
     * Must be called OUTSIDE of a WaitSet onEvent callback (i.e. after
     * wait_and_process_once_with_timeout returns) so the guards are not invalidated
     * while we are still iterating over triggered events.
     */
    void processPendingIPortReset()
    {
        if (!inputPortResetPending)
            return;
        for (auto &iport : inPortInfo) {
            iport->d->ioxGuard.reset();
            iport->d->ioxSub.reset();
            iport->d->connected = false;
        }
        inputPortResetPending = false;
        waitSetDirty = true;
    }

    /**
     * Send an EdlReserveRequest to master and return the parsed reply.
     */
    std::expected<EdlReserveReply, std::string> sendEdlReserveRequest(const EdlReserveRequest &req)
    {
        const auto bytes = req.toBytes();
        auto maybeSlice = cltEdlReserve->loan_slice_uninit(static_cast<uint64_t>(bytes.size()));
        if (!maybeSlice.has_value())
            return std::unexpected(
                std::string("Failed to loan slice for EdlReserve: ")
                + iox2::bb::into<const char *>(maybeSlice.error()));
        auto rawSlice = std::move(maybeSlice).value();
        std::memcpy(rawSlice.payload_mut().data(), bytes.data(), bytes.size());
        auto pending = iox2::send(iox2::assume_init(std::move(rawSlice))).value();
        notifyMaster();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (true) {
            auto response = pending.receive().value();
            if (response.has_value()) {
                const auto pl = response->payload();
                return EdlReserveReply::fromMemory(pl.data(), pl.number_of_bytes());
            }
            if (std::chrono::steady_clock::now() >= deadline)
                return std::unexpected("Timeout waiting for EdlReserve response from master.");
            std::this_thread::sleep_for(std::chrono::microseconds(25));
        }
    }

    /**
     * Process any incoming data on the input ports.
     */
    void processPendingData(const iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> &attachmentId)
    {
        for (auto &iport : inPortInfo) {
            if (!iport->d->connected || !iport->d->ioxSub.has_value())
                continue;
            if (!iport->d->ioxGuard.has_value() || !attachmentId.has_event_from(*iport->d->ioxGuard))
                continue;

            if (iport->d->newDataCb) {
                iport->d->ioxSub->handleEvents([&](const IoxImmutableByteSlice &pl) {
                    iport->d->newDataCb(pl.data(), pl.number_of_bytes());
                });
            } else {
                // Still drain to prevent the queue filling up even if there's no callback.
                iport->d->ioxSub->handleEvents([](const IoxImmutableByteSlice &) {});
            }
        }

        // Handle output-port publisher events (SubscriberConnected / SubscriberDisconnected).
        for (auto &oport : outPortInfo) {
            if (!oport->d->ioxPub.has_value() || !oport->d->ioxGuard.has_value())
                continue;
            if (!attachmentId.has_event_from(*oport->d->ioxGuard))
                continue;
            oport->d->ioxPub->handleEvents();
        }
    }
};

SyntalosLink::SyntalosLink(const std::string &instanceId)
    : d(new SyntalosLink::Private(instanceId))
{
    d->syTimer = new SyncTimer;

    // we us the fast, async start() by default
    d->allowAsyncStart = true;

    // Immediately upon creation, we send a message that we are initializing.
    // A client using this interface has to set this to IDLE once it has set up the basics.
    setState(ModuleState::INITIALIZING);
}

SyntalosLink::~SyntalosLink()
{
    delete d->syTimer;
}

std::string SyntalosLink::instanceId() const
{
    return d->modId;
}

void SyntalosLink::raiseError(const std::string &title, const std::string &message)
{
    auto uninit = d->pubError->loan_uninit().value();
    auto &ev = uninit.payload_mut();
    ev.title = iox2::bb::StaticString<128>::from_utf8_null_terminated_unchecked_truncated(title.c_str(), title.size());
    ev.message = iox2::bb::StaticString<2048>::from_utf8_null_terminated_unchecked_truncated(
        message.c_str(), message.size());
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    setState(ModuleState::ERROR);
}

void SyntalosLink::raiseError(const std::string &message)
{
    auto uninit = d->pubError->loan_uninit().value();
    auto &ev = uninit.payload_mut();
    ev.title = iox2::bb::StaticString<128>();
    ev.message = iox2::bb::StaticString<2048>::from_utf8_null_terminated_unchecked_truncated(
        message.c_str(), message.size());
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    setState(ModuleState::ERROR);
}

SY_DEFINE_LOG_CATEGORY(logIpc, "ipc");

static void ipcLogMessageDispatch(datactl::LogSeverity severity, const std::string &msg)
{
    if (::Syntalos::datactl::shouldLog(logIpc, severity))
        ::Syntalos::datactl::dispatchLog(logIpc, severity, __FILE__, __LINE__, __func__, msg);
}

void SyntalosLink::processPendingControl()
{
    // Drain the master control listener to keep its socket buffer clear.
    // We *must* drain at the start to immediately consume the notification that
    // triggered this call.
    // Any new notifications arriving DURING processing are left intact,  preventing
    // the race where a notification for a freshly-queued request arrives just before
    // an end-of-function drain and gets silently discarded, stranding the request.
    drainListenerEvents(*d->masterCtlEventListener);

    // ---- ApiVersion ----
    while (true) {
        auto req = safeReceive(*d->srvApiVersion);
        if (!req.has_value())
            break;

        auto maybeResponse = req->loan_uninit();
        if (!maybeResponse.has_value()) {
            SY_LOG_ERROR(
                logSyLink,
                "Failed to loan response for API version request: {}",
                iox2::bb::into<const char *>(maybeResponse.error()));
            continue;
        }

        ApiVersionResponse resp;
        resp.apiVersion = iox2::bb::StaticString<64>::from_utf8_null_terminated_unchecked_truncated(
            SY_MODULE_API_TAG, std::strlen(SY_MODULE_API_TAG));
        iox2::send(std::move(maybeResponse).value().write_payload(std::move(resp))).value();
    }

    // ---- SetNiceness ----
    while (true) {
        auto req = safeReceive(*d->srvSetNiceness);
        if (!req.has_value())
            break;

        // apply niceness request immediately to current thread
        const bool ok = setCurrentThreadNiceness(req->payload().nice);
        if (!ok)
            // Rtkit may have hit its per-user concurrent-thread limit.
            // The module will continue at default priority rather than failing to start entirely.
            SY_LOG_WARNING(
                logSyLink,
                "Worker thread niceness could not be set to {} - module will run at default priority.",
                req->payload().nice);
        Private::replyDone(*req, true);
    }

    // ---- SetMaxRealtimePriority ----
    while (true) {
        auto req = safeReceive(*d->srvSetMaxRTPriority);
        if (!req.has_value())
            break;
        d->maxRTPriority = req->payload().priority;
        Private::replyDone(*req, true);
    }

    // ---- SetCPUAffinity ----
    while (true) {
        auto req = safeReceive(*d->srvSetCPUAffinity);
        if (!req.has_value())
            break;
        const auto &cores = req->payload().cores;
        if (!cores.empty()) {
            std::vector<uint> coreVec;
            coreVec.reserve(cores.size());
            for (uint64_t i = 0; i < cores.size(); ++i)
                coreVec.push_back(cores.unchecked_access()[i]);
            thread_set_affinity_from_vec(pthread_self(), coreVec);
        }
        Private::replyDone(*req, true);
    }

    // ---- LoadScript ----
    // Execute the script callback BEFORE replying so that by the time the master's
    // callSliceClientSimple() returns, all port ADD/REMOVE messages that the script
    // published are already in the IPC queues.  The master can then drain them with
    // a single handleIncomingControl() call and be guaranteed to see all ports.
    while (true) {
        auto req = safeReceive(*d->srvLoadScript);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto scriptReqData = LoadScriptRequest::fromMemory(pl.data(), pl.number_of_bytes());
        // If the caller requested a port reset, clear all existing port state first so
        // the script starts with a clean slate (needed when reloading persistent-mode scripts).
        if (scriptReqData.resetPorts)
            resetPorts();
        // execute script first - any registerInput/OutputPort() calls happen here
        if (d->loadScriptCb && !scriptReqData.script.empty())
            d->loadScriptCb(scriptReqData.script, scriptReqData.workingDir);
        // only then reply, so the master knows ports are settled
        Private::replyDoneSlice(*req, true);
    }

    // ---- SetPortsPreset ----
    // Add any ports not yet known; never remove existing ones (they may be in use).
    while (true) {
        auto req = safeReceive(*d->srvSetPortsPreset);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto sppReq = SetPortsPresetRequest::fromMemory(pl.data(), pl.number_of_bytes());

        // We must not remove existing ports, as they might be in use. So instead, we
        // add & merge port data.
        for (const auto &ipc : sppReq.inPorts) {
            bool skip = false;
            for (const auto &ip : d->inPortInfo) {
                if (ip->id() == ipc.id)
                    skip = true;
            }
            if (skip)
                continue;
            d->inPortInfo.push_back(std::shared_ptr<InputPortInfo>(new InputPortInfo(ipc)));

            // we will have to rebuild the waitset after ports changed
            d->waitSetDirty = true;
        }

        for (const auto &opc : sppReq.outPorts) {
            std::shared_ptr<OutputPortInfo> oport;
            bool update = false;
            for (const auto &op : d->outPortInfo) {
                if (op->id() != opc.id)
                    continue;
                oport = op;
                update = true;
                break;
            }
            if (!oport)
                oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));

            // Detach old publisher from WaitSet before replacement.
            oport->d->ioxGuard.reset();
            oport->d->ioxPub.reset(); // drop the old connection first, before trying to create a new one
            oport->d->ioxPub.emplace(
                SyPublisher::create(*d->node, d->modId, oport->d->ipcChannelId(), opc.topology, ipcLogMessageDispatch));
            if (!update)
                d->outPortInfo.push_back(oport);

            // we will have to rebuild the waitset after ports changed
            d->waitSetDirty = true;
        }

        Private::replyDoneSlice(*req, true);
    }

    // ---- ConnectInputPort ----
    while (true) {
        auto req = safeReceive(*d->srvConnectIPort);
        if (!req.has_value())
            break;
        const auto &r = req->payload();
        const std::string_view portId = r.portId.unchecked_access().c_str();

        // find the port
        auto it = std::find_if(d->inPortInfo.begin(), d->inPortInfo.end(), [&](const auto &ip) {
            return ip->id() == portId;
        });
        if (it == d->inPortInfo.end()) {
            // return error if the port was not registered
            Private::replyDone(*req, false);
            continue;
        }
        auto &iport = *it;

        // connect the port
        iport->d->connected = true;

        // MUST reset the WaitSet guard BEFORE replacing the old subscriber
        iport->d->ioxGuard.reset();
        iport->d->ioxSub.reset(); // drop the old connection first, before trying to create a new one
        iport->d->ioxSub.emplace(
            SySubscriber::create(
                *d->node,
                std::string(r.instanceId.unchecked_access().c_str()),
                std::string(r.channelId.unchecked_access().c_str()),
                r.topology,
                ipcLogMessageDispatch));

        Private::replyDone(*req, true);

        // we will have to rebuild the waitset after ports changed
        d->waitSetDirty = true;
    }

    // ---- UpdateInputPortMetadata ----
    while (true) {
        auto req = safeReceive(*d->srvUpdateIPortMetadata);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto reqUpdateMD = UpdateInputPortMetadataRequest::fromMemory(pl.data(), pl.number_of_bytes());

        // update metadata
        for (const auto &ip : d->inPortInfo) {
            if (ip->id() == reqUpdateMD.id) {
                ip->d->metadata = reqUpdateMD.metadata;
                break;
            }
        }

        Private::replyDoneSlice(*req, true);
    }

    // ---- SaveSettings ----
    while (true) {
        auto req = safeReceive(*d->srvSaveSettings);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto ssReq = SaveSettingsRequest::fromMemory(pl.data(), pl.number_of_bytes());
        SaveSettingsResponse ssResp;
        ssResp.success = true;
        if (d->saveSettingsCb)
            ssResp.success = d->saveSettingsCb(ssResp.data, ssReq.baseDir);

        auto ssRespData = ssResp.toBytes();
        auto maybeResponse = req->loan_slice_uninit(ssRespData.size());
        if (!maybeResponse.has_value()) {
            SY_LOG_ERROR(
                logSyLink,
                "Failed to loan response ({} bytes) for reply to SaveSettings: {}",
                ssRespData.size(),
                iox2::bb::into<const char *>(maybeResponse.error()));
            break;
        }

        auto rawResponse = std::move(maybeResponse).value();
        std::memcpy(rawResponse.payload_mut().data(), ssRespData.data(), ssRespData.size());
        iox2::send(iox2::assume_init(std::move(rawResponse))).value();
    }

    // ---- LoadSettings ----
    while (true) {
        auto req = safeReceive(*d->srvLoadSettings);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto lsReq = LoadSettingsRequest::fromMemory(pl.data(), pl.number_of_bytes());
        auto success = true;
        if (d->loadSettingsCb)
            success = d->loadSettingsCb(lsReq.data, lsReq.baseDir);

        Private::replyDoneSlice(*req, success);
    }

    // ---- PrepareRun ----
    while (true) {
        auto req = safeReceive(*d->srvPrepareRun);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto prepReq = PrepareRunRequest::fromMemory(pl.data(), pl.number_of_bytes());

        // update test subject details
        d->testSubject.id = prepReq.subjectId;
        d->testSubject.group = prepReq.subjectGroup;

        // set up run info and build local EDL tree root
        d->runInfo.uuid = Uuid(prepReq.runUuid);
        d->runInfo.moduleName = prepReq.moduleName;
        if (!prepReq.edlRootPath.empty()) {
            const fs::path rootPath(prepReq.edlRootPath);
            auto rootGroup = std::make_shared<EDLGroup>(rootPath.filename().string());
            rootGroup->setPath(rootPath);
            rootGroup->setCollectionId(prepReq.runUuid);
            d->runInfo.rootGroup = std::move(rootGroup);
        }

        // prepare the run
        auto success = true;
        if (d->prepareRunCb)
            success = d->prepareRunCb();

        Private::replyDoneSlice(*req, success);
    }

    // ---- Start ----
    while (true) {
        auto req = safeReceive(*d->srvStart);
        if (!req.has_value())
            break;

        const auto timePoint = symaster_timepoint(microseconds_t(req->payload().startTimestampUsec));
        delete d->syTimer;
        d->syTimer = new SyncTimer;
        d->syTimer->startAt(timePoint);

        // Ensure all output-port publishers know about subscribers that connected
        // during the prepare phase.
        for (auto &oport : d->outPortInfo) {
            if (oport->d->ioxPub.has_value())
                oport->d->ioxPub->handleEvents();
        }

        if (d->allowAsyncStart) {
            // reply immediately, so the master can continue starting other modules
            Private::replyDone(*req, true);
            if (d->startCb)
                d->startCb();
        } else {
            // Run the start callback BEFORE sending Done so that any port metadata
            // updates (e.g. forwarding framerate to output ports) are acknowledged
            // by the master while it is still in the callClientSimple(START) wait loop.
            // This guarantees that startStream() on the master side sees the final metadata.
            // Any errors are reported via the error channel.
            if (d->startCb)
                d->startCb();
            Private::replyDone(*req, true);
        }
    }

    // ---- Stop ----
    while (true) {
        auto req = safeReceive(*d->srvStop);
        if (!req.has_value())
            break;
        // we wait for the stop callback to finish before responding
        if (d->stopCb)
            d->stopCb();

        // save local EDL subtree (if any was created during the run)
        if (d->runInfo.rootGroup) {
            for (const auto &child : d->runInfo.rootGroup->children()) {
                if (auto r = child->save(); !r)
                    SY_LOG_ERROR(logSyLink, "Failed to save EDL child '{}': {}", child->name(), r.error());
            }
        }

        // reset EDL reference, it should no longer be used after stop()
        d->runInfo.rootGroup.reset();

        Private::replyDone(*req, true);

        // Mark input ports for deferred dropping. We do NOT drop them here because
        // this function may be called from inside a WaitSet onEvent callback
        // (awaitData / awaitDataForever). If we drop the subscriber guards while the
        // WaitSet is still iterating over triggered events, any data event that fired
        // in the same cycle as this Stop would be silently discarded.
        // By deferring the drop to the end of the WaitSet iteration, concurrent data
        // events are still delivered before the ports are torn down.
        if (!d->inPortInfo.empty())
            d->inputPortResetPending = true;
    }

    // ---- Shutdown ----
    while (true) {
        auto req = safeReceive(*d->srvShutdown);
        if (!req.has_value())
            break;
        // NOTE: We reply immediately here and defer processing of the call,
        // because otherwise the master would never get a response if we
        // tear down the process too quickly.
        Private::replyDone(*req, true);

        // execute shutdown action after replying to master
        // if no callback is defined, we just exit()
        if (d->shutdownCb)
            d->shutdownCb();
        d->shutdownPending = true;
        break;
    }

    // ---- ShowDisplay ----
    while (true) {
        auto req = safeReceive(*d->srvShowDisplay);
        if (!req.has_value())
            break;
        Private::replyDone(*req, true);
        if (d->showDisplayCb)
            d->showDisplayCb();
    }

    // ---- ShowSettings ----
    while (true) {
        auto req = safeReceive(*d->srvShowSettings);
        if (!req.has_value())
            break;
        Private::replyDone(*req, true);
        if (d->showSettingsCb)
            d->showSettingsCb();
    }
}

void SyntalosLink::awaitData(int timeoutUsec, const std::function<void()> &eventFn)
{
    // If a shutdown request was processed, we should die ASAP and this function
    // should no longer run - otherwise we may block forever and get killed
    if (d->shutdownPending)
        return;

    // Complete any deferred input-port subscriber drop from a previous Stop command,
    // the rebuild the WaitSet if needed.
    d->processPendingIPortReset();
    if (d->waitSetDirty)
        d->rebuildWaitSet();

    auto onEvent =
        [this](const iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> &attachmentId) -> iox2::CallbackProgression {
        // handle control messages
        if (attachmentId.has_event_from(*d->waitSetCtrlGuard)) {
            processPendingControl();
            if (d->waitSetDirty)
                return iox2::CallbackProgression::Stop;
        } else {
            // handle incoming data
            d->processPendingData(attachmentId);
        }

        return d->shutdownPending ? iox2::CallbackProgression::Stop : iox2::CallbackProgression::Continue;
    };

    // Helper: inspect the WaitSet run-result and trigger a clean shutdown when
    // IOX reports that SIGTERM/SIGINT was received
    auto handleRunResult = [this](const iox2::bb::Expected<iox2::WaitSetRunResult, iox2::WaitSetRunError> &res) {
        if (!res.has_value())
            return;
        const auto r = res.value();
        if (r == iox2::WaitSetRunResult::Interrupt || r == iox2::WaitSetRunResult::TerminationRequest)
            d->shutdownPending = true;
    };

    if (timeoutUsec < 0) {
        do {
            // Complete deferred input-port subscriber drop, then rebuild WaitSet if needed.
            d->processPendingIPortReset();
            if (d->waitSetDirty)
                d->rebuildWaitSet();

            handleRunResult(
                d->waitSet->wait_and_process_once_with_timeout(onEvent, iox2::bb::Duration::from_millis(250)));
            if (eventFn)
                eventFn();

            // Keep the default GLib context alive so timer/idle sources are dispatched.
            iterateDefaultMainContextNonBlocking();

            // exit if we are supposed to shutdown
            if (d->shutdownPending)
                break;
        } while (d->state == ModuleState::RUNNING);
    } else {
        handleRunResult(
            d->waitSet->wait_and_process_once_with_timeout(onEvent, iox2::bb::Duration::from_micros(timeoutUsec)));
        if (eventFn)
            eventFn();
    }
}

void SyntalosLink::awaitDataForever(const std::function<void()> &eventFn, int intervalUsec)
{
    if (d->waitSetDirty)
        d->rebuildWaitSet();

    auto onEvent =
        [this](const iox2::WaitSetAttachmentId<iox2::ServiceType::Ipc> &attachmentId) -> iox2::CallbackProgression {
        if (attachmentId.has_event_from(*d->waitSetCtrlGuard)) {
            processPendingControl();
            if (d->waitSetDirty)
                return iox2::CallbackProgression::Stop;
        } else {
            d->processPendingData(attachmentId);
        }
        return d->shutdownPending ? iox2::CallbackProgression::Stop : iox2::CallbackProgression::Continue;
    };

    iterateDefaultMainContextNonBlocking();

    while (true) {
        // Complete deferred input-port subscriber drop, then rebuild WaitSet if ports were connected/disconnected
        // since the last iteration (processPendingControl() sets waitSetDirty when that happens).
        d->processPendingIPortReset();
        if (d->waitSetDirty)
            d->rebuildWaitSet();

        const auto res = d->waitSet->wait_and_process_once_with_timeout(
            onEvent, iox2::bb::Duration::from_micros(intervalUsec));
        if (!res.has_value()) {
            SY_LOG_WARNING(
                logSyLink, "Event loop terminated unexpectedly: {}", iox2::bb::into<const char *>(res.error()));
            return;
        }
        // Treat SIGTERM/SIGINT as a shutdown request
        const auto r = res.value();
        if (r == iox2::WaitSetRunResult::Interrupt || r == iox2::WaitSetRunResult::TerminationRequest)
            d->shutdownPending = true;

        // call external event function
        if (eventFn)
            eventFn();

        // Dispatch GLib events periodically so external sources can run on the default context.
        iterateDefaultMainContextNonBlocking();

        // exit if we are about to shutdown
        if (d->shutdownPending)
            break;
    }
}

ModuleState SyntalosLink::state() const
{
    return d->state;
}

bool SyntalosLink::isShutdownPending() const
{
    return d->shutdownPending;
}

void SyntalosLink::setShutdownPending(bool pending)
{
    d->shutdownPending = pending;
}

void SyntalosLink::setState(ModuleState state)
{
    auto uninit = d->pubState->loan_uninit().value();
    uninit.payload_mut().state = state;
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    d->state = state;
}

void SyntalosLink::setStatusMessage(const std::string &message)
{
    auto uninit = d->pubStatusMsg->loan_uninit().value();
    uninit.payload_mut().text = iox2::bb::StaticString<512>::from_utf8_null_terminated_unchecked_truncated(
        message.c_str(), message.size());
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();
}

int SyntalosLink::maxRealtimePriority() const
{
    return d->maxRTPriority;
}

void SyntalosLink::setLoadScriptCallback(LoadScriptFn callback)
{
    d->loadScriptCb = std::move(callback);
}

void SyntalosLink::setSaveSettingsCallback(SaveSettingsFn callback)
{
    d->saveSettingsCb = std::move(callback);
}

void SyntalosLink::setLoadSettingsCallback(LoadSettingsFn callback)
{
    d->loadSettingsCb = std::move(callback);
}

void SyntalosLink::setPrepareRunCallback(PrepareRunFn callback)
{
    d->prepareRunCb = std::move(callback);
}

void SyntalosLink::setStartCallback(StartFn callback)
{
    d->startCb = std::move(callback);
}

void SyntalosLink::setStopCallback(StopFn callback)
{
    d->stopCb = std::move(callback);
}

void SyntalosLink::setShutdownCallback(ShutdownFn callback)
{
    d->shutdownCb = std::move(callback);
}

SyncTimer *SyntalosLink::timer() const
{
    return d->syTimer;
}

const TestSubjectInfo &SyntalosLink::testSubject() const
{
    return d->testSubject;
}

const RunInfo &SyntalosLink::runInfo() const
{
    return d->runInfo;
}

/**
 * Helper: compute the relative path of `target` from the rootGroup.
 * Returns "" if target IS the rootGroup (i.e., put at root level).
 */
static std::string edlRelPath(const EDLGroup &root, const EDLGroup &target)
{
    if (root.path() == target.path())
        return {};
    const auto rootStr = root.path().string();
    auto targetStr = target.path().string();
    if (targetStr.size() > rootStr.size() + 1 && targetStr.substr(0, rootStr.size()) == rootStr)
        return targetStr.substr(rootStr.size() + 1); // strip leading slash
    return targetStr;                                // fallback: return full path (shouldn't happen)
}

auto SyntalosLink::reserveEdlGroup(const std::shared_ptr<EDLGroup> &parent, const std::string &name)
    -> std::expected<std::shared_ptr<EDLGroup>, std::string>
{
    if (!d->runInfo.rootGroup)
        return std::unexpected("No EDL root group available (prepare step has not been reached yet).");
    if (!d->cltEdlReserve.has_value())
        return std::unexpected("EDL reserve client not initialized.");

    EdlReserveRequest req;
    req.kind = EdlReserveRequest::Kind::Group;
    req.parentRelPath = edlRelPath(*d->runInfo.rootGroup, *parent);
    req.name = name;

    auto rep = d->sendEdlReserveRequest(req);
    if (!rep)
        return std::unexpected(rep.error());
    if (!rep->success)
        return std::unexpected(rep->errorMessage);
    return parent->groupByName(name, EDLCreateFlag::CREATE_OR_OPEN);
}

auto SyntalosLink::reserveEdlDataset(const std::shared_ptr<EDLGroup> &parent, const std::string &name)
    -> std::expected<std::shared_ptr<EDLDataset>, std::string>
{
    if (!d->runInfo.rootGroup)
        return std::unexpected("No EDL root group available (prepare step has not been reached yet).");
    if (!d->cltEdlReserve.has_value())
        return std::unexpected("EDL reserve client not initialized.");

    EdlReserveRequest req;
    req.kind = EdlReserveRequest::Kind::Dataset;
    req.parentRelPath = edlRelPath(*d->runInfo.rootGroup, *parent);
    req.name = name;

    auto rep = d->sendEdlReserveRequest(req);
    if (!rep)
        return std::unexpected(rep.error());
    if (!rep->success)
        return std::unexpected(rep->errorMessage);
    return parent->datasetByName(name, EDLCreateFlag::MUST_CREATE);
}

auto SyntalosLink::createDefaultDataset(const std::string &preferredName)
    -> std::expected<std::shared_ptr<EDLDataset>, std::string>
{
    const auto root = d->runInfo.rootGroup;
    if (!root)
        return std::unexpected("No EDL root group available (prepare step has not been reached yet).");
    const auto &modName = d->runInfo.moduleName;
    const auto &name = preferredName.empty() ? modName : preferredName;
    if (name.empty())
        return std::unexpected("Unable to determine dataset name.");
    return reserveEdlDataset(root, name);
}

auto SyntalosLink::createStorageGroup(const std::string &name) -> std::expected<std::shared_ptr<EDLGroup>, std::string>
{
    const auto root = d->runInfo.rootGroup;
    if (!root)
        return std::unexpected("No EDL root group available (prepare step has not been reached yet).");
    return reserveEdlGroup(root, name);
}

bool SyntalosLink::allowAsyncStart() const
{
    return d->allowAsyncStart;
}

void SyntalosLink::setAllowAsyncStart(bool allow)
{
    d->allowAsyncStart = allow;
}

void SyntalosLink::setShowSettingsCallback(ShowSettingsFn callback)
{
    d->showSettingsCb = std::move(callback);
}

void SyntalosLink::setShowDisplayCallback(ShowDisplayFn callback)
{
    d->showDisplayCb = std::move(callback);
}

std::vector<std::shared_ptr<InputPortInfo>> SyntalosLink::inputPorts() const
{
    return d->inPortInfo;
}

std::vector<std::shared_ptr<OutputPortInfo>> SyntalosLink::outputPorts() const
{
    return d->outPortInfo;
}

auto SyntalosLink::registerInputPort(const std::string &id, const std::string &title, BaseDataType::TypeId typeId)
    -> std::expected<std::shared_ptr<InputPortInfo>, std::string>
{
    // passing an invalid data type is a hard error
    if (!BaseDataType::typeIdIsValid(typeId)) {
        return std::unexpected(
            std::format("Can not register input port. Data type with ID '{}' is unknown.", static_cast<int>(typeId)));
    }

    // check for duplicates
    for (const auto &ip : d->inPortInfo) {
        if (ip->id() != id)
            continue;

        if (ip->dataTypeId() != typeId)
            return std::unexpected(
                std::format(
                    "Can not register input port. A port with ID '{}', but different data type, already exists.", id));
        if (ip->title() != title) {
            ip->d->title = title;
            updateInputPort(ip);
        }
        return ip;
    }

    // construct our reference for this port
    InputPortChangeRequest ipc(PortAction::ADD);
    ipc.id = id;
    ipc.title = title;
    ipc.dataTypeId = typeId;

    // announce the new port to master
    const auto iportData = ipc.toBytes();
    d->sendPortChangeData(*d->cltInPortChange, iportData);

    // we need to rebuild the waitset
    d->waitSetDirty = true;

    // construct proxy
    auto iport = std::shared_ptr<InputPortInfo>(new InputPortInfo(ipc));
    d->inPortInfo.push_back(iport);
    return iport;
}

auto SyntalosLink::registerOutputPort(
    const std::string &id,
    const std::string &title,
    BaseDataType::TypeId typeId,
    const MetaStringMap &metadata) -> std::expected<std::shared_ptr<OutputPortInfo>, std::string>
{
    // passing an invalid data type is a hard error
    if (!BaseDataType::typeIdIsValid(typeId)) {
        return std::unexpected(
            std::format("Can not register output port. Data type with ID '{}' is unknown.", static_cast<int>(typeId)));
    }

    // check for duplicates
    for (const auto &op : d->outPortInfo) {
        if (op->id() != id)
            continue;

        if (op->dataTypeId() != typeId)
            return std::unexpected(
                std::format(
                    "Can not register output port. A port with ID '{}', but different data type, already exists.", id));
        op->d->title = title;
        op->d->metadata = metadata;
        updateOutputPort(op);
        return op;
    }

    // construct our reference for this port
    OutputPortChangeRequest opc(PortAction::ADD);
    opc.id = id;
    opc.title = title;
    opc.dataTypeId = typeId;
    opc.metadata = metadata;

    // announce the new port to master
    const auto oportData = opc.toBytes();
    d->sendPortChangeData(*d->cltOutPortChange, oportData);

    // we need to rebuild the waitset
    d->waitSetDirty = true;

    // construct proxy
    auto oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));
    oport->d->ioxPub.reset();
    oport->d->ioxPub.emplace(
        SyPublisher::create(*d->node, d->modId, oport->d->ipcChannelId(), opc.topology, ipcLogMessageDispatch));
    d->outPortInfo.push_back(oport);

    return oport;
}

void SyntalosLink::updateInputPort(const std::shared_ptr<InputPortInfo> &iport)
{
    InputPortChangeRequest ipc(PortAction::CHANGE);
    ipc.id = iport->id();
    ipc.title = iport->d->title;
    ipc.dataTypeId = iport->d->dataTypeId;
    ipc.metadata = iport->d->metadata;
    ipc.throttleItemsPerSec = iport->d->throttleItemsPerSec;

    const auto iportData = ipc.toBytes();
    d->sendPortChangeData(*d->cltInPortChange, iportData);
}

void SyntalosLink::updateOutputPort(const std::shared_ptr<OutputPortInfo> &oport)
{
    OutputPortChangeRequest opc(PortAction::CHANGE);
    opc.id = oport->id();
    opc.title = oport->d->title;
    opc.dataTypeId = oport->dataTypeId();
    opc.metadata = oport->d->metadata;

    const auto oportData = opc.toBytes();
    d->sendPortChangeData(*d->cltOutPortChange, oportData);
}

void SyntalosLink::removeInputPort(const std::shared_ptr<InputPortInfo> &iport)
{
    InputPortChangeRequest ipc(PortAction::REMOVE);
    ipc.id = iport->id();

    // notify master
    const auto iportData = ipc.toBytes();
    d->sendPortChangeData(*d->cltInPortChange, iportData);

    // reset our reference (it will not be usable afterwards)
    iport->d->ioxGuard.reset();
    iport->d->ioxSub.reset();
    iport->d->connected = false;
    d->waitSetDirty = true;
}

void SyntalosLink::removeOutputPort(const std::shared_ptr<OutputPortInfo> &oport)
{
    OutputPortChangeRequest opc(PortAction::REMOVE);
    opc.id = oport->id();

    const auto oportData = opc.toBytes();
    // notify master
    d->sendPortChangeData(*d->cltOutPortChange, oportData);

    // reset
    oport->d->ioxGuard.reset();
    oport->d->ioxPub.reset();
    d->waitSetDirty = true;
}

void SyntalosLink::resetPorts()
{
    // Tear down every IPC publisher and subscriber.
    // WaitSet guards must be dropped before the publishers/subscribers they
    // guard are destroyed (iceoryx2 contract: guard must not outlive the WaitSet).
    for (auto &op : d->outPortInfo)
        removeOutputPort(op);
    for (auto &ip : d->inPortInfo)
        removeInputPort(ip);

    d->outPortInfo.clear();
    d->inPortInfo.clear();

    // request a WaitSet rebuild (already done by the port removal calls, this is just to be explicit)
    d->waitSetDirty = true;
}

bool SyntalosLink::submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const BaseDataType &data)
{
    if (!oport->d->ioxPub.has_value()) {
        raiseError(std::format("Failed to send data on output port '{}': Publisher was not initialized!", oport->id()));
        return false;
    }
    auto &pub = *oport->d->ioxPub;

    try {
        const auto memSize = data.memorySize();
        if (memSize < 0) {
            // we do not know the required memory size in advance, so we need to
            // perform a serialization and extra copy operation
            data.toBytes(oport->d->outBuffer);
            auto slice = pub.loanSlice(static_cast<size_t>(oport->d->outBuffer.size()));
            std::memcpy(
                slice.payload_mut().data(),
                oport->d->outBuffer.data(),
                static_cast<size_t>(oport->d->outBuffer.size()));
            pub.sendSlice(std::move(slice));
        } else {
            // Higher efficiency code-path since the size is known in advance
            auto loan = pub.loanSlice(static_cast<size_t>(memSize));
            if (!data.writeToMemory(loan.payload_mut().data(), static_cast<ssize_t>(memSize))) {
                raiseError(std::format("Failed to serialize data for output port '{}'.", oport->id()));
                return false;
            }
            pub.sendSlice(std::move(loan));
        }
    } catch (std::exception &e) {
        raiseError(std::format("Failed to send data on output port '{}': {}", oport->id(), e.what()));
        return false;
    }

    return true;
}

} // namespace Syntalos
