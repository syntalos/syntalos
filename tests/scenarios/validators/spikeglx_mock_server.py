#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""
Minimal mock of the SpikeGLX remote command server.

Implements the subset of the line protocol that the Syntalos SpikeGLX
module (through SpikeGLX-CPP-SDK) uses: text commands terminated by
newline, replies of 0..n lines followed by "OK" or "ERROR <message>",
the READY handshake of SETMETADATA and the binary FETCH response.

Simulated layout: one IMEC probe stream ("imec0", 384 AP + 384 LF + 1 SY
channels at 30 kHz) and one OneBox stream ("obx0", 12 XA + 1 DW + 1 SY at
30 kHz). Sample values are deterministic sine waves, the SY/DW channels
carry a 1 Hz square wave.

The process prints "Received: <CMD> ..." lines for run-control commands so
a test harness can verify the expected command sequence, and exits with
status 0 once a run was stopped and all clients disconnected (or after
--lifetime seconds).
"""

import argparse
import math
import socketserver
import struct
import sys
import threading
import time

try:
    import numpy as np
except ImportError:  # pragma: no cover
    np = None


class MockState:
    def __init__(self, ring_seconds):
        self.lock = threading.Lock()
        self.initialized = True
        self.running = False
        self.saving = False
        self.run_name = "myRun"
        self.data_dir = "C:/SGL_DATA"
        self.g = -1
        self.t = -1
        self.run_start = None
        self.ring_seconds = ring_seconds
        self.metadata = {}
        self.files = []
        self.saw_stoprun = False
        self.clients = 0
        # js -> list of streams (ip index)
        self.streams = {
            2: [
                {
                    "name": "imec0",
                    "js": 2,
                    "rate": 30000.0,
                    "counts": [384, 384, 1],
                    "sn": ("9223372036854775807", -1),
                    "i16tov": [2.34375e-06] * 384 + [4.6875e-06] * 384 + [1.0],
                    "maxint": 512,
                }
            ],
            1: [
                {
                    "name": "obx0",
                    "js": 1,
                    "rate": 30000.0,
                    "counts": [12, 1, 1],
                    "sn": ("21000042", 3),
                    "i16tov": [3.0517578125e-04] * 12 + [1.0, 1.0],
                    "maxint": 32768,
                }
            ],
            0: [],
        }

    def stream(self, js, ip):
        if js == -2:
            js = 2
        lst = self.streams.get(js)
        if lst is None or ip < 0 or ip >= len(lst):
            return None
        return lst[ip]

    def sample_count(self, stream):
        if not self.running or self.run_start is None:
            return 0
        return int((time.monotonic() - self.run_start) * stream["rate"])


def digital_words(stream):
    """Absolute channel indices of the stream's packed digital words, by group name."""
    names = {2: ["AP", "LF", "SY"], 1: ["XA", "DW", "SY"], 0: ["MN", "MA", "XA", "DW"]}[
        stream["js"]
    ]
    words = {}
    offset = 0
    for name, count in zip(names, stream["counts"]):
        if name in ("SY", "DW"):
            for w in range(count):
                words[offset + w] = name
        offset += count
    return words


def gen_samples(stream, chans, start, n):
    """Generate n samples for the given absolute channel indices, sample-major int16.

    Digital words are packed the way SpikeGLX packs them: the lowest numbered line
    sits in the lowest order bit. The SY word carries the 1 Hz sync square wave in
    bit #6 (and a permanently set bit #0, the acquisition-start flag); the DW word
    drives all of its 12 lines (bits 0-11) with the same square wave.
    """
    rate = stream["rate"]
    dwords = digital_words(stream)

    def digital(idx):
        """Value of a digital word for sample index array/scalar `idx`."""
        return (idx // (rate / 2.0)) % 2

    if np is not None:
        idx = np.arange(start, start + n, dtype=np.float64)
        out = np.empty((n, len(chans)), dtype=np.int16)
        for j, c in enumerate(chans):
            group = dwords.get(c)
            if group == "SY":
                out[:, j] = (1 | (digital(idx).astype(np.int64) << 6)).astype(np.int16)
            elif group == "DW":
                out[:, j] = (digital(idx).astype(np.int64) * 0x0FFF).astype(np.int16)
            else:
                freq = 1.0 + (c % 10)
                out[:, j] = (2000.0 * np.sin(2.0 * math.pi * freq * idx / rate)).astype(np.int16)
        return out.tobytes()
    # pure Python fallback (slow, but fine for tiny fetches)
    vals = []
    for i in range(start, start + n):
        for c in chans:
            group = dwords.get(c)
            if group == "SY":
                vals.append(1 | (int(digital(i)) << 6))
            elif group == "DW":
                vals.append(int(digital(i)) * 0x0FFF)
            else:
                freq = 1.0 + (c % 10)
                vals.append(int(2000.0 * math.sin(2.0 * math.pi * freq * i / rate)))
    return struct.pack("<%dh" % len(vals), *vals)


ILLEGAL_RUNNAME_CHARS = set('/\\[]<>*":;,?|=')


class Handler(socketserver.StreamRequestHandler):
    def setup(self):
        super().setup()
        with self.server.state.lock:
            self.server.state.clients += 1

    def finish(self):
        with self.server.state.lock:
            self.server.state.clients -= 1
        super().finish()

    def send(self, text):
        self.wfile.write(text.encode())
        self.wfile.flush()

    def ok(self, *lines):
        out = "".join(l + "\n" for l in lines) + "OK\n"
        self.send(out)

    def error(self, msg):
        self.send("ERROR " + msg + "\n")

    def handle(self):
        st = self.server.state
        while True:
            raw = self.rfile.readline()
            if not raw:
                break
            line = raw.decode(errors="replace").strip()
            if not line:
                continue
            try:
                self.dispatch(st, line)
            except BrokenPipeError:
                break
            except Exception as e:  # noqa: BLE001
                self.error("%s: internal mock error: %s" % (line.split()[0], e))

    def stream_or_error(self, st, cmd, toks):
        if len(toks) < 3:
            self.error("%s: Requires parameters js ip." % cmd)
            return None
        s = st.stream(int(toks[1]), int(toks[2]))
        if s is None:
            self.error("%s: Stream (%s,%s) does not exist." % (cmd, toks[1], toks[2]))
        return s

    def dispatch(self, st, line):
        toks = line.split()
        cmd = toks[0].upper()

        if cmd == "NOOP":
            self.ok()
        elif cmd == "GETVERSION":
            self.ok("SpikeGLX mock v20260101 api v4.1.3")
        elif cmd == "ISINITIALIZED":
            self.ok("1" if st.initialized else "0")
        elif cmd == "ISRUNNING":
            self.ok("1" if st.running else "0")
        elif cmd == "ISSAVING":
            self.ok("1" if st.saving else "0")
        elif cmd == "GETTIME":
            self.ok("%.3f" % time.monotonic())
        elif cmd == "GETRUNNAME":
            self.ok(st.run_name)
        elif cmd == "GETDATADIR":
            self.ok(st.data_dir)
        elif cmd == "GETLASTGT":
            self.ok("%d %d" % (st.g, st.t))
        elif cmd == "GETPROBELIST":
            self.ok("(0,1,PRB_1_4_0480_1_C)")
        elif cmd == "ENUMDATADIR":
            self.ok(*st.files)
        elif cmd == "GETPARAMS":
            self.ok(
                "gateMode=0",
                "trigMode=0",
                "manOvShowBut=true",
                "manOvInitOff=true",
                "snsRunName=" + st.run_name,
                "imEnabled=true",
                "niEnabled=false",
            )
        elif cmd == "GETSTREAMNP":
            js = int(toks[1])
            if js == -2:
                js = 2
            self.ok(str(len(st.streams.get(js, []))))
        elif cmd == "GETSTREAMSAMPLERATE":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                self.ok("%.6f" % s["rate"])
        elif cmd == "GETSTREAMACQCHANS":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                self.ok(" ".join(str(c) for c in s["counts"]))
        elif cmd == "GETSTREAMSAVECHANS":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                self.ok(" ".join(str(i) for i in range(sum(s["counts"]))))
        elif cmd == "GETSTREAMSN":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                self.ok("%s %d" % s["sn"])
        elif cmd == "GETSTREAMMAXINT":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                self.ok(str(s["maxint"]))
        elif cmd == "GETSTREAMI16TOVOLTS":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                ch = int(toks[3]) if len(toks) > 3 else 0
                if ch < 0 or ch >= len(s["i16tov"]):
                    self.error("GETSTREAMI16TOVOLTS: Channel out of range.")
                else:
                    self.ok("%g" % s["i16tov"][ch])
        elif cmd == "GETSTREAMSAMPLECOUNT":
            s = self.stream_or_error(st, cmd, toks)
            if s:
                with st.lock:
                    self.ok(str(st.sample_count(s)))
        elif cmd == "SETRUNNAME":
            name = line[len(toks[0]) :].strip()
            if not name:
                self.error("SETRUNNAME: Requires name parameter.")
            elif any(ch in ILLEGAL_RUNNAME_CHARS for ch in name):
                self.error('SETRUNNAME: Run names may not contain any of {/\\[]<>*":;,?|=}')
            else:
                with st.lock:
                    st.run_name = name
                print("Received: SETRUNNAME %s" % name, flush=True)
                self.ok()
        elif cmd == "STARTRUN":
            with st.lock:
                if st.running:
                    self.error("STARTRUN: Already running.")
                    return
                st.running = True
                st.saving = False
                st.g = -1
                st.t = -1
                st.run_start = time.monotonic()
            print("Received: STARTRUN", flush=True)
            self.ok()
        elif cmd == "STOPRUN":
            with st.lock:
                st.running = False
                st.saving = False
                st.run_start = None
                st.saw_stoprun = True
            print("Received: STOPRUN", flush=True)
            self.ok()
        elif cmd == "SETRECORDENAB":
            if not st.running:
                self.error("SETRECORDENAB: Run not yet started.")
                return
            enable = bool(int(toks[1])) if len(toks) > 1 else False
            with st.lock:
                if enable and not st.saving:
                    st.g += 1
                    st.t = 0
                    prefix = "%s_g%d_t%d" % (st.run_name, st.g, st.t)
                    st.files += [prefix + ".imec0.ap.bin", prefix + ".imec0.ap.meta"]
                st.saving = enable
            print("Received: SETRECORDENAB %d" % (1 if enable else 0), flush=True)
            self.ok()
        elif cmd == "TRIGGERGT":
            if not st.running:
                self.error("TRIGGERGT: Run not yet started.")
                return
            print("Received: TRIGGERGT %s" % " ".join(toks[1:]), flush=True)
            self.ok()
        elif cmd == "SETMETADATA":
            if not st.running:
                self.error("SETMETADATA: Run not yet started.")
                return
            self.send("READY\n")
            kv = {}
            while True:
                raw = self.rfile.readline()
                if not raw:
                    return
                l = raw.decode(errors="replace").rstrip("\r\n")
                if not l:
                    break
                if "=" in l:
                    k, v = l.split("=", 1)
                    kv[k.strip()] = v.strip()
            if not kv:
                self.error("SETMETADATA: Sent metadata is empty.")
                return
            with st.lock:
                st.metadata.update(kv)
            for k, v in kv.items():
                print("Received: SETMETADATA %s=%s" % (k, v), flush=True)
            self.ok()
        elif cmd == "FETCH":
            self.fetch(st, toks)
        else:
            self.error("%s: Unknown command." % cmd)

    def fetch(self, st, toks):
        if len(toks) < 5:
            self.error("FETCH: Requires at least 4 params.")
            return
        js, ip = int(toks[1]), int(toks[2])
        s = st.stream(js, ip)
        if s is None or not st.running:
            self.error("FETCH: Not running or stream not enabled.")
            return
        start = int(toks[3])
        nmax = int(toks[4])
        nchan_total = sum(s["counts"])
        chan_tok = toks[5] if len(toks) > 5 else "-1#"
        if chan_tok in ("-1#", "-2#"):
            chans = list(range(nchan_total))
        else:
            chans = [int(c) for c in chan_tok.split("#") if c != ""]
            if any(c < 0 or c >= nchan_total for c in chans):
                self.error("FETCH: Channel subset out of range.")
                return

        with st.lock:
            end = st.sample_count(s)
        ring = int(st.ring_seconds * s["rate"])
        head = max(0, end - ring)
        if start < head:
            self.error("FETCH: Too late.")
            return
        if start >= end:
            # like SpikeGLX: briefly wait for a little data to arrive
            time.sleep(0.002)
            with st.lock:
                end = st.sample_count(s)
        n = max(0, min(nmax, end - start))
        payload = gen_samples(s, chans, start, n) if n > 0 else b""
        self.send("BINARY_DATA %d %d uint64(%d)\n" % (len(chans), n, start))
        if payload:
            self.wfile.write(payload)
            self.wfile.flush()
        self.ok()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    parser = argparse.ArgumentParser(description="SpikeGLX mock command server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=41420)
    parser.add_argument(
        "--lifetime", type=float, default=180.0, help="exit after this many seconds"
    )
    parser.add_argument(
        "--ring-seconds", type=float, default=30.0, help="simulated ring buffer length"
    )
    parser.add_argument("--stay", action="store_true", help="do not exit after the run was stopped")
    # accepted for compatibility with the test runner, unused
    parser.add_argument("--cmd-port", type=int, default=None)
    parser.add_argument("--fb-port", type=int, default=None)
    args = parser.parse_args()

    state = MockState(args.ring_seconds)
    server = Server((args.host, args.port), Handler)
    server.state = state
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    print("Mock SpikeGLX server listening on %s:%d" % (args.host, args.port), flush=True)

    deadline = time.monotonic() + args.lifetime
    try:
        while time.monotonic() < deadline:
            time.sleep(0.2)
            with state.lock:
                done = state.saw_stoprun and state.clients == 0 and not args.stay
            if done:
                # give a late reconnect a moment, then finish
                time.sleep(0.5)
                with state.lock:
                    if state.clients == 0:
                        break
    except KeyboardInterrupt:
        pass
    finally:
        server.shutdown()
        server.server_close()

    print("Mock SpikeGLX server exiting", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
