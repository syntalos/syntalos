# Communication glue code to allow MicroPython to interface
# with Syntalos ports to transmit tabular data between host and device

import sys
import json
import time
import uasyncio


def dump_json_compact(obj):
    """Dump a JSON object to a compact string"""
    return json.dumps(obj, separators=(',', ':'))


class SyntalosOutPort:
    def __init__(self, writer, port_idx: int):
        self._port_idx = port_idx
        self._out_writer = writer

    async def send_data(self, *args, **kwargs):
        """Send tabular data to the host"""
        if not args:
            return
        if len(args) == 1 and isinstance(args[0], (list, tuple)):
            args = args[0]

        timestamp = kwargs.get('timestamp_ms')
        if timestamp:
            self._out_writer.write(
                dump_json_compact({'p': self._port_idx, 'd': args, 't': timestamp})
            )
        else:
            self._out_writer.write(dump_json_compact({'p': self._port_idx, 'd': args}))

        self._out_writer.write('\n')
        await self._out_writer.drain()

    def send_data_sync(self, *args, **kwargs):
        """Synchronous function for sending data."""
        uasyncio.run(self.send_data(*args, **kwargs))


class SyntalosCommunicator:
    def __init__(self):
        self._out_writer = uasyncio.StreamWriter(sys.stdout)
        self._in_reader = uasyncio.StreamReader(sys.stdin)
        self._oport_count = 0
        self._iport_map = {}
        self._iport_pending = {}
        self._ref_time_ms = time.ticks_ms()
        self._elapsed_ms = 0

        print(dump_json_compact({'dc': 'start-time', 't_ms': self.ticks_ms()}))

    def _register_input_port_info(self, hdata):
        if hdata['hc'] == 'in-port':
            _, callback = self._iport_pending.pop(hdata['p'], (None, None))
            if callback is None:
                self._iport_pending[hdata['p']] = (hdata['i'], None)
            else:
                self._iport_map[hdata['i']] = callback

    async def _read_stdin(self):
        buf = bytearray()
        while True:
            b = await self._in_reader.read(1)
            if b == '\r' or b == '\n':
                s = buf.decode()
                if s.startswith('{'):
                    obj = json.loads(s)
                    if 'hc' in obj:
                        self._register_input_port_info(obj)
                    elif 'd' in obj:
                        idx = obj['p']
                        callback = self._iport_map.get(idx, None)
                        if callback:
                            callback(obj['d'])
                buf[:] = b''
            else:
                buf.extend(b)

    def enable_input(self):
        """Enable host input handling"""
        uasyncio.create_task(self._read_stdin())

    def ticks_ms(self):
        """A safer version of time.ticks_ms() that tries to mitigate the time.ticks_ms() overflow, if possible."""
        cticks = time.ticks_ms()
        tdiff = time.ticks_diff(cticks, self._ref_time_ms)
        self._ref_time_ms = cticks
        self._elapsed_ms += tdiff
        return self._elapsed_ms

    def get_output_port(self, port_id: str) -> SyntalosOutPort:
        """Register a port to be used for communication to the host."""

        port_idx = self._oport_count
        self._oport_count += 1
        print(dump_json_compact({'dc': 'new-out-port', 'i': port_idx, 'n': port_id}))

        return SyntalosOutPort(self._out_writer, port_idx)

    def register_on_input(self, port_id: str, callback):
        """Register a callback to run when data is received from the host."""

        idx, _ = self._iport_pending.pop(port_id, (None, None))
        if idx is None:
            self._iport_pending[port_id] = (-1, callback)
        else:
            self._iport_map[idx] = callback
