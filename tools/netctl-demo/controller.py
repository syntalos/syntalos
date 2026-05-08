#!/usr/bin/env python3
"""
Syntalos network-control demo: CONTROLLER

Binds a PUB socket (command port) and a PULL socket (feedback/ACK port),
broadcasts prepare / start / stop commands and collects acknowledgments.

Usage:
    python controller.py [--cmd-port 5556] [--fb-port 5557] [--ident MYHOST]
                         [--auto] [--expected-acks N] [--ack-timeout 1500]

When --auto is given the script runs through prepare → start → stop automatically.
Without it an interactive prompt lets you issue commands manually.
"""

import argparse
import json
import sys
import zmq
import threading
import time
import uuid
from datetime import datetime, timezone


def walltime_unix_us() -> int:
    return int(datetime.now(timezone.utc).timestamp() * 1_000_000)


def new_run_id() -> str:
    f = getattr(uuid, 'uuid7', None)
    if f is not None:
        return str(f())
    return str(uuid.uuid4())


def make_msg(msg_type: str, run_id: str, sender: str, **extra) -> bytes:
    obj = {
        "v": 1,
        "type": msg_type,
        "sender": sender,
        "run_id": run_id,
        **extra,
    }
    return json.dumps(obj).encode()


class AckCollector(threading.Thread):
    """
    ACK collector (runs in background thread)
    """

    def __init__(self, pull_sock):
        super().__init__(daemon=True)
        self._sock = pull_sock
        self._counts: dict[str, int] = {}
        self._lock = threading.Lock()
        self._running = True

    def run(self):
        while self._running:
            try:
                data = self._sock.recv(flags=zmq.NOBLOCK)
                obj = json.loads(data)
                phase = obj.get("ack_for", "?")
                sender = obj.get("sender", "unknown")
                run_id = obj.get("run_id", "")
                success = obj.get("success", True)
                error = obj.get("error", "")
                key = f"{phase}:{run_id}"
                with self._lock:
                    self._counts[key] = self._counts.get(key, 0) + 1
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                status = "OK" if success else f"FAILED: {error}"
                print(f"\n<  [{ts}] ACK from '{sender}' for '{phase}'  {status}  run={run_id[:8]}…")
            except Exception:
                time.sleep(0.05)

    def count(self, phase: str, run_id: str) -> int:
        with self._lock:
            return self._counts.get(f"{phase}:{run_id}", 0)

    def stop(self):
        self._running = False


def wait_for_acks(
    collector: AckCollector, phase: str, run_id: str, expected: int, timeout_ms: int
) -> bool:
    if expected <= 0:
        return True
    deadline = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < deadline:
        if collector.count(phase, run_id) >= expected:
            return True
        time.sleep(0.05)
    got = collector.count(phase, run_id)
    print(f"  [warn] ACK timeout for '{phase}': {got}/{expected} received")
    return got >= expected


def main():
    parser = argparse.ArgumentParser(description="Syntalos network-control demo controller")
    parser.add_argument("--cmd-port", type=int, default=5556, help="Command/broadcast port (PUB)")
    parser.add_argument("--fb-port", type=int, default=5557, help="Feedback/ACK port (PULL)")
    parser.add_argument("--ident", default="demo-controller", help="Sender instance ID")
    parser.add_argument("--auto", action="store_true", help="Auto-run prepare→start→stop sequence")
    parser.add_argument(
        "--expected-acks", type=int, default=0, help="Required ACK count (0=fire-and-forget)"
    )
    parser.add_argument("--ack-timeout", type=int, default=1500, help="ACK wait timeout in ms")
    parser.add_argument("--subject", default="", help="Subject ID for prepare message")
    parser.add_argument("--group", default="", help="Subject group for prepare message")
    parser.add_argument("--experiment", default="demo-experiment", help="Experiment ID")
    parser.add_argument("--project", default="demo-project", help="Project name")
    args = parser.parse_args()

    ctx = zmq.Context()

    pub = ctx.socket(zmq.PUB)
    pub.setsockopt(zmq.LINGER, 0)
    pub.bind(f"tcp://*:{args.cmd_port}")

    pull = ctx.socket(zmq.PULL)
    pull.setsockopt(zmq.LINGER, 0)
    pull.bind(f"tcp://*:{args.fb_port}")

    # brief pause to let subscriber connections settle
    time.sleep(0.3)

    collector = AckCollector(pull)
    collector.start()

    print(f"Controller ready  cmd={args.cmd_port}  fb={args.fb_port}  ident={args.ident!r}")

    run_id = new_run_id()

    def broadcast(topic: str, payload: bytes):
        pub.send_multipart([topic.encode(), payload])

    def do_prepare():
        nonlocal run_id
        run_id = new_run_id()
        msg = make_msg(
            "prepare",
            run_id,
            args.ident,
            project=args.project,
            subject_id=args.subject,
            subject_group=args.group,
            experiment_id=args.experiment,
        )
        broadcast("sy.cmd", msg)
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{ts}] PREPARE  run_id={run_id}")
        wait_for_acks(collector, "prepare", run_id, args.expected_acks, args.ack_timeout)

    def do_start():
        msg = make_msg("start", run_id, args.ident, ts_start_us=walltime_unix_us())
        broadcast("sy.cmd", msg)
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{ts}] START    run_id={run_id[:8]}…")
        if args.expected_acks > 0:
            ok = wait_for_acks(collector, "start", run_id, args.expected_acks, args.ack_timeout)
            if not ok:
                print(
                    "  [warn] insufficient start ACKs — listeners may have missed the start signal"
                )

    def do_stop(success: bool = True):
        msg = make_msg("stop", run_id, args.ident, success=success)
        broadcast("sy.cmd", msg)
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{ts}] STOP     success={success}  run_id={run_id[:8]}…")

    if args.auto:
        print("Running auto sequence: prepare → (2s) → start → (5s) → stop")
        do_prepare()
        time.sleep(2.0)
        do_start()
        time.sleep(5.0)
        do_stop()
    else:
        print("Commands: p=prepare  s=start  x=stop  q=quit")
        while True:
            try:
                cmd = input("> ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break
            if cmd == "p":
                do_prepare()
            elif cmd == "s":
                do_start()
            elif cmd == "x":
                do_stop()
            elif cmd in ("q", "quit"):
                break
            else:
                print("Unknown command. Use p / s / x / q")

    collector.stop()
    pub.close()
    pull.close()
    ctx.term()
    print("Controller exited.")


if __name__ == "__main__":
    main()
