Trace Plot
##########
.. image:: ../../modules/traceplot/traceplot.svg
   :width: 72
   :align: right

Very simple module to plot traces, often used for electrophysiology traces.


Usage
=====

Select the port and channel to display data from, and adjust their scaling.
Data is displayed as time series.

This module was originally created for testing purposes, but may be useful for other things too.


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - 🠺Float In 1
     - In
     - ``FloatSignalBlock``
     - Float signal input channel 1
   * - 🠺Float In 2
     - In
     - ``FloatSignalBlock``
     - Float signal input channel 2
   * - 🠺Float In 3
     - In
     - ``FloatSignalBlock``
     - Float signal input channel 3
   * - 🠺Integer In 1
     - In
     - ``IntSignalBlock``
     - Integer signal input channel 1


Stream Metadata
===============

None generated (no output ports).
