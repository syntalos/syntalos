Video Transformer
#################

Transform a stream of frames.


Usage
=====

The video transformer allows scaling & cropping of incoming frames right now.
Multiple scale and/or crop operations can be stacked in sequence.

For the module to learn about frame dimensions, you may need to launch the experiment
once in ephemeral mode.


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - 🠺Frames
     - In
     - ``Frame``
     - Input frames to transform.
   * - Edited Frames🠺
     - Out
     - ``Frame``
     - Generated processed frames.


Stream Metadata
===============

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Name
     - Metadata

   * - Edited Frames🠺
     - | ``framerate``: Double, frame rate in FPS.
