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

#pragma once

#include <chrono>
#include <optional>

#include <QObject>
#include <QString>

#include "datactl/uuid.h"
#include "fabric/globalconfig.h"

namespace Syntalos
{
class AbstractModule;
class Engine;
} // namespace Syntalos

/**
 * @brief ZeroMQ-based network coordinator for synchronized multi-device runs.
 *
 * Owned by Engine.  In controller mode the engine calls the on-controller-*
 * methods at the respective points in runInternal; the controller broadcasts
 * these over the network and optionally waits for ACKs from listeners.
 * In listener mode incoming commands are applied to the Engine
 * (subject override, attribute injection) and the MainWindow is notified
 * via signals to trigger / stop runs.
 *
 * Protocol:
 *   Commands  - PUB (controller binds cmd_port) / SUB (listener connects)
 *   Feedback  - PULL (controller binds fb_port) / PUSH (listener connects)
 *
 * ACK timing (listener side):
 *   "prepare" ACK -> sent after Engine::runReadyToStart (or on Engine::runFailed);
 *                    this tells the controller "I am prepared and waiting for START".
 *   "stop"    ACK -> sent after Engine::runStopped
 *   "start"   is informational only — ACKed immediately on receipt; listener stores
 *                    the controller's timestamps in EDL for cross-device alignment.
 */
class NetworkController : public QObject
{
    Q_OBJECT

public:
    explicit NetworkController(Syntalos::GlobalConfig *gconf, Syntalos::Engine *engine, QObject *parent = nullptr);
    ~NetworkController() override;

    bool startControllerMode();
    void stopControllerMode();
    bool startListenerMode();
    void stopListenerMode();

    bool isControllerActive() const;
    bool isListenerActive() const;

    // True while the current engine run was remotely triggered.
    // Engine uses this to suppress controller broadcasts during remote runs.
    bool isRunOriginRemote() const;

    /**
     * Called from the engine just before starting the timer.
     * If this instance is a network listener in a remote run, blocks (processing
     * Qt events) until the controller's START command arrives, then returns the
     * wall-clock reference time via outWallTime so the engine can call
     * SyncTimer::startAtWallTime() for cross-device alignment.
     *
     * Returns true immediately with outWallTime=nullopt if not in listener mode.
     * Returns false on timeout (30 s) - caller should abort the run.
     */
    bool waitForStartCommand(
        const Syntalos::Uuid &runId,
        std::optional<std::chrono::system_clock::time_point> &outWallTime);

    /**
     * Send PREPARE over the network, if network-mode is configured, and wait for all
     * participants to ACK the request.
     * Called synchronously from inside runInternal / stop path.
     *
     * @return True on success, False if an error was emitted.
     */
    bool broadcastPrepare(
        const Syntalos::Uuid &runId,
        const QString &subjectId,
        const QString &subjectGroup,
        const QString &experimentId);

    /**
     * Broadcast START.
     * Called synchronously from inside runInternal / stop path.
     */
    void broadcastStart(const Syntalos::Uuid &runId, qint64 startTimeUnixUs);

    /**
     * Broadcast STOP.
     * Called synchronously from inside runInternal / stop path.
     */
    void broadcastStop(const Syntalos::Uuid &runId, bool success);

    /**
     * Reset all per-run listener bookkeeping. Engine calls this from
     * abort paths in runInternal that bypass the normal runStopped
     * / runFailed signal flow, so a subsequent remote prepare starts
     * from a clean slate.
     */
    void resetListenerRunState();

    // Set project name (if we have any) to include in PREPARE broadcast
    void setProjectId(const QString &id);

signals:
    // Listener received a "prepare" command; Engine state (subject, experiment,
    // extra attributes) has already been updated.  MainWindow should call
    // runActionTriggered() in response.
    void prepareReceived(const Syntalos::Uuid &runId);

    // Listener received a "stop" command.  MainWindow should call
    // stopActionTriggered() in response.
    void stopReceived(const Syntalos::Uuid &runId);

    /**
     * An error occured and we can not continue.
     */
    void errorMessage(const QString &msg);

    /**
     * Status information that is user-facing.
     */
    void statusMessage(const QString &msg);

private slots:
    // Engine callbacks for the listener ACK path
    void onListenerRunReadyToStart();
    void onListenerRunStarted();
    void onListenerRunStopped();
    void onListenerRunFailed(Syntalos::AbstractModule *mod, const QString &message);

private:
    class Private;
    Q_DISABLE_COPY(NetworkController)
    std::unique_ptr<Private> d;
};
