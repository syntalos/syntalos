import maio as io
from maio import PinType

import sys
import time
from threading import Timer

#
# Configure the pins we want to use
#
fmod = io.get_firmata_interface('Firmata I/O')
fmod.new_digital_pin(0, 'armLeft',  PinType.INPUT)
fmod.new_digital_pin(2, 'armRight', PinType.INPUT)

fmod.new_digital_pin(6, 'dispLeft',  PinType.OUTPUT)
fmod.new_digital_pin(8, 'dispRight', PinType.INPUT)

fmod.new_digital_pin(2, 'pinSignal', PinType.OUTPUT)

mtable = io.EventTable('maze')

g_last_arm = 'unknown'


def signal_led_blink():
    fmod.pin_set_value('pinSignal', True)
    time.sleep(.5)  # wait 500 msec
    fmod.pin_set_value('pinSignal', False)


def digital_input_received(pin_name, value):
    global g_last_arm
    if not value:
        return

    if pin_name == g_last_arm:
        return
    g_last_arm = pin_name

    mtable.add_event(['success'])

    if pin_name == 'armLeft':
        io.pin_signal_pulse('dispLeft')
    elif (pin_name == 'armRight'):
        io.pin_signal_pulse('dispRight')


def main():
    mtable.set_header(['State'])

    # light LED on port 2 briefly after 3 seconds
    timer = Timer(3, signal_led_blink)
    timer.start()

    while True:
        r, pin_name, value = fmod.fetch_digital_input()
        if r:
            digital_input_received(pin_name, value)


main()
