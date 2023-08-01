TIS Camera
##########

The "TIS Camera" module supports industrial cameras from `The Imaging Source <https://www.theimagingsource.com/>`_.

Usage
=====

You need to install the `Linux driver/support package <https://www.theimagingsource.com/support/download/>`_
first in order to use the Syntalos camera module.
More advanced users can also build & install the Imaging Source SDK from its `source code (GitHub) <https://github.com/TheImagingSource/tiscamera>`_.

Afterwards, you can select a camera via its V4L2 or Aravis drivers and configure it in Syntalos.
Syntalos should support the full range of configuration options for each camera (with exception of partial sensor readout on some devices).

To vide the generated video, use a *Canvas* module.


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - Video🠺
     - Out
     - ``Frame``
     - ~


Stream Metadata
===============

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Name
     - Metadata

   * - Video🠺
     - | ``size``: 2D Size, Dimension of generated frames
       | ``framerate``: Double, Target framerate per second.
       | ``has_color``: Boolean, Whether the output frames have color or are grayscale.
