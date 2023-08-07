Audio Source
############
.. image:: ../../modules/audiosource/audiosource.svg
   :width: 72
   :align: right

The "Audio Source" module can generate various test sounds, from sine-waves to clicks.

Usage
=====

Configure as usual. Configuration can not be changed after experiment has started.


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - 🠺Control
     - In
     - ``ControlCommand``
     - Supports ``START``, ``STOP``/``PAUSE`` commands to start/stop the audio output.


Stream Metadata
===============

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Name
     - Metadata

   * - 🠺Control
     - None
