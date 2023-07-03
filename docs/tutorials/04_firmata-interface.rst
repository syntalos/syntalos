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

4. Automation
=============

To automate things, we need to go back to the Python script again.

TODO

.. image:: /graphics/syntalos-firmata-pyscript-config.avif
  :width: 440
  :alt: Using an Arduino with Firmata controlled by a Python script in Syntalos
