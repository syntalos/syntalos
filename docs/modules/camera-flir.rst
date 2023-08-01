FLIR Camera
##############

The "FLIR Camera" module can connect to any `FLIR Systems <https://www.flir.com/>`_ camera that
can be addressed using their propriatery `Spinnaker SDK <https://www.flir.com/products/spinnaker-sdk/>`_.

.. warning::
    This module is currently lacking a maintainer and is not built by default, due to its reliance on
    the proprietary Spinnaker SDK.

Usage
=====

You require a Syntalos version with this module enabled & built. This requires the Spinnaker SDK to be installed.

Once a Syntalos version with this module is available, it can be configured as usual.


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
