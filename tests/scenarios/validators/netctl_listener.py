#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Syntalos network-control test listener.

Connects to a Syntalos controller instance, receives and ACKs prepare/start/stop
commands, then exits cleanly.  Exits non-zero if no commands arrive within the
timeout so the test framework detects the failure immediately.
"""

import argparse
import json
import sys
import time

TIMEOUT_SEC = 30


def main():
    try:
        import zmq
    except ImportError:
        print("pyzmq is required: pip install pyzmq", file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser(description="Syntalos test network-control listener")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--cmd-port", type=int, required=True)
    parser.add_argument("--fb-port", type=int, required=True)
    parser.add_argument("--ident", default="test-listener")
    args = parser.parse_args()

    ctx = zmq.Context()

    sub = ctx.socket(zmq.SUB)
    sub.setsockopt(zmq.LINGER, 0)
    sub.setsockopt_string(zmq.SUBSCRIBE, "")
    sub.connect(f"tcp://{args.host}:{args.cmd_port}")

    push = ctx.socket(zmq.PUSH)
    push.setsockopt(zmq.LINGER, 0)
    push.connect(f"tcp://{args.host}:{args.fb_port}")

    # allow subscription to propagate before the controller's first broadcast
    time.sleep(0.3)

    def send_ack(msg_type, run_id, success=True):
        ack = {
            "v": 1,
            "type": "ack",
            "sender": args.ident,
            "run_id": run_id,
            "ack_for": msg_type,
            "success": success,
        }
        push.send(json.dumps(ack).encode())

    poller = zmq.Poller()
    poller.register(sub, zmq.POLLIN)

    deadline = time.monotonic() + TIMEOUT_SEC
    received = set()

    while True:
        remaining_ms = max(1, int((deadline - time.monotonic()) * 1000))

        events = poller.poll(remaining_ms)
        if not events:
            if "stop" in received:
                break
            # deadline expired - exit with failure whether or not we received prepare
            print(f"Timeout waiting for commands (received: {received})", file=sys.stderr)
            sub.close()
            push.close()
            ctx.term()
            sys.exit(1)

        parts = sub.recv_multipart()
        if len(parts) < 2:
            continue
        if parts[0].decode(errors="replace") != "sy.cmd":
            continue

        try:
            obj = json.loads(parts[1])
        except json.JSONDecodeError:
            continue

        if obj.get("v") != 1:
            continue

        msg_type = obj.get("type", "")
        run_id = obj.get("run_id", "")

        if msg_type in ("prepare", "start", "stop"):
            received.add(msg_type)
            print(f"Received: {msg_type}", flush=True)
            send_ack(msg_type, run_id, success=True)

        if msg_type == "stop":
            break

        # extend deadline as long as we keep receiving messages
        deadline = time.monotonic() + TIMEOUT_SEC

    sub.close()
    push.close()
    ctx.term()
    sys.exit(0)


if __name__ == "__main__":
    main()
