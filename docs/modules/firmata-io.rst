Firmata I/O
###########
.. image:: ../../modules/firmata-io/firmata-io.svg
   :width: 72
   :align: right

This module can connect with any device speaking the `Firmata Protocol <https://github.com/firmata/protocol>`_ via
a serial port.


Usage
=====

Configure as usual by setting a serial device to connect to.

.. note::
    If the device does not show up for selection or you get a permission error,
    you may need to add yourself to the ``dialout`` group to use serial devices.
    In order to do that, open a terminal and enter ``sudo adduser $USER dialout``, confirming with
    you administrator password. The log in again.

This module needs another module to be useful.


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - 🠺Firmata Control
     - In
     - ``FirmataControl``
     - Control commands for Firmata
   * - Firmata Data🠺
     - Out
     - ``FirmataData``
     - Data read from the Firmata device.


Stream Metadata
===============

Default stream metadata.
