Canvas
######
.. image:: ../../modules/canvas/canvas.svg
   :width: 72
   :align: right

The "Canvas" module displays any image content, most of the time any streaming video.


Usage
=====

No configuration required.


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
     - Accepts ``START`` and ``STOP``/``PAUSE`` command kinds to start/pause this module.
   * - 🠺Frames
     - In
     - ``Frame``
     - Frames to be displayed.


Stream Metadata
===============

None generated (no output ports).
