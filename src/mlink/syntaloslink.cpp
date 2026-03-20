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

#include <QDebug>
#include <QCoreApplication>
#include <csignal>
#include <sys/prctl.h>
#include <iox2/iceoryx2.hpp>

#include "mlink/ipc-types-private.h"
#include "mlink/ipc-config-private.h"
#include "mlink/ipc-iox-private.h"
#include "utils/misc.h"
#include "utils/rtkit.h"
#include "utils/cpuaffinity.h"

using namespace Syntalos;
using namespace Syntalos::ipc;

namespace Syntalos
{

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
        std::cerr << "Client IPC receive failed:" << iox2::bb::into<const char *>(result.error());
        return {};
    }
    return std::move(result).value();
}

std::unique_ptr<SyntalosLink> initSyntalosModuleLink()
{
    auto syModuleId = qgetenv("SYNTALOS_MODULE_ID");
    if (syModuleId.isEmpty() || syModuleId.length() < 2)
        throw std::runtime_error("This module was not run by Syntalos, can not continue!");

    // set up stream data type mapping, if it hasn't been initialized yet
    registerStreamMetaTypes();

    // load shared iceoryx2 configuration
    if (auto res = setupIoxConfiguration(); !res.has_value()) {
        qCritical().noquote() << "Failed to set up IOX configuration:" << QString::fromStdString(res.error());
        throw std::runtime_error("Failed to set up IOX configuration: " + res.error());
    }

    // set IOX log level
    auto verboseLevel = qgetenv("SY_VERBOSE");
    if (verboseLevel == "1")
        iox2::set_log_level(iox2::LogLevel::Debug);
    else
        iox2::set_log_level(iox2::LogLevel::Info);

    // ensure we (try to) die if Syntalos, our parent, dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    return std::unique_ptr<SyntalosLink>(new SyntalosLink(syModuleId));
}

/**
 * Reference for a module input port
 */
class InputPortInfo::Private
{
public:
    explicit Private(const InputPortChange &pc)
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

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;

    NewDataRawFn newDataCb;
    uint throttleItemsPerSec;
};

InputPortInfo::InputPortInfo(const InputPortChange &pc)
    : d(new InputPortInfo::Private(pc))
{
}

QString InputPortInfo::id() const
{
    return d->id;
}

int InputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

QString InputPortInfo::title() const
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

QVariantHash InputPortInfo::metadata() const
{
    return d->metadata;
}

/**
 * Reference for a module output port
 */
class OutputPortInfo::Private
{
public:
    explicit Private(const OutputPortChange &pc)
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

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;

    // utilized to reuse allocated memory when sending data, to prevent fragmentation
    ByteVector outBuffer;

    [[nodiscard]] std::string ipcChannelId() const
    {
        return "o/" + id.toStdString();
    }
};

OutputPortInfo::OutputPortInfo(const OutputPortChange &pc)
    : d(new OutputPortInfo::Private(pc))
{
}

QString OutputPortInfo::id() const
{
    return d->id;
}

int OutputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

void OutputPortInfo::setMetadataVar(const QString &key, const QVariant &value)
{
    d->metadata[key] = value;
}

class SyntalosLink::Private
{
public:
    Private(const QString &instanceId)
        : modId(instanceId.toStdString()),
          state(ModuleState::UNKNOWN),
          maxRTPriority(0),
          syTimer(nullptr),
          shutdownPending(false)
    {
        node.emplace(
            iox2::NodeBuilder()
                .name(iox2::NodeName::create(modId.c_str()).value())
                .signal_handling_mode(iox2::SignalHandlingMode::HandleTerminationRequests)
                .create<iox2::ServiceType::Ipc>()
                .value());

        // interfaces
        pubError.emplace(makeTypedPublisher<ErrorEvent>(*node, svcName(ERROR_CHANNEL_ID)));
        pubState.emplace(makeTypedPublisher<StateChangeEvent>(*node, svcName(STATE_CHANNEL_ID)));
        pubStatusMsg.emplace(makeTypedPublisher<StatusMessageEvent>(*node, svcName(STATUS_MESSAGE_CHANNEL_ID)));

        pubSettingsChange.emplace(makeSlicePublisher(*node, svcName(SETTINGS_CHANGE_CHANNEL_ID)));
        pubInPortChange.emplace(makeSlicePublisher(*node, svcName(IN_PORT_CHANGE_CHANNEL_ID)));
        pubOutPortChange.emplace(makeSlicePublisher(*node, svcName(OUT_PORT_CHANGE_CHANNEL_ID)));

        srvPingPong.emplace(makeTypedServer<PingRequest, DoneResponse>(*node, svcName(PING_CALL_ID)));
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
        srvLoadScript.emplace(makeSliceServer(*node, svcName(LOAD_SCRIPT_CALL_ID)));
        srvSetPortsPreset.emplace(makeSliceServer(*node, svcName(SET_PORTS_PRESET_CALL_ID)));
        srvUpdateIPortMetadata.emplace(makeSliceServer(*node, svcName(IN_PORT_UPDATE_METADATA_ID)));
        srvPrepareStart.emplace(makeSliceServer(*node, svcName(PREPARE_START_CALL_ID)));
        srvShowSettings.emplace(makeSliceServer(*node, svcName(SHOW_SETTINGS_CALL_ID)));

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
    std::optional<IoxSlicePublisher> pubSettingsChange;
    std::optional<IoxSlicePublisher> pubInPortChange;
    std::optional<IoxSlicePublisher> pubOutPortChange;

    // Servers: Syntalos master -> Module process commands
    std::optional<IoxServer<PingRequest, DoneResponse>> srvPingPong;
    std::optional<IoxServer<SetNicenessRequest, DoneResponse>> srvSetNiceness;
    std::optional<IoxServer<SetMaxRealtimePriority, DoneResponse>> srvSetMaxRTPriority;
    std::optional<IoxServer<SetCPUAffinityRequest, DoneResponse>> srvSetCPUAffinity;
    std::optional<IoxServer<ConnectInputRequest, DoneResponse>> srvConnectIPort;
    std::optional<IoxServer<StartRequest, DoneResponse>> srvStart;
    std::optional<IoxServer<StopRequest, DoneResponse>> srvStop;
    std::optional<IoxServer<ShutdownRequest, DoneResponse>> srvShutdown;
    std::optional<IoxUntypedServer> srvLoadScript;
    std::optional<IoxUntypedServer> srvSetPortsPreset;
    std::optional<IoxUntypedServer> srvUpdateIPortMetadata;
    std::optional<IoxUntypedServer> srvPrepareStart;

    std::optional<IoxServer<ShowDisplayRequest, DoneResponse>> srvShowDisplay;
    std::optional<IoxUntypedServer> srvShowSettings;

    // Listens for messages from the server
    std::optional<IoxListener> masterCtlEventListener;

    // Used by us to ping master if we have a message
    std::optional<IoxNotifier> ctlEventNotifier;

    // WaitSet to efficiently wait for messages from master
    std::optional<IoxWaitSet> waitSet;
    std::optional<IoxWaitSetGuard> waitSetCtrlGuard;
    bool waitSetDirty = true;

    ModuleState state;
    int maxRTPriority;
    std::vector<std::shared_ptr<InputPortInfo>> inPortInfo;
    std::vector<std::shared_ptr<OutputPortInfo>> outPortInfo;
    SyncTimer *syTimer;

    LoadScriptFn loadScriptCb;
    PrepareStartFn prepareStartCb;
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
            std::cerr << "notifyMaster: Notifier was not initialized, can not notify master!" << std::endl;
            return;
        }

        auto r = ctlEventNotifier->notify();
        if (!r.has_value())
            std::cerr << "Failed to notify master of control event:" << iox2::bb::into<const char *>(r.error())
                      << std::endl;
    }

    /**
     * Send a typed DoneResponse from a typed active request.
     */
    template<typename Req>
    static void replyDone(iox2::ActiveRequest<iox2::ServiceType::Ipc, Req, void, DoneResponse, void> &req, bool success)
    {
        auto maybeResponse = req.loan_uninit();
        if (!maybeResponse.has_value()) {
            std::cerr << "Failed to loan response for 'done' reply: "
                      << iox2::bb::into<const char *>(maybeResponse.error()) << '\n';
            return;
        }
        iox2::send(std::move(maybeResponse).value().write_payload(DoneResponse{success})).value();
    }

    using SliceActiveRequest =
        iox2::ActiveRequest<iox2::ServiceType::Ipc, iox2::bb::Slice<std::byte>, void, DoneResponse, void>;

    /**
     * Send a DoneResponse from a slice active request.
     */
    static void replyDoneSlice(SliceActiveRequest &req, bool success)
    {
        auto maybeResponse = req.loan_uninit();
        if (!maybeResponse.has_value()) {
            std::cerr << "Failed to loan response for 'done' reply: "
                      << iox2::bb::into<const char *>(maybeResponse.error()) << '\n';
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
                iport->d->ioxSub->handleEvents([&](const IoxByteSlice &pl) {
                    iport->d->newDataCb(pl.data(), pl.number_of_bytes());
                });
            } else {
                // Still drain to prevent the queue filling up even if there's no callback.
                iport->d->ioxSub->handleEvents([](const IoxByteSlice &) {});
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

SyntalosLink::SyntalosLink(const QString &instanceId, QObject *parent)
    : QObject(parent),
      d(new SyntalosLink::Private(instanceId))
{
    d->syTimer = new SyncTimer;

    // Immediately upon creation, we send a message that we are initializing.
    // A client using this interface has to set this to IDLE once it has set up the basics.
    setState(ModuleState::INITIALIZING);
}

SyntalosLink::~SyntalosLink()
{
    delete d->syTimer;
}

QString SyntalosLink::instanceId() const
{
    return QString::fromUtf8(d->modId.c_str());
}

void SyntalosLink::raiseError(const QString &title, const QString &message)
{
    auto uninit = d->pubError->loan_uninit().value();
    auto &ev = uninit.payload_mut();
    ev.title = iox2::bb::StaticString<128>::from_utf8_null_terminated_unchecked_truncated(
        title.toUtf8().constData(), title.toUtf8().size());
    ev.message = iox2::bb::StaticString<2048>::from_utf8_null_terminated_unchecked_truncated(
        message.toUtf8().constData(), message.toUtf8().size());
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    setState(ModuleState::ERROR);
}

void SyntalosLink::raiseError(const QString &message)
{
    auto uninit = d->pubError->loan_uninit().value();
    auto &ev = uninit.payload_mut();
    ev.title = iox2::bb::StaticString<128>();
    ev.message = iox2::bb::StaticString<2048>::from_utf8_null_terminated_unchecked_truncated(
        message.toUtf8().constData(), message.toUtf8().size());
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    setState(ModuleState::ERROR);
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

    // ---- Ping ----
    while (true) {
        auto req = safeReceive(*d->srvPingPong);
        if (!req.has_value())
            break;

        // just respond as fast as we can
        Private::replyDone(*req, true);
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
            std::cerr << "Worker thread niceness could not be set to " << req->payload().nice
                      << " - module will run at default priority." << std::endl;
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
    while (true) {
        auto req = safeReceive(*d->srvLoadScript);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto scriptReqData = LoadScriptRequest::fromMemory(pl.data(), pl.number_of_bytes());
        // reply before invoking callback so master is not blocked longer than needed
        Private::replyDoneSlice(*req, true);
        // set script
        if (d->loadScriptCb && !scriptReqData.script.isEmpty())
            d->loadScriptCb(scriptReqData.script, scriptReqData.workingDir);
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
            bool skip = false;
            for (const auto &op : d->outPortInfo) {
                if (op->id() == opc.id)
                    skip = true;
            }
            if (skip)
                continue;

            auto oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));
            oport->d->ioxPub = SyPublisher::create(*d->node, d->modId, oport->d->ipcChannelId());
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
        const auto portId = QString::fromUtf8(r.portId.unchecked_access().c_str());

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
        iport->d->ioxSub.emplace(
            SySubscriber::create(
                *d->node,
                std::string(r.instanceId.unchecked_access().c_str()),
                std::string(r.channelId.unchecked_access().c_str())));

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

    // ---- PrepareStart ----
    while (true) {
        auto req = safeReceive(*d->srvPrepareStart);
        if (!req.has_value())
            break;
        const auto pl = req->payload();
        const auto prepReq = PrepareStartRequest::fromMemory(pl.data(), pl.number_of_bytes());
        const auto prepareSettings = prepReq.settings;
        Private::replyDoneSlice(*req, true); // reply before callback so master is not blocked
        if (d->prepareStartCb)
            d->prepareStartCb(prepareSettings);
    }

    // ---- Start ----
    while (true) {
        auto req = safeReceive(*d->srvStart);
        if (!req.has_value())
            break;
        // NOTE: We reply immediately here and defer processing of the call,
        // so the master will not wait for us. Errors are reported exclusively
        // via the error channel.

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

        Private::replyDone(*req, true);
        if (d->startCb)
            d->startCb();
    }

    // ---- Stop ----
    while (true) {
        auto req = safeReceive(*d->srvStop);
        if (!req.has_value())
            break;
        // we wait for the stop callback to finish before responding
        if (d->stopCb)
            d->stopCb();
        Private::replyDone(*req, true);

        // After a stop, drop all input-port subscribers so upstream publishers
        // immediately stop sending notifications to us. This prevents their event
        // socket buffers from filling up while we are in IDLE state in case for
        // whatever reason the master still tries to send something.
        for (auto &iport : d->inPortInfo) {
            iport->d->ioxGuard.reset();
            iport->d->ioxSub.reset();
            iport->d->connected = false;
        }
        if (!d->inPortInfo.empty())
            d->waitSetDirty = true;
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
        const auto pl = req->payload();
        const auto showReq = ShowSettingsRequest::fromMemory(pl.data(), pl.number_of_bytes());
        const QByteArray settingsData = showReq.settings;
        Private::replyDoneSlice(*req, true);
        if (d->showSettingsCb)
            d->showSettingsCb(settingsData);
    }
}

void SyntalosLink::awaitData(int timeoutUsec, const std::function<void()> &eventFn)
{
    // If a shutdown request was processed, we should die ASAP and this function
    // should no longer run - otherwise we may block forever and get killed
    if (d->shutdownPending)
        return;

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
            // Rebuild the WaitSet if ports were connected/disconnected since the last iteration
            if (d->waitSetDirty)
                d->rebuildWaitSet();

            // we do not use wait() here as some functionality depends on the Qt/GLib event loop, and especially
            // for Python users it can be a bit jarring if that is not available. So we will occasionally
            // process events here.
            handleRunResult(
                d->waitSet->wait_and_process_once_with_timeout(onEvent, iox2::bb::Duration::from_millis(250)));
            if (eventFn)
                eventFn();

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

    while (true) {
        // Rebuild the WaitSet if ports were connected/disconnected since the last iteration
        // (processPendingControl() sets waitSetDirty when that happens).
        if (d->waitSetDirty)
            d->rebuildWaitSet();

        const auto res = d->waitSet->wait_and_process_once_with_timeout(
            onEvent, iox2::bb::Duration::from_micros(intervalUsec));
        if (!res.has_value()) {
            qDebug().noquote() << "Event loop terminated unexpectedly:" << iox2::bb::into<const char *>(res.error());
            return;
        }
        // Treat SIGTERM/SIGINT as a shutdown request
        const auto r = res.value();
        if (r == iox2::WaitSetRunResult::Interrupt || r == iox2::WaitSetRunResult::TerminationRequest)
            d->shutdownPending = true;

        // call external event function
        if (eventFn)
            eventFn();

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

void SyntalosLink::setState(ModuleState state)
{
    auto uninit = d->pubState->loan_uninit().value();
    uninit.payload_mut().state = state;
    iox2::send(iox2::assume_init(std::move(uninit))).value();
    d->notifyMaster();

    d->state = state;
}

void SyntalosLink::setStatusMessage(const QString &message)
{
    auto uninit = d->pubStatusMsg->loan_uninit().value();
    uninit.payload_mut().text = iox2::bb::StaticString<512>::from_utf8_null_terminated_unchecked_truncated(
        message.toUtf8().constData(), message.toUtf8().size());
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

void SyntalosLink::setPrepareStartCallback(PrepareStartFn callback)
{
    d->prepareStartCb = std::move(callback);
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

void SyntalosLink::setSettingsData(const QByteArray &data)
{
    // we copy twice here - but this is a low-volume event, so it should be fine
    const auto scEvData = SettingsChangeEvent(data).toBytes();
    auto uninit = d->pubSettingsChange->loan_slice_uninit(static_cast<uint64_t>(scEvData.size())).value();
    auto initialized = uninit.write_from_fn([&](uint64_t i) {
        return static_cast<std::byte>(scEvData[static_cast<int>(i)]);
    });
    iox2::send(std::move(initialized)).value();
    d->notifyMaster();
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

std::shared_ptr<InputPortInfo> SyntalosLink::registerInputPort(
    const QString &id,
    const QString &title,
    const QString &dataTypeName)
{
    // construct our reference for this port
    InputPortChange ipc(PortAction::ADD);
    ipc.id = id;
    ipc.title = title;
    ipc.dataTypeId = BaseDataType::typeIdFromString(dataTypeName.toStdString());

    // check for duplicates
    for (const auto &ip : d->inPortInfo) {
        if (ip->id() == ipc.id)
            return nullptr;
    }

    const auto iportData = ipc.toBytes();

    // announce the new port to master
    auto uninit = d->pubInPortChange->loan_slice_uninit(static_cast<uint64_t>(iportData.size())).value();
    iox2::send(uninit.write_from_fn([&](uint64_t i) {
        return static_cast<std::byte>(iportData[static_cast<int>(i)]);
    })).value();
    d->notifyMaster();

    // we need to rebuild the waitset
    d->waitSetDirty = true;

    // construct proxy
    auto iport = std::shared_ptr<InputPortInfo>(new InputPortInfo(ipc));
    d->inPortInfo.push_back(iport);
    return iport;
}

std::shared_ptr<OutputPortInfo> SyntalosLink::registerOutputPort(
    const QString &id,
    const QString &title,
    const QString &dataTypeName,
    const QVariantHash &metadata)
{
    // construct our reference for this port
    OutputPortChange opc(PortAction::ADD);
    opc.id = id;
    opc.title = title;
    opc.dataTypeId = BaseDataType::typeIdFromString(dataTypeName.toStdString());
    opc.metadata = metadata;

    // check for duplicates
    for (const auto &op : d->outPortInfo) {
        if (op->id() == opc.id)
            return nullptr;
    }

    const auto oportData = opc.toBytes();

    // announce the new port to master
    auto uninit = d->pubOutPortChange->loan_slice_uninit(static_cast<uint64_t>(oportData.size())).value();
    iox2::send(uninit.write_from_fn([&](uint64_t i) {
        return static_cast<std::byte>(oportData[static_cast<int>(i)]);
    })).value();
    d->notifyMaster();

    // we need to rebuild the waitset
    d->waitSetDirty = true;

    // construct proxy
    auto oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));
    oport->d->ioxPub = SyPublisher::create(*d->node, d->modId, oport->d->ipcChannelId());
    d->outPortInfo.push_back(oport);
    return oport;
}

void SyntalosLink::updateOutputPort(const std::shared_ptr<OutputPortInfo> &oport)
{
    OutputPortChange opc(PortAction::CHANGE);
    opc.id = oport->id();
    opc.title = oport->d->title;
    opc.dataTypeId = oport->dataTypeId();
    opc.metadata = oport->d->metadata;

    const auto oportData = opc.toBytes();
    auto uninit = d->pubOutPortChange->loan_slice_uninit(static_cast<uint64_t>(oportData.size())).value();
    // we copy twice here - but this is a low-volume event, so it should be fine
    iox2::send(uninit.write_from_fn([&](uint64_t i) {
        return static_cast<std::byte>(oportData[static_cast<int>(i)]);
    })).value();
    d->notifyMaster();
}

void SyntalosLink::updateInputPort(const std::shared_ptr<InputPortInfo> &iport)
{
    InputPortChange ipc(PortAction::CHANGE);
    ipc.id = iport->id();
    ipc.title = iport->d->title;
    ipc.dataTypeId = iport->d->dataTypeId;
    ipc.metadata = iport->d->metadata;
    ipc.throttleItemsPerSec = iport->d->throttleItemsPerSec;

    const auto iportData = ipc.toBytes();
    auto uninit = d->pubInPortChange->loan_slice_uninit(static_cast<uint64_t>(iportData.size())).value();
    // we copy twice here - but this is a low-volume event, so it should be fine
    iox2::send(uninit.write_from_fn([&](uint64_t i) {
        return static_cast<std::byte>(iportData[static_cast<int>(i)]);
    })).value();
    d->notifyMaster();
}

bool SyntalosLink::submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const BaseDataType &data)
{
    if (!oport->d->ioxPub.has_value()) {
        raiseError(
            QStringLiteral("Failed to send data on output port '%1': Publisher was not initialized!").arg(oport->id()));
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
                raiseError(QStringLiteral("Failed to serialize data for output port '%1'.").arg(oport->id()));
                return false;
            }
            pub.sendSlice(std::move(loan));
        }
    } catch (std::exception &e) {
        raiseError(QStringLiteral("Failed to send data on output port '%1': %2")
                       .arg(oport->id(), QString::fromUtf8(e.what())));
        return false;
    }

    return true;
}

} // namespace Syntalos
