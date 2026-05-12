/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "networkcontroller.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <zmq.hpp>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "engine.h"
#include "fabric/logging.h"
#include "fabric/utils/misc.h"
#include "datactl/syclock.h"
#include "datactl/streammeta.h"

using namespace Syntalos;

/**
 * Internal command codes sent from the main thread to the worker
 */
enum class CommandKind : uint8_t {
    Shutdown = 0,
    Broadcast = 1,
    SendAck = 2
};

// Carries the result of one ACK received on the controller's PULL socket.
struct AckResult {
    bool success{true};
    QString error;
    QString sender;
    QString phase;
    Uuid runId;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class NetworkController::Private
{
public:
    Private() = default;
    ~Private() = default;

    QuillLogger *log{nullptr};
    Engine *engine{nullptr};
    NetworkController *self{nullptr};

    NetworkControlConfig config;

    // mode flags (main-thread only)
    bool controllerActive{false};
    bool listenerActive{false};

    // true while the current engine run was triggered by a remote prepare command
    bool runOriginRemote{false};
    // controller's run_id received in the most recent remote prepare
    Uuid remoteRunId;
    // true while we are waiting for the engine to start so we can send a prepare ACK
    bool listenerPrepareAckPending{false};
    // true while we are waiting for the engine to stop so we can send a stop ACK
    bool listenerStopAckPending{false};

    // a (human-friendly) project name included in controller prepare broadcasts
    QString projectId;

    // ZMQ context shared across all sockets
    zmq::context_t ctx{1};
    // inproc endpoint for main->worker commands; unique per instance
    std::string ctrlEndpoint;
    // main-thread side of the control PAIR socket
    std::optional<zmq::socket_t> ctrlSend;

    // worker thread
    std::thread workerThread;
    std::atomic_bool workerRunning{false};

    Uuid currentRunId;

    // ACK tracking for controller prepare phase (main thread only)
    std::atomic_int pendingPrepareAckCount{0};
    std::vector<AckResult> pendingPrepareAckResults; // accumulated error ACKs

    // ACK tracking for controller start phase (async - checked after timeout)
    std::atomic_int pendingStartAckCount{0};

    // Set by onRecvStartCommand; read by waitForStartCommand on the main thread
    std::optional<std::chrono::system_clock::time_point> pendingStartWallTime;

    void sendCtrlMsg(CommandKind kind, const QByteArray &payload = {})
    {
        if (!ctrlSend)
            return;

        try {
            const auto kindByte = static_cast<uint8_t>(kind);
            ctrlSend->send(
                zmq::buffer(&kindByte, 1),
                payload.isEmpty() ? zmq::send_flags::none : zmq::send_flags::sndmore);
            if (!payload.isEmpty())
                ctrlSend->send(
                    zmq::buffer(payload.constData(), static_cast<size_t>(payload.size())),
                    zmq::send_flags::none);
        } catch (const zmq::error_t &e) {
            LOG_WARNING(log, "Control send failed: {}", e.what());
        }
    }

    QByteArray buildPrepareJson(
        const Uuid &runId,
        const QString &subjectId,
        const QString &subjectGroup,
        const QString &experimentId) const
    {
        QJsonObject obj;
        obj[QStringLiteral("v")] = 1;
        obj[QStringLiteral("type")] = QStringLiteral("prepare");
        obj[QStringLiteral("sender")] = config.instanceId;
        obj[QStringLiteral("run_id")] = QString::fromStdString(runId.toHex());
        obj[QStringLiteral("project")] = projectId;
        obj[QStringLiteral("subject_id")] = subjectId;
        obj[QStringLiteral("subject_group")] = subjectGroup;
        obj[QStringLiteral("experiment_id")] = experimentId;

        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QByteArray buildStartJson(const Uuid &runId, qint64 startTimeUnixUs) const
    {
        QJsonObject obj;
        obj[QStringLiteral("v")] = 1;
        obj[QStringLiteral("type")] = QStringLiteral("start");
        obj[QStringLiteral("sender")] = config.instanceId;
        obj[QStringLiteral("run_id")] = QString::fromStdString(runId.toHex());
        obj[QStringLiteral("ts_start_us")] = startTimeUnixUs;

        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QByteArray buildStopJson(const Uuid &runId, bool success) const
    {
        QJsonObject obj;
        obj[QStringLiteral("v")] = 1;
        obj[QStringLiteral("type")] = QStringLiteral("stop");
        obj[QStringLiteral("sender")] = config.instanceId;
        obj[QStringLiteral("run_id")] = QString::fromStdString(runId.toHex());
        obj[QStringLiteral("success")] = success;

        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QByteArray buildAckJson(const QString &ackFor, const Uuid &runId, bool success, const QString &error = {}) const
    {
        QJsonObject obj;
        obj[QStringLiteral("v")] = 1;
        obj[QStringLiteral("type")] = QStringLiteral("ack");
        obj[QStringLiteral("sender")] = config.instanceId;
        obj[QStringLiteral("run_id")] = QString::fromStdString(runId.toHex());
        obj[QStringLiteral("ack_for")] = ackFor;
        obj[QStringLiteral("success")] = success;
        if (!error.isEmpty())
            obj[QStringLiteral("error")] = error;

        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    // Send an ACK from main thread by routing through the worker's inproc channel.
    void sendAckReply(const QString &ackFor, const Uuid &runId, bool success, const QString &error = {})
    {
        const auto payload = buildAckJson(ackFor, runId, success, error);
        sendCtrlMsg(CommandKind::SendAck, payload);
    }

    void workerFunc(bool doController, bool doListener)
    {
        const auto instanceId = config.instanceId.toStdString();
        const int cmdPort = config.controlPort;
        const int fbPort = config.feedbackPort;
        const auto host = config.controlHost.toStdString();

        try {
            zmq::socket_t ctrlRecv(ctx, zmq::socket_type::pair);
            ctrlRecv.set(zmq::sockopt::linger, 0);
            ctrlRecv.connect(ctrlEndpoint);

            std::optional<zmq::socket_t> pubSock;
            std::optional<zmq::socket_t> pullSock;
            std::optional<zmq::socket_t> subSock;
            std::optional<zmq::socket_t> pushSock;

            if (doController) {
                pubSock.emplace(ctx, zmq::socket_type::pub);
                pubSock->set(zmq::sockopt::linger, 0);
                pubSock->bind(std::string("tcp://*:") + std::to_string(cmdPort));

                pullSock.emplace(ctx, zmq::socket_type::pull);
                pullSock->set(zmq::sockopt::linger, 0);
                pullSock->bind(std::string("tcp://*:") + std::to_string(fbPort));

                // Allow any pre-existing subscribers time to re-establish their TCP
                // connections and send their SUBSCRIBE control frames before the first
                // broadcast.  Without this sleep the first PUB message can be lost
                // (ZMQ "slow joiner" problem) when a subscriber was already waiting
                // on the port before we bound.
                std::this_thread::sleep_for(std::chrono::milliseconds(600));

                LOG_INFO(log, "Controller mode active - cmd:{} fb:{}", cmdPort, fbPort);
            }

            if (doListener) {
                subSock.emplace(ctx, zmq::socket_type::sub);
                subSock->set(zmq::sockopt::linger, 0);
                subSock->set(zmq::sockopt::subscribe, std::string("sy.cmd"));
                subSock->connect(std::string("tcp://") + host + ":" + std::to_string(cmdPort));

                pushSock.emplace(ctx, zmq::socket_type::push);
                pushSock->set(zmq::sockopt::linger, 0);
                pushSock->connect(std::string("tcp://") + host + ":" + std::to_string(fbPort));

                // wait for the SUB socket to settle before receiving messages
                std::this_thread::sleep_for(std::chrono::milliseconds(200));

                LOG_INFO(log, "Listener mode active - controller: {}:{}", host, cmdPort);
            }

            zmq::pollitem_t pollItems[3];
            int nPollItems = 0;
            pollItems[nPollItems++] = {ctrlRecv.handle(), 0, ZMQ_POLLIN, 0};

            const int pullIdx = pullSock ? nPollItems : -1;
            if (pullSock)
                pollItems[nPollItems++] = {pullSock->handle(), 0, ZMQ_POLLIN, 0};
            const int subIdx = subSock ? nPollItems : -1;
            if (subSock)
                pollItems[nPollItems++] = {subSock->handle(), 0, ZMQ_POLLIN, 0};

            while (workerRunning) {
                zmq::poll(pollItems, nPollItems, std::chrono::milliseconds(250));

                // ctrl commands from main thread
                if (pollItems[0].revents & ZMQ_POLLIN) {
                    zmq::message_t cmdMsg;
                    if (!ctrlRecv.recv(cmdMsg)) {
                        LOG_WARNING(log, "Ctrl recv returned no data");
                        continue;
                    }
                    if (cmdMsg.size() < 1) {
                        LOG_WARNING(log, "Ctrl recv: empty command frame");
                        continue;
                    }
                    const auto cmdKind = static_cast<CommandKind>(*static_cast<uint8_t *>(cmdMsg.data()));

                    bool doShutdown = false;
                    switch (cmdKind) {
                    case CommandKind::Shutdown:
                        doShutdown = true;
                        break;
                    case CommandKind::Broadcast:
                        if (pubSock) {
                            zmq::message_t payload;
                            if (!ctrlRecv.recv(payload)) {
                                LOG_WARNING(log, "Failed to receive broadcast payload, skipping");
                            } else {
                                try {
                                    zmq::message_t topicMsg("sy.cmd", 6);
                                    pubSock->send(topicMsg, zmq::send_flags::sndmore);
                                    pubSock->send(payload, zmq::send_flags::none);
                                } catch (const zmq::error_t &e) {
                                    LOG_WARNING(log, "Failed to broadcast command: {}", e.what());
                                }
                            }
                        }
                        break;
                    case CommandKind::SendAck:
                        if (pushSock) {
                            zmq::message_t payload;
                            if (!ctrlRecv.recv(payload)) {
                                LOG_WARNING(log, "Failed to receive ACK payload, skipping");
                            } else {
                                try {
                                    pushSock->send(payload, zmq::send_flags::dontwait);
                                } catch (const zmq::error_t &e) {
                                    LOG_WARNING(log, "Failed to send ACK: {}", e.what());
                                }
                            }
                        }
                        break;
                    default:
                        LOG_WARNING(log, "Unknown ctrl command: {}", static_cast<int>(cmdKind));
                        zmq::message_t extra;
                        while (ctrlRecv.get(zmq::sockopt::rcvmore)) {
                            [[maybe_unused]] auto rd = ctrlRecv.recv(extra);
                        }
                        break;
                    }
                    if (doShutdown)
                        break;
                }

                // incoming ACK from a listener (controller PULL)
                if (pullIdx >= 0 && (pollItems[pullIdx].revents & ZMQ_POLLIN)) {
                    zmq::message_t msg;
                    if (!pullSock->recv(msg)) {
                        LOG_WARNING(log, "PULL recv returned no data");
                        continue;
                    }
                    const QByteArray data(static_cast<char *>(msg.data()), static_cast<int>(msg.size()));
                    handleAck(data);
                }

                // incoming command from controller (listener SUB)
                if (subIdx >= 0 && (pollItems[subIdx].revents & ZMQ_POLLIN)) {
                    zmq::message_t topicMsg;
                    if (!subSock->recv(topicMsg)) {
                        LOG_WARNING(log, "SUB topic recv returned no data");
                        continue;
                    }
                    const std::string topic(static_cast<char *>(topicMsg.data()), topicMsg.size());
                    zmq::message_t payloadMsg;
                    if (!subSock->recv(payloadMsg)) {
                        LOG_WARNING(log, "SUB payload recv returned no data");
                        continue;
                    }
                    if (topic != "sy.cmd") {
                        LOG_DEBUG(log, "Ignored message with unexpected topic: {}", topic);
                        continue;
                    }
                    const QByteArray data(static_cast<char *>(payloadMsg.data()), static_cast<int>(payloadMsg.size()));
                    handleCommand(data, instanceId, pushSock);
                }
            }

        } catch (const zmq::error_t &e) {
            LOG_ERROR(log, "Worker error: {}", e.what());
            QMetaObject::invokeMethod(
                self,
                [this, msg = QString::fromLatin1(e.what())]() {
                    Q_EMIT self->errorMessage(msg);
                },
                Qt::QueuedConnection);
        }

        workerRunning = false;
        LOG_DEBUG(log, "Worker thread exited");
    }

    void handleAck(const QByteArray &data)
    {
        auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
            return;
        const auto obj = doc.object();
        if (obj.value(QStringLiteral("v")).toInt() != 1) {
            LOG_WARNING(log, "Ignoring ACK with invalid version: {}", obj.value(QStringLiteral("v")).toInt());
            return;
        }
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("ack"))
            return;

        const auto phase = obj.value(QStringLiteral("ack_for")).toString();
        const auto sender = obj.value(QStringLiteral("sender")).toString();
        const auto success = obj.value(QStringLiteral("success")).toBool(true);
        const auto error = obj.value(QStringLiteral("error")).toString(QStringLiteral("Error unknown"));
        const auto runIdOpt = Uuid::fromHex(obj.value(QStringLiteral("run_id")).toString().toStdString());
        if (!runIdOpt)
            return;

        if (*runIdOpt != currentRunId) {
            LOG_DEBUG(log, "Ignoring ACK for run {} (current is {})", (*runIdOpt).toHex(), currentRunId.toHex());
            return;
        }

        if (phase == QStringLiteral("start")) {
            ++pendingStartAckCount;
        } else if (phase == QStringLiteral("prepare")) {
            ++pendingPrepareAckCount;
        }

        // errors on start ACK are reported on the main thread
        if (!success) {
            AckResult res;
            res.success = false;
            res.error = error;
            res.sender = sender;
            res.phase = phase;
            res.runId = *runIdOpt;
            // we post the pending error results to the main thread so they can be handled
            QMetaObject::invokeMethod(
                self,
                [this, res]() mutable {
                    pendingPrepareAckResults.push_back(std::move(res));
                },
                Qt::QueuedConnection);
        }
    }

    void handleCommand(const QByteArray &data, const std::string &ownInstanceId, std::optional<zmq::socket_t> &pushSock)
    {
        auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject())
            return;
        const auto obj = doc.object();
        if (obj.value(QStringLiteral("v")).toInt() != 1)
            return;

        const auto sender = obj.value(QStringLiteral("sender")).toString().toStdString();
        if (sender == ownInstanceId)
            return; // self-broadcast filter

        const auto type = obj.value(QStringLiteral("type")).toString();
        const auto runIdOpt = Uuid::fromHex(obj.value(QStringLiteral("run_id")).toString().toStdString());
        if (!runIdOpt)
            return;
        const auto runId = *runIdOpt;

        if (type == QStringLiteral("prepare")) {
            const auto project = obj.value(QStringLiteral("project")).toString();
            const auto subjectId = obj.value(QStringLiteral("subject_id")).toString();
            const auto subjectGroup = obj.value(QStringLiteral("subject_group")).toString();
            const auto experimentId = obj.value(QStringLiteral("experiment_id")).toString();

            // Dispatch to main thread - prepareReceived() triggers the engine run.
            // ACK is sent later (by onListenerRunReadyToStart / onListenerRunFailed).
            QMetaObject::invokeMethod(
                self,
                [this, runId, project, subjectId, subjectGroup, experimentId, sender]() {
                    onRecvPrepareCommand(runId, project, subjectId, subjectGroup, experimentId, sender);
                },
                Qt::QueuedConnection);

        } else if (type == QStringLiteral("start")) {
            // ACK immediately - the listener is already running by this point
            if (pushSock) {
                const auto ackPayload = buildAckJson(QStringLiteral("start"), runId, true);
                try {
                    pushSock->send(
                        zmq::buffer(ackPayload.constData(), static_cast<size_t>(ackPayload.size())),
                        zmq::send_flags::dontwait);
                } catch (const zmq::error_t &e) {
                    LOG_WARNING(log, "Failed to send start ACK: {}", e.what());
                }
            }

            const qint64 startTimeUnixUs = static_cast<qint64>(obj.value(QStringLiteral("ts_start_us")).toDouble());
            QMetaObject::invokeMethod(
                self,
                [this, runId, startTimeUnixUs]() {
                    onRecvStartCommand(runId, startTimeUnixUs);
                },
                Qt::QueuedConnection);

        } else if (type == QStringLiteral("stop")) {
            // Dispatch to main thread - stopReceived() tells MainWindow to call stopActionTriggered().
            // ACK is sent later (by onListenerRunStopped).
            QMetaObject::invokeMethod(
                self,
                [this, runId]() {
                    onRecvStopCommand(runId);
                },
                Qt::QueuedConnection);
        }
    }

    /**
     * Clears all per-run listener bookkeeping. Called both before applying a
     * new prepare and from any path that might otherwise leave stale state
     * (failed run, listener mode toggled off).
     */
    void resetListenerRunState()
    {
        runOriginRemote = false;
        remoteRunId = Uuid{};
        listenerPrepareAckPending = false;
        listenerStopAckPending = false;
        pendingStartWallTime.reset();
    }

    void onRecvPrepareCommand(
        const Uuid &runId,
        const QString &project,
        const QString &subjectId,
        const QString &subjectGroup,
        const QString &experimentId,
        const std::string &triggerInstanceId)
    {
        if (engine->isActive()) {
            // Instance is already busy - reject with an error ACK immediately
            sendAckReply("prepare", runId, false, QStringLiteral("Instance is already running an experiment"));
            LOG_WARNING(log, "Received prepare for run {} while instance is already running", runId.toHex());
            return;
        }

        // Defensively clear any stray state from a previous (possibly aborted) run
        resetListenerRunState();

        // Mark this run as remotely triggered so the controller path stays quiet
        runOriginRemote = true;
        remoteRunId = runId;
        listenerPrepareAckPending = true;

        // Apply controller's run parameters to the local engine
        TestSubject ts;
        ts.id = subjectId;
        ts.group = subjectGroup;
        engine->setTestSubject(ts);
        engine->setExperimentId(experimentId);

        // Add some extra metadata about the remote-trigger event
        MetaStringMap netMeta;
        MetaStringMap metaRemote;
        metaRemote["project"] = project.toStdString();
        metaRemote["launched_by"] = triggerInstanceId;
        netMeta["remote"] = metaRemote;
        engine->addNextRunExtraAttrMetadata(netMeta);

        // Signal to allow connected slots to actually launch the run (in this case, we go through MainWindow)
        Q_EMIT self->prepareReceived(runId);
    }

    void onRecvStartCommand(const Uuid &runId, qint64 startTimeUnixUs)
    {
        if (!runOriginRemote || runId != remoteRunId) {
            LOG_DEBUG(
                log,
                "Received START for run {}, current run is for {} or not remote, ignoring",
                runId.toHex(),
                remoteRunId.toHex());
            return;
        }

        // Store the controller's wall-clock start time so waitForStartCommand() can
        // return it to the engine.  SyncTimer::startAtWallTime() will back-compute
        // the local master-clock start from this reference so timestamps from all
        // fleet members share a common t=0.
        pendingStartWallTime = std::chrono::system_clock::time_point(microseconds_t(startTimeUnixUs));
    }

    void onRecvStopCommand(const Uuid &runId)
    {
        if (!runOriginRemote || runId != remoteRunId) {
            LOG_DEBUG(
                log,
                "Received STOP for run {}, current run is for {} or not remote, ignoring",
                runId.toHex(),
                remoteRunId.toHex());
            return;
        }
        listenerStopAckPending = true;
        Q_EMIT self->stopReceived(runId);
    }

    void stopWorker()
    {
        if (workerRunning.load())
            sendCtrlMsg(CommandKind::Shutdown);
        if (workerThread.joinable())
            workerThread.join();
        ctrlSend.reset();
    }

    void startWorker()
    {
        // Generate a fresh endpoint each time to avoid "address already in use"
        // errors when inproc sockets from a previous worker are still cleaning up.
        ctrlEndpoint = "inproc://sy-netctl-" + createRandomString(10).toStdString();

        ctrlSend.emplace(ctx, zmq::socket_type::pair);
        ctrlSend->set(zmq::sockopt::linger, 0);
        ctrlSend->bind(ctrlEndpoint);

        workerRunning = true;
        const bool ctrl = controllerActive;
        const bool lst = listenerActive;
        workerThread = std::thread([this, ctrl, lst]() {
            workerFunc(ctrl, lst);
        });
    }
};
#pragma GCC diagnostic pop

NetworkController::NetworkController(Engine *engine, QObject *parent)
    : QObject(parent),
      d(new NetworkController::Private)
{
    d->self = this;
    d->log = getLogger("netctl");
    d->engine = engine;

    // Connect to Engine signals for listener-side ACK sending
    connect(engine, &Engine::runReadyToStart, this, &NetworkController::onListenerRunReadyToStart);
    connect(engine, &Engine::runStarted, this, &NetworkController::onListenerRunStarted);
    connect(engine, &Engine::runStopped, this, &NetworkController::onListenerRunStopped);
    connect(engine, &Engine::runFailed, this, &NetworkController::onListenerRunFailed);
}

NetworkController::~NetworkController()
{
    d->controllerActive = false;
    d->listenerActive = false;
    d->stopWorker();
}

bool NetworkController::startControllerMode()
{
    if (d->controllerActive)
        return true;

    d->stopWorker();
    d->controllerActive = true;
    d->startWorker();

    return true;
}

void NetworkController::stopControllerMode()
{
    if (!d->controllerActive)
        return;

    d->controllerActive = false;
    d->stopWorker();
    if (d->listenerActive)
        d->startWorker();
}

bool NetworkController::startListenerMode()
{
    if (d->listenerActive)
        return true;

    d->stopWorker();
    d->listenerActive = true;
    d->startWorker();

    return true;
}

void NetworkController::stopListenerMode()
{
    if (!d->listenerActive)
        return;

    d->listenerActive = false;
    d->resetListenerRunState();
    d->stopWorker();
    if (d->controllerActive)
        d->startWorker();
}

bool NetworkController::isControllerActive() const
{
    return d->controllerActive;
}

bool NetworkController::isListenerActive() const
{
    return d->listenerActive;
}

bool NetworkController::isRunOriginRemote() const
{
    return d->runOriginRemote;
}

bool NetworkController::waitForStartCommand(
    const Uuid &runId,
    std::optional<std::chrono::system_clock::time_point> &outWallTime)
{
    outWallTime.reset();

    if (!d->listenerActive || !d->runOriginRemote || runId != d->remoteRunId)
        return true; // not applicable - caller starts timer normally

    LOG_INFO(d->log, "Waiting for network START command from controller...");
    Q_EMIT statusMessage(QStringLiteral("Waiting for START command from the network..."));

    QDeadlineTimer deadline(30000);
    while (!d->pendingStartWallTime.has_value() && !d->engine->isStopRequested()) {
        if (deadline.hasExpired()) {
            LOG_WARNING(d->log, "Timed out waiting for network START command after 30s");
            return false;
        }
        qApp->processEvents(QEventLoop::AllEvents);
    }

    outWallTime = d->pendingStartWallTime;
    return true;
}

void NetworkController::setProjectId(const QString &id)
{
    d->projectId = id;
}

void NetworkController::applyConfig(const NetworkControlConfig &config)
{
    d->config = config;

    // If the worker is already running, restart it so the new ports / host take effect.
    if (d->workerRunning.load()) {
        const bool ctrl = d->controllerActive;
        const bool lst = d->listenerActive;
        d->stopWorker();
        if (ctrl || lst)
            d->startWorker();
    }
}

bool NetworkController::broadcastPrepare(
    const Uuid &runId,
    const QString &subjectId,
    const QString &subjectGroup,
    const QString &experimentId)
{
    if (!d->controllerActive || d->runOriginRemote)
        return true;
    if (d->engine->isRunEphemeral())
        return true;

    d->currentRunId = runId;
    d->pendingPrepareAckCount = 0;
    d->pendingPrepareAckResults.clear();

    const auto payload = d->buildPrepareJson(runId, subjectId, subjectGroup, experimentId);
    d->sendCtrlMsg(CommandKind::Broadcast, payload);

    LOG_INFO(
        d->log,
        "Broadcasted 'Prepare' for {}/{}, waiting for {} participants",
        d->projectId,
        runId.toHex(),
        d->config.expectedClientCount);
    if (d->config.expectedClientCount <= 0)
        return true;

    // Block (processing Qt events) until we have enough ACKs or time out
    QDeadlineTimer deadline(d->config.controlTimeoutMs);
    while (!deadline.hasExpired()) {
        if (d->pendingPrepareAckCount >= d->config.expectedClientCount)
            break;
        qApp->processEvents(QEventLoop::AllEvents, 50);
    }

    if (d->pendingPrepareAckCount < d->config.expectedClientCount) {
        Q_EMIT errorMessage(
            QStringLiteral("Only %1 of expected %2 network participants confirmed finishing their preparations.")
                .arg(d->pendingPrepareAckCount.load())
                .arg(d->config.expectedClientCount));
        return false;
    }

    // Emit error messages for any failures reported by fleet members
    bool success = true;
    qApp->processEvents(QEventLoop::AllEvents, 50);
    for (const auto &res : d->pendingPrepareAckResults) {
        if (res.success)
            continue;
        success = false;
        Q_EMIT errorMessage(QStringLiteral("Issue on instance \"%1\": %2").arg(res.sender, res.error));
    }

    return success;
}

void NetworkController::broadcastStart(const Uuid &runId, qint64 startTimeUnixUs)
{
    if (!d->controllerActive || d->runOriginRemote)
        return;
    if (d->engine->isRunEphemeral())
        return;

    const auto payload = d->buildStartJson(runId, startTimeUnixUs);
    d->sendCtrlMsg(CommandKind::Broadcast, payload);
    LOG_INFO(d->log, "Broadcasted 'Start' for {}/{}", d->projectId, runId.toHex());

    const int expected = d->config.expectedClientCount;
    if (expected <= 0)
        return;

    d->currentRunId = runId;
    d->pendingStartAckCount = 0;

    // Passively collect start ACKs; if not enough arrive within the timeout, stop the run.
    // We never block here - the run has already started.
    QTimer::singleShot(d->config.controlTimeoutMs, this, [this, runId, expected]() {
        if (d->currentRunId != runId)
            return; // run already finished or superseded
        const int got = d->pendingStartAckCount.load();
        if (got < expected) {
            Q_EMIT errorMessage(
                QStringLiteral(
                    "Not all network participants acknowledged the 'Start' command, received only %1 of expected %2.")
                    .arg(got)
                    .arg(expected));
        }
    });
}

void NetworkController::broadcastStop(const Uuid &runId, bool success)
{
    if (!d->controllerActive || d->runOriginRemote)
        return;
    if (d->engine->isRunEphemeral())
        return;

    const auto payload = d->buildStopJson(runId, success);
    d->sendCtrlMsg(CommandKind::Broadcast, payload);
}

void NetworkController::resetListenerRunState()
{
    d->resetListenerRunState();
}

void NetworkController::onListenerRunReadyToStart()
{
    // All modules are prepared; the engine is about to enter the start barrier.
    // Send the prepare ACK now so the controller knows we are ready and can
    // proceed to broadcast START once it has collected enough prepare ACKs.
    if (!d->listenerActive || !d->runOriginRemote || !d->listenerPrepareAckPending)
        return;

    d->listenerPrepareAckPending = false;
    d->sendAckReply(QStringLiteral("prepare"), d->remoteRunId, true);
}

void NetworkController::onListenerRunStarted()
{
    // The engine has started (timer is running). The prepare ACK was already
    // sent from onListenerRunReadyToStart(). Clear any residual pending flag.
    d->listenerPrepareAckPending = false;
}

void NetworkController::onListenerRunStopped()
{
    if (!d->listenerActive || !d->runOriginRemote)
        return;

    if (d->listenerStopAckPending) {
        d->listenerStopAckPending = false;
        d->sendAckReply(QStringLiteral("stop"), d->remoteRunId, !d->engine->hasFailed());
    }
    d->runOriginRemote = false;
}

void NetworkController::onListenerRunFailed(AbstractModule * /*mod*/, const QString &message)
{
    if (!d->listenerActive || !d->runOriginRemote)
        return;

    // Surface the failure as a negative prepare ACK if we hadn't ACKed yet, so
    // the controller knows immediately that this listener can not participate.
    if (d->listenerPrepareAckPending)
        d->sendAckReply(QStringLiteral("prepare"), d->remoteRunId, false, message);

    // A failure may bypass the normal runStopped -> onListenerRunStopped path
    // (e.g. when runInternal aborts before its teardown). Reset state defensively
    // so the next remote prepare starts from a clean slate.
    d->resetListenerRunState();
}
