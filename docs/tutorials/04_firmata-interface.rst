04. Controlling simple devices
##############################

`Arduino <https://www.arduino.cc/>`_ is an open-source microcontroller platform that,
when combined with the `Firmata <https://github.com/firmata/protocol>`_ firmware, can
be used to easily control a variety of devices using its analog and digital output ports.

Anything that can be controlled using TTL pulses will work, and there are also other devices
and more specialized sensors that you can integrate.
We published instructions on how to build some of these devices
at `our Maze Hardware site <https://github.com/bothlab/maze-hardware/blob/main/README.md>`_.

This tutorial will require some hardware tinkering and a bit of coding in Syntalos to be useful,
so it is for intermediate users.

1. Prepare your Arduino
=======================

Open your Arduino IDE. The navigate to *Sketch → Include Library → Manage Libraries*.
Search for *Firmata* and install the "Firmata by Firmata Developers" item.

.. image:: /graphics/arduino-firmata-install.avif
  :width: 340
  :alt: Installing Firmata for Arduino

Then, navigate to *File → Examples → Firmata*, and select the Firmata variant you want. *Standard Firmata* is the
option we select here.

The Firmata code for Arduino opens in a window, and you can upload it to your device the usual way.
The Arduino is now ready to be used, so let's test it!

2. Set up Syntalos
==================

Create a new Syntalos project and add a `Python Script`, `Firmata User Control`, `Firmata IO` and `Table` module.
Enter the settings of the Python module and edit its ports. Add a ``firmata-in`` input port with input data
type ``FirmataData`` and an output port of type ``FirmataControl`` named ``firmatactl-out``.
You can also add an output port of type ``TableRow`` named ``table-out`` for later use.
The we create some boilerplate code for the Python module, which does nothing, for now:

.. code-block:: python

    import syio as sy
    from syio import InputWaitResult

    fm_iport = sy.get_input_port('firmata-in')
    fm_oport = sy.get_input_port('firmatactl-out')
    tab_oport = sy.get_input_port('table-out')


    def prepare():
        pass


    def start():
        pass


    def loop() -> bool:
        # wait for new input to arrive
        wait_result = sy.await_new_input()
        if wait_result == InputWaitResult.CANCELLED:
            return False

        # return True, so the loop function is called again when new data is available
        return True


    def stop():
        pass

To play around with Firmata manually, without using the script yet, we connect input and output of `Firmata User Control`
to the respective ports of `Firmata IO`:

.. image:: /graphics/syntalos-firmata-manual-config.avif
  :width: 440
  :alt: Using an Arduino with Firmata manually in Syntalos

Open the settings of *Firmata IO* and select the serial port number of your plugged-in Arduino.

.. note::
    If the device does not show up for selection or you get a permission error upon launching Syntalos,
    you may need to add yourself to the ``dialout`` group to use serial devices.
    In order to do that, open a terminal and enter ``sudo adduser $USER dialout``, confirming with
    you administrator password. After a reboot / relogin, connecting to your Arduino should work now.

3. Manual work
==============

Before automating anything, we want to run some manual tests first and control our Arduino by hand.
For testing purposes, we wire up an LED to one of its free ports.
We can then already hit the *Ephemeral Run* button of Syntalos, to start a run without saving any data.

Double-click on the `Firmata User Control` module to bring up its display window. There, you can read
inputs and write to outputs. Click on the *Plus* sign to add a new *Menual Output Control* and add
a digital output pin.
On the *Received Input* side, select an analog, or digital input. Select the Arduino pins that you want to read
or write from, and change the values of your output.
The Arduino should react accordingly, and also display the read input values.

.. image:: /graphics/manual-firmata-control-dialog.avif
  :width: 480
  :alt: Manually reading Arduino pin values and writing to pins

This is pretty nice already, but we do want to automate this, so Syntalos can change values automatically,
for example based on test subject behavior, and also write the data it reads to a file for later analysis.

4. Automation: Blinking light
=============================

To automate things, we need to go back to the Python script again.
First, we need to break the port connections between `Firmata User Control` and `Firmata IO` (select them
with a click, and then push the *Disconnect* button), and instead connect the ports to the respective
`Python Script` ports:

.. image:: /graphics/syntalos-firmata-pyscript-config.avif
  :width: 440
  :alt: Using an Arduino with Firmata controlled by a Python script in Syntalos

For demonstration purposes, we will let an LED blink at a given interval first, and log the time
when we sent the command to get the LED to blink.

This is the code we need to achieve that:

.. code-block:: python
    :linenos:
    :emphasize-lines: 11,17,23,31

    import syio as sy
    from syio import InputWaitResult, ControlCommand, ControlCommandKind


    # constants
    LED_DURATION_MSEC = 250
    LED_INTERVAL_MSEC = 2000


    fm_iport = sy.get_input_port('firmata-in')
    fm_oport = sy.get_input_port('firmatactl-out')
    tab_oport = sy.get_input_port('table-out')


    def prepare():
        # set table header and save filename
        tab_oport.set_metadata_value('table_header', ['Time', 'Event'])
        tab_oport.set_metadata_value('data_name_proposal', 'events/led_status')


    def start():
        # set pin 8 as LED output pin
        fm_oport.firmata_register_digital_pin(8, 'led1', True)


    def loop() -> bool:
        # loop forever, as we do not need to read any input data
        while True:
            tab_oport.submit([sy.time_since_start_msec(),
                            'led-pulse'])
            fm_oport.firmata_submit_digital_pulse('led1', LED_DURATION_MSEC)

            sy.wait(LED_INTERVAL_MSEC)
            if not sy.check_running():
                break

        # ensure LED is off
        fm_oport.firmata_submit_digital_value('led1', False)

        return False

Initially, in line 10, we need to fetch references to our input/output ports (using only the latter for now), so we
can use them in later parts of the script. The ``prepare()`` function is called before the experiment run is actually started.
In it we can set metadata on our respective ports. In our case we set a table header using the ``table_header`` property on the
table row output port, and also suggest a name to save the resulting CSV table under using the ``data_name_proposal`` property.

Then, once the experiment is started, we can actually send messages from our module to other modules. In the ``start()`` routine,
we first register pin ``8`` on the Arduino as digital output pin (adjust this if your LED is on a different pin).

.. note::
    This example uses convenience methods to handle digital pins. For example, the call to
    ``firmata_register_digital_pin()`` on the Firmata control port could also be written as:

    .. code-block:: python

        ctl = sy.new_firmatactl_with_id_name(sy.FirmataCommandKind.NEW_DIG_PIN, 8, 'led1')
        ctl.is_output = True
        fm_oport.submit(ctl)

    Not every action has convenience methods, but the most common operations do.

Then, in the ``loop()`` function the actual logic happens to make the LED blink. Normally, this function is called
by Syntalos constantly when new data arrives. But since we do not need to wait for incoming data, we first just enter
an endless ``while`` loop.
In it, we send a new table row to the `Table` module for storage & display, using the ``sy.time_since_start_msec()`` function
to get the current time since the experiment run was started and naming the event ``led-pulse``. You should see these two values
show up in the table later. Then, we actually send a message to the `Firmata IO` module to instruct it to set the LED pin ``HIGH``
for the time ``LED_DURATION_MSEC``. Then we wait using ``sy.wait(LED_INTERVAL_MSEC)`` until we repeat the process again, and exit
the loop when the experiment is stopped.

.. warning::
    Keep in mid that when submitting data on a port, you are **not** calling the respective task immediately - you are
    merely enqueueing an instructions for the other module to act upon at a later time.
    Realistically, Syntalos will execute the queued action instantly with little delay, but Syntalos can not make any
    real-time guarantees. If you need those, consider using dedicated hardware or an FPGA, and control those components
    with Syntalos instead.

If you hit the *Run* button, the experiment should run and the LED should blink for 250 msec every 2 sec.

4. Automation: Reading Data
===========================

Now, let's read some data and let an LED blink for each piece of data that was received!
We assume you have a switch placed on one Ardino pin, and an LED on another for testing purposes.

The code we need for this looks very similar to our previous one:

.. code-block:: python
    :linenos:
    :emphasize-lines: 22,30,36,47

    import syio as sy
    from syio import InputWaitResult, ControlCommand, ControlCommandKind


    # constants
    LED_DURATION_MSEC = 500


    fm_iport = sy.get_input_port('firmata-in')
    fm_oport = sy.get_input_port('firmatactl-out')
    tab_oport = sy.get_input_port('table-out')


    def prepare():
        # set table header and save filename
        tab_oport.set_metadata_value('table_header', ['Time', 'Event'])
        tab_oport.set_metadata_value('data_name_proposal', 'events/led_status')


    def start():
        # set pin 7 as input pin
        fm_oport.firmata_register_digital_pin(7, 'switch', False)

        # set pin 8 as LED output pin
        fm_oport.firmata_register_digital_pin(8, 'led1', True)


    def loop() -> bool:
        # wait for new input to arrive
        if sy.await_new_input() == InputWaitResult.CANCELLED:
            # the run has been cancelled (by the user or an error)
            return False

        while True:
            data = fm_iport.next()
            if data is None:
                # no more data, exit
                break

            # we are only interested in digital input
            if not data.is_digital:
                continue
            # we only want to look at the 'switch' pin
            if data.pin_name != 'switch':
                continue

            if data.value:
                tab_oport.submit([sy.time_since_start_msec(),
                                  'switch-on'])
                fm_oport.firmata_submit_digital_pulse('led1', LED_DURATION_MSEC)
            else:
                tab_oport.submit([sy.time_since_start_msec(),
                                  'switch-off'])

        # return True, so this function is called again
        return True

In ``start()`` we additionally register pin ``7`` as an input pin this time, while all the other changes are in the ``loop()``
function. There, we initially just wait for new input to arrive. The ``sy.await_new_input()`` call will return if there was
new data to process on *any* of the Python script modules' input ports. In this case we have only one input port, but of we
had more than one we would now need to check all input ports for new data. Since there might also be more than one data block,
we enter a ``while`` loop and pull new data from the input port using ``fm_iport.next()`` until no more data is available.

Next, we check if we have data from the right, registered block by checking if the pin is digital and if it is our ``switch`` labelled
pin. We ignore any other data (there should not be any, but just in case...).
Then, if we receive a ``True`` value, we command the LED to blink for half a second and log that fact in our table, otherwise
we just log the fact that the switch is off.

Finally, we let the ``loop()`` function return ``True``, so it is called again soon.

Upon running this project, you should see the LED flash briefly once you push the button, and see the state of the button logged
in the table displayed by the `Table` module.

5. Expansion
============

With this, you have basic control over a lot of equipment to control behavior experiments, from TTL-controlled lasers,
to gates and lick sensors.
Try making this work with your hardware, try `some DIY Maze Hardware <https://github.com/bothlab/maze-hardware/blob/main/README.md>`_
or hardware from other open source projects to make behavior experiments work.
