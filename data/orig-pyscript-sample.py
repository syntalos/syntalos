import maio as io
import time
from threading import Timer

#
# Configure the pins we want to use
#
io.new_digital_pin(0, 'armLeft',  'input')
io.new_digital_pin(2, 'armRight', 'input')

io.new_digital_pin(6, 'dispLeft',  'output')
io.new_digital_pin(8, 'dispRight', 'output')

io.new_digital_pin(2, 'pinSignal', 'output')

lastArm = 'unknown'


def signal_led_blink():
    io.pin_set_value('pinSignal', True)
    time.sleep(.5) # wait 500 msec
    io.pin_set_value('pinSignal', False)


def digital_input_received(pinName, value):
    global lastArm
    if not value:
        return

    if pinName == lastArm:
        return
    lastArm = pinName

    io.save_event('success')

    if pinName == 'armLeft':
        io.pin_signal_pulse('dispLeft')
    elif (pinName == 'armRight'):
        io.pin_signal_pulse('dispRight')


def main():
    io.set_events_header(['State'])
    # light LED on port 2 briefly after 3 seconds
    timer = Timer(3, signal_led_blink)
    timer.start()

    while True:
        r, pinName, value = io.fetch_digital_input()
        if r:
            digital_input_received(pinName, value)


main()
