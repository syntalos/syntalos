Reading recorded data
#####################

TODO

Syntalos will store all data recorded with it in a defined directory structure, called "**EDL**" (Experiment Directory Layout).

Data can be read manually without specialized tools, but to make things easier we also provide the
`edlio <https://github.com/bothlab/edlio>`_ Python module which can load EDL directories and also apply any timestamps
stored in binary tsync files to their respective data automatically. You can install it via pip: ``pip install edlio``.

If you just want to convert any *tsync* binary files to their text representations, the **syntalos-metaview** command-line
tool can help you with that.
