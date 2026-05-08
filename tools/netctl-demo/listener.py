#!/usr/bin/env python3
"""
Syntalos network-control demo: LISTENER

Connects a SUB socket to the controller's command port and a PUSH socket to
its feedback port. Prints every received command and immediately ACKs it.

Usage:
    python listener.py [--host localhost] [--cmd-port 5556] [--fb-port 5557]
                       [--ident MYHOST]
"""

import argparse
import json
import sys
import time
from datetime import datetime, timezone


def main():
    try:
        import zmq
    except ImportError:
        print("pyzmq is required: pip install pyzmq", file=sys.stderr)
        sys.exit(1)

    parser = argparse.ArgumentParser(description="Syntalos network-control demo listener")
    parser.add_argument("--host", default="localhost", help="Controller hostname or IP")
    parser.add_argument("--cmd-port", type=int, default=5556, help="Command/broadcast port (SUB)")
    parser.add_argument("--fb-port", type=int, default=5557, help="Feedback/ACK port (PUSH)")
    parser.add_argument("--ident", default="demo-listener", help="Sender instance ID")
    args = parser.parse_args()

    ctx = zmq.Context()

    sub = ctx.socket(zmq.SUB)
    sub.setsockopt(zmq.LINGER, 0)
    sub.setsockopt_string(zmq.SUBSCRIBE, "")
    sub.connect(f"tcp://{args.host}:{args.cmd_port}")

    push = ctx.socket(zmq.PUSH)
    push.setsockopt(zmq.LINGER, 0)
    push.connect(f"tcp://{args.host}:{args.fb_port}")

    # settle time to avoid missing the first broadcast (slow-joiner problem)
    time.sleep(0.3)

    print(
        f"Listener ready  host={args.host}  cmd={args.cmd_port}  fb={args.fb_port}  ident={args.ident!r}"
    )
    print("Waiting for commands (Ctrl-C to quit)…\n")

    def send_ack(msg_type: str, run_id: str, success: bool = True, error: str = ""):
        ack: dict = {
            "v": 1,
            "type": "ack",
            "sender": args.ident,
            "run_id": run_id,
            "ack_for": msg_type,
            "success": success,
        }
        if not success and error:
            ack["error"] = error
        push.send(json.dumps(ack).encode())

    try:
        while True:
            parts = sub.recv_multipart()
            if len(parts) < 2:
                continue
            topic = parts[0].decode(errors="replace")
            if topic != "sy.cmd":
                continue
            try:
                obj = json.loads(parts[1])
            except json.JSONDecodeError:
                print("  [warn] malformed JSON payload - ignored")
                continue

            if obj.get("v") != 1:
                print(f"  [warn] unknown protocol version {obj.get('v')} — ignored")
                continue

            msg_type = obj.get("type", "?")
            run_id = obj.get("run_id", "")
            sender = obj.get("sender", "?")
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]

            if msg_type == "prepare":
                project = obj.get("project", "")
                subject = obj.get("subject_id", "")
                group = obj.get("subject_group", "")
                experiment = obj.get("experiment_id", "")
                print(f"[{ts}] PREPARE  from={sender!r}  run={run_id[:8]}…")
                print(
                    f"          project={project!r}  subject={subject!r}({group})  experiment={experiment!r}"
                )

            elif msg_type == "start":
                master_us_val = obj.get("ts_start_us", 0)
                print(f"[{ts}] START    from={sender!r}  run={run_id[:8]}…")
                print(f"          controller ts_start_us={master_us_val}")

            elif msg_type == "stop":
                success = obj.get("success", True)
                print(f"[{ts}] STOP     from={sender!r}  run={run_id[:8]}…  success={success}")

            else:
                print(f"[{ts}] UNKNOWN  type={msg_type!r}  from={sender!r}")

            # "start" is ACKed immediately (the listener is already running).
            # "prepare" and "stop" are ACKed after the operation completes.
            # In this demo we simulate prepare/stop work with a short delay.
            if msg_type == "start":
                send_ack(msg_type, run_id, success=True)
                print(f"          → ACKed 'start' immediately")
            elif msg_type in ("prepare", "stop"):
                import time as _time

                _time.sleep(0.1)  # simulate work
                send_ack(msg_type, run_id, success=True)
                print(f"          → ACKed '{msg_type}' (success)")

    except KeyboardInterrupt:
        print("\nListener exited.")
    finally:
        sub.close()
        push.close()
        ctx.term()


if __name__ == "__main__":
    main()
