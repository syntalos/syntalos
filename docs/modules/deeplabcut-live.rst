DeepLabCut Live
###############

This module can be used for live animal tracking using `DeepLabCut Live <https://github.com/DeepLabCut/DeepLabCut-live>`_
on a system with a capable GPU.

.. note::
    This module is a proof-of-concept and has not yet received wide testing.

Usage
=====

When first adding the module, it will automatically create a Python virtual environment and install the required
Python modules, including ``deeplabcut-live`` itself.

It then has very basic configuration options in its "Settings" panel, and required a pretrained DLC model for
live analysis.


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
     - Video frames to be analyzed.
   * - Tracking🠺
     - Out
     - ``TableRow``
     - Tracking information as table rows.


Stream Metadata
===============

.. list-table::
   :widths: 15 85
   :header-rows: 1

   * - Name
     - Metadata

   * - Tracking🠺
     - | ``table_header``: Header of the generated table.
