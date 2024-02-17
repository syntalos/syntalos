Table
#####
.. image:: ../../data/modules/table.svg
   :width: 72
   :align: right

The "Table" module displays and saves any tabular content.


Usage
=====

No configuration required for now.
Data is saved unconditionally into a sanitized CSV table.

It is recommended to load the generated data using the `edlio <https://edl.readthedocs.io/latest/>`_
Python module, but you can also manually open the data and read it.
For example, using Pandas in Python:

.. code-block:: python

    import pandas as pd

    df = pd.read_csv('/path/to/data.csv', sep=';')
    print(df)


Ports
=====

.. list-table::
   :widths: 14 10 22 54
   :header-rows: 1

   * - Name
     - Direction
     - Data Type
     - Description

   * - 🠺Rows
     - In
     - ``TableRow``
     - Table rows to display an save.


Stream Metadata
===============

None generated (no output ports).
