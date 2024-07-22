import time
import machine
from machine import Pin

ledPin = Pin('LED', Pin.OUT)
testPin = Pin(10, Pin.OUT)


async def blink_led():
    oport_f = sy.get_output_port('float-out')
    while True:
        # send some numbers to the host, with the device timestamp
        timestamp = time.ticks_ms()
        await oport_f.send_data([0.5, 1 if timestamp % 2 else 0], timestamp_us=timestamp*1000)

        # toggle the LEDs
        ledPin.high()
        testPin.low()
        await uasyncio.sleep(0.5)
        ledPin.low()
        testPin.high()
        await uasyncio.sleep(0.5)


def on_table_row_received(data):
    # just print any received table row to the console
    print('Received row:', data)


async def main():
    # Enable reading incoming data from the host
    sy.enable_input()

    # Blink a LED
    uasyncio.create_task(blink_led())

    # Receive tabular input
    sy.register_on_input('table-in', on_table_row_received)

    # Run this program indefinitely
    while True:
        await uasyncio.sleep(1)


# Run the main coroutine
uasyncio.run(main())
