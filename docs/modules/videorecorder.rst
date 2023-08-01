Video Recorder
##############

Record a stream of frames as video.


Usage
=====

This module has a multitude of settings to configure the recording and select a good balance between quality and speed.

It also permits encoding to be deferred to after the experiment run, to save resources for data acquisition during a run
(this will require a lot of disk space temporarily, so ensure you have enough space available).


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
     - | ``STOP``: Stops recording and creates a new file.
       | ``START``: Resume recording, creating a new file if stopped, resuming the current file if paused.
       | ``PAUSE``: Pauses the current recording, does not create a new file.
       | ``STEP``: Encode one frame, then revert to the previous state (useful in combination with ``PAUSE``).
   * - 🠺Frames
     - In
     - ``Frame``
     - The frames to record.


Stream Metadata
===============

None generated (no output ports).
