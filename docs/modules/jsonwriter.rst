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

Reading Data
------------

It is recommended to load the generated data using the `edlio <https://edl.readthedocs.io/latest/>`_
Python module, which will do the right thing automatically.
You can also manually open the data and read it though.

To load the generated data with Pandas directly (without using the ``edlio`` module),
and if you have selected the *Pandas-compatible JSON* schema, you can use this Python snippet:

.. code-block:: python

    import pandas as pd

    df = pd.read_json('/path/to/data.json.zst', orient='split')
    print(df)


If you selected the *Metadata-extended JSON* schema, you can also load the data manually,
including the stored metadata:

.. code-block:: python

    import json
    import zstandard as zstd
    import pandas as pd

    with open('/path/to/data.json.zst', 'rb') as f:
        dctx = zstd.ZstdDecompressor()
        zr = dctx.stream_reader(f)
        jd = json.load(zr)

    collection_id = jd.pop('collection_id')
    time_unit = jd.pop('time_unit')
    data_unit = jd.pop('data_unit')
    df = pd.DataFrame(jd.pop('data'), columns=jd.pop('columns'))

    print(collection_id)
    print(time_unit, data_unit)
    print(df)


JSON Extensions
---------------

Canonical standard-compliant JSON does not allow special types for infinity as well as
non-numbers for numeric values, and also does not distinguish between floating-point
numbers and integers.

To be more precise, this module will return ``Infinity``/``-Infinity`` for positive/negative
infinite values, as well as ``NaN`` for non-numbers. These values are not covered by
the JSON specification, but parsed by Pandas and many other JSON parsers.

Additionally, floating-point numbers will always contain a dot, while integers will never,
making them distinguishable.


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

No output streams are generated, but input streams of type ``FloatSignalBlock``/``IntSignalBlock`` must have
``time_unit``, ``data_unit`` and ``signal_names`` set.

For ``TableRow`` data, the ``table_header`` metadata entry has to be set.
