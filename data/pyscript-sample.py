import maio as io
import sys
from threading import Timer

def signal_led_blink():
    print(io.time_since_start_msec())
    sys.stdout.flush()


def main():
    print(io.time_since_start_msec())

    x = io.EventTable('test')
    x.set_header(['A', 'B'])

    x.add_event(['dsvdf', 'dsvs'])

    # light LED on port 2 briefly after 3 seconds
    timer = Timer(3, signal_led_blink)
    timer.start()

    fmod = io.get_firmata_interface('Firmata I/O')
    print(fmod.test())

    while True:
        pass


main()
