uEye Camera
##############
.. image:: ../../modules/camera-ueye/camera-ueye.svg
   :width: 72
   :align: right

The "uEye Camera" module can connect to many `IDS Imaging Development Systems <https://en.ids-imaging.com/>`_ camera that
can be addressed using their propriatery SDK.

.. warning::
    This module is currently lacking a maintainer and is not built by default, due to its reliance on
    the proprietary SDK and due to not having been tested for a long time.
    If you want to use it, some work to refresh its code may be required.

Usage
=====

You require a Syntalos version with this module enabled & built.
Once a Syntalos version with this module is available, it can be configured as usual.
The module can import a camera configuration file created using the uEye software, to tweak settings not available
via its user interface.


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
