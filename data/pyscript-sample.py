import maio as io
import sys
from threading import Timer

def signal_led_blink():
    print(io.time_since_start_msec())
    sys.stdout.flush()


def main():
    print(io.time_since_start_msec())

    # light LED on port 2 briefly after 3 seconds
    timer = Timer(3, signal_led_blink)
    timer.start()

    while True:
        pass


main()
