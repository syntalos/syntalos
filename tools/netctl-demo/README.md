# Syntalos Network Control - Demo Tools

Python scripts that speak the Syntalos network-control protocol. Useful for
manual testing, integration without a full Syntalos installation, and as a
starting reference for building your own network-aware clients.

## Prerequisites

```
pip install pyzmq
```

Python 3.12+.

## Quick start

Terminal 1 - start a listener:
```bash
python listener.py --ident my-rig
```

Terminal 2 - broadcast commands interactively:
```bash
python controller.py
# > p   ← broadcast prepare
# > s   ← broadcast start
# > x   ← broadcast stop
# > q   ← quit
```

Or run the automatic sequence (prepare → 2 s → start → 5 s → stop):
```bash
python controller.py --auto --subject M01 --group control --experiment exp-01
```

## Protocol

All messages travel over two ZeroMQ socket pairs:

| Direction         | Pattern     | Default port |
|-------------------|-------------|--------------|
| Commands (C → L)  | PUB / SUB   | 5556         |
| Feedback (L → C)  | PUSH / PULL | 5557         |

The controller **binds** both ports; listeners **connect** to them.

Every command is a two-frame ZMQ message:
- Frame 0: topic string `sy.cmd`
- Frame 1: UTF-8 JSON payload

### Common JSON fields

Every message (command or ACK) carries:

| Field    | Type   | Description                                     |
|----------|--------|-------------------------------------------------|
| `v`      | int    | Protocol version (`1`)                          |
| `type`   | string | Message type (see below)                        |
| `sender` | string | Originator's instance ID (used for self-filter) |
| `run_id` | string | UUIDv7 hex string identifying the run           |

### Command types (controller → listeners)

**`prepare`** - controller announces an upcoming run; listeners should get ready:

| Field           | Type   | Description              |
|-----------------|--------|--------------------------|
| `project`       | string | Project name (if any)    |
| `subject_id`    | string | Subject ID               |
| `subject_group` | string | Subject group            |
| `experiment_id` | string | Experiment ID            |

Listeners apply the provided subject/experiment settings and start their own recording run.
Once a listener has finished preparing and is ready to start, it sends a `prepare` ACK.

**`start`** - controller has started its run timer; listeners should align theirs:

| Field         | Type | Description                                            |
|---------------|------|--------------------------------------------------------|
| `ts_start_us` | int  | Controller wall-clock start time (µs since Unix epoch) |

Listeners use this timestamp to back-calculate a shared t=0.

This only works when all host clocks are synchronized (NTP/PTP).
Ensure time synchronization is properly set up for all machines!

**`stop`** - controller's run has ended:

| Field     | Type | Description           |
|-----------|------|-----------------------|
| `success` | bool | Whether run succeeded |

### ACK messages (listener → controller, via PUSH/PULL)

| Field     | Type   | Description                                      |
|-----------|--------|--------------------------------------------------|
| `type`    | string | Always `"ack"`                                   |
| `ack_for` | string | `"prepare"`, `"start"`, or `"stop"`              |
| `success` | bool   | `true` if the operation succeeded                |
| `error`   | string | Error message; only present when `success=false` |

#### ACK timing

| ACK       | When sent                                                                                                                                 |
|-----------|-------------------------------------------------------------------------------------------------------------------------------------------|
| `prepare` | After all modules have prepared and the engine is **ready to start its timer**. Sent with `success=false` if the engine fails to prepare. |
| `start`   | Immediately on receipt - the controller/listener are already in running by this point.                                                    |
| `stop`    | After the listener's engine has fully stopped and data has been flushed to disk.                                                          |

The controller blocks on `prepare` ACKs (up to the configured timeout) before
broadcasting `start`. It never blocks on `start` ACKs, but with `expected_acks > 0`
it passively checks that enough arrived within the timeout.

### State machine

```
Controller                                   Listener(s)
──────────                                   ──────────
broadcastPrepare()                    ──▶    receive PREPARE
  wait for N prepare-ACKs                    [engine prepares all modules]
                                      ◀──    send prepare-ACK (success/fail)
broadcastStart()                      ──▶    receive START → align timer to ts_start_us
  (run starts locally)                ◀──    send start-ACK immediately
  … run in progress …                        … run in progress …
broadcastStop()                       ──▶    receive STOP
  (run stops locally)                        [engine stops, data flushed]
                                      ◀──    send stop-ACK
```

If a listener times out waiting for the START command (30 s default), it aborts
its local run, sends a negative `prepare` ACK, and resets to idle.  The controller
automatically broadcasts `stop` for any failure that occurs after `broadcastPrepare`
succeeded, so stranded listeners can clean up without waiting for their timeout.

## Options

### controller.py

| Option            | Default           | Description                               |
|-------------------|-------------------|-------------------------------------------|
| `--cmd-port`      | 5556              | PUB socket port                           |
| `--fb-port`       | 5557              | PULL socket port                          |
| `--ident`         | `demo-controller` | Sender instance ID                        |
| `--auto`          | off               | Run automatic prepare→start→stop sequence |
| `--expected-acks` | 0                 | Wait for N ACKs before proceeding         |
| `--ack-timeout`   | 1500              | ACK wait timeout (ms)                     |
| `--project`       | `demo-project`    | Project name in prepare message           |
| `--subject`       | `""`              | Subject ID                                |
| `--group`         | `""`              | Subject group                             |
| `--experiment`    | `demo-experiment` | Experiment ID                             |

### listener.py

| Option       | Default         | Description                |
|--------------|-----------------|----------------------------|
| `--host`     | `localhost`     | Controller hostname or IP  |
| `--cmd-port` | 5556            | SUB socket port            |
| `--fb-port`  | 5557            | PUSH socket port           |
| `--ident`    | `demo-listener` | Sender instance ID in ACKs |

## Timing note (slow-joiner problem)

ZeroMQ PUB/SUB sockets are asynchronous - a subscriber that connects after the
publisher has already sent a message will miss it. In normal operation this is
not an issue because listeners connect at startup (when the user enables listener
mode in Syntalos), long before the user clicks *Run*.  Both scripts add a 200 ms
settle pause after connecting to mitigate the race when running two commands
back-to-back in the same terminal session.

## Modes

Syntalos can be set to **both** listener and controller mode at once. In that case,
whatever happens first determines the final role of the Syntalos instance: If the user
presses the record button, the syntalos instance will assume the cole of controller.
If it receives a PREPARE request, it will assume the role of listener.

Syntalos can never have two roles at once during a run.

## Security note

The protocol assumes a trusted LAN.  There is no authentication or encryption.
Do not expose the ZMQ ports on untrusted networks.
