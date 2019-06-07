import maio as io
from maio import PinType

import time
from threading import Timer

#
# Configure the pins we want to use
#
fm = io.get_firmata_interface('Firmata I/O')

fm.new_digital_pin(0, 'armLeft',   PinType.INPUT)
fm.new_digital_pin(2, 'armRight',  PinType.INPUT)

fm.new_digital_pin(6, 'foodLeft',  PinType.OUTPUT)
fm.new_digital_pin(8, 'foodRight', PinType.INPUT)

fm.new_digital_pin(2, 'pinSignal', PinType.OUTPUT)


# global variables
g_last_arm = 'unknown'


def signal_led_blink():
    fm.pin_set_value('pinSignal', True)
    time.sleep(.5)  # wait 500 msec
    fm.pin_set_value('pinSignal', False)


def digital_input_received(mtable, pin_name, value):
    global g_last_arm
    if not value:
        return

    if pin_name == g_last_arm:
        return
    g_last_arm = pin_name

    mtable.add_event(['success'])

    if pin_name == 'armLeft':
        fm.pin_signal_pulse('foodLeft')
    elif (pin_name == 'armRight'):
        fm.pin_signal_pulse('foodRight')


def main():
    mtable = io.EventTable('maze')
    mtable.set_header(['State'])

    # light LED on port 2 briefly after 3 seconds
    timer = Timer(3, signal_led_blink)
    timer.start()

    while True:
        r, pin_name, value = fm.fetch_digital_input()
        if r:
            digital_input_received(mtable, pin_name, value)


main()
