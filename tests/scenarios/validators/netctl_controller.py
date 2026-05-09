#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Syntalos network-control test controller.

Binds PUB + PULL sockets, waits for the Syntalos listener to connect,
then drives it through the full prepare → start → (wait) → stop sequence.
Exits 0 on success, 1 on any failure so the test framework can detect
protocol errors immediately via the exit code.
"""

import argparse
import json
import sys
import time
import uuid

SETTLE_SEC = 3.0  # time to wait for the listener to connect after binding
ACK_TIMEOUT_SEC = 15.0


def main():
    try:
        import zmq
    except ImportError:
        print("pyzmq is required: pip install pyzmq", file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser(description="Syntalos test network-control controller")
    parser.add_argument("--cmd-port", type=int, required=True)
    parser.add_argument("--fb-port", type=int, required=True)
    parser.add_argument("--ident", default="test-controller")
    parser.add_argument(
        "--run-duration",
        type=float,
        default=6.0,
        help="Seconds to let the run proceed before sending stop",
    )
    args = parser.parse_args()

    ctx = zmq.Context()

    pub = ctx.socket(zmq.PUB)
    pub.setsockopt(zmq.LINGER, 0)
    pub.bind(f"tcp://*:{args.cmd_port}")

    pull = ctx.socket(zmq.PULL)
    pull.setsockopt(zmq.LINGER, 0)
    pull.bind(f"tcp://*:{args.fb_port}")

    print(f"Controller bound: cmd={args.cmd_port}, fb={args.fb_port}", flush=True)
    print(f"Waiting {SETTLE_SEC}s for listener to connect...", flush=True)
    time.sleep(SETTLE_SEC)

    run_id = str(uuid.uuid4())

    def send_cmd(msg_type, extra=None):
        msg = {"v": 1, "type": msg_type, "sender": args.ident, "run_id": run_id}
        if extra:
            msg.update(extra)
        pub.send_multipart([b"sy.cmd", json.dumps(msg).encode()])
        print(f"Sent: {msg_type}", flush=True)

    def wait_ack(for_type, timeout=ACK_TIMEOUT_SEC):
        poller = zmq.Poller()
        poller.register(pull, zmq.POLLIN)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining_ms = max(1, int((deadline - time.monotonic()) * 1000))
            if not poller.poll(remaining_ms):
                break
            raw = pull.recv()
            try:
                obj = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if obj.get("ack_for") == for_type and obj.get("run_id") == run_id:
                if not obj.get("success", True):
                    print(
                        f"ACK for '{for_type}' reported failure: {obj.get('error', '?')}",
                        file=sys.stderr,
                    )
                    return False
                print(f"Received ACK: {for_type}", flush=True)
                return True
        print(f"Timeout waiting for '{for_type}' ACK", file=sys.stderr)
        return False

    def cleanup(code):
        pub.close()
        pull.close()
        ctx.term()
        sys.exit(code)

    # --- PREPARE ---
    send_cmd(
        "prepare", {"subject_id": "", "subject_group": "", "experiment_id": "", "project": "test"}
    )
    if not wait_ack("prepare"):
        cleanup(1)

    # --- START ---
    ts_us = int(time.time() * 1_000_000)
    send_cmd("start", {"ts_start_us": ts_us})

    # --- RUN ---
    time.sleep(args.run_duration)

    # --- STOP ---
    send_cmd("stop", {"success": True})

    cleanup(0)


if __name__ == "__main__":
    main()
