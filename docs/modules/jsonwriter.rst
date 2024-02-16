JSON Writer
###########
.. image:: ../../modules/jsonwriter/jsonwriter.svg
   :width: 72
   :align: right

This module writes various data into a `Zstd <https://en.wikipedia.org/wiki/Zstd>`_-compressed
`JSON <https://en.wikipedia.org/wiki/JSON>`_ textfile.

Data saved like this can be read back using the `Pandas <https://pandas.pydata.org/>`_
module in Python, or with any available JSON parser after decompression.


Usage
=====

Only one input modality is accepted at a time!
Make sure you only have one input port connected at a time.

The module settings allow storing data in *Pandas-compatible JSON* or
*Metadata-extended JSON* schemas. The only difference is that in the latter case,
a bunch of extra metadata is added to the JSON file (like units of measurement used,
or the experiment ID) that is not present in the *Pandas* variant.

The extra data makes Pandas fail to load the generated data if passed without preprocessing,
which is why both options are available for convenience.
Metadata will always also be stored in the EDL attributes of the generated dataset.

If an Integer or Float signal is connected, the channels to be recorded in JSON can be
manually selected. Otherwise, all data will be stored.

To load the generated data with Pandas directly (without using the ``edlio`` module),
you can use this Python snippet:

.. code-block:: python

    import pandas as pd

    df = pd.read_json('/path/to/data.json.zst', orient='split')
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

   * - 🠺Float Signals
     - In
     - ``FloatSignalBlock``
     - Float signal data
   * - 🠺Integer Signals
     - In
     - ``IntSignalBlock``
     - Integer signal data
   * - 🠺Table Rows
     - In
     - ``TableRow``
     - Table rows to save.


Stream Metadata
===============

No output streams are generated, but input streams must have ``time_unit``, ``data_unit`` and ``signal_names`` set.
If the time unit is `index`, ``sample_rate`` also has to be set on the incoming channel.
