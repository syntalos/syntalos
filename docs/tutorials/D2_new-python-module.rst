D2. Creating a new Python Module
################################

These instructions exit to create a new Python module from scratch.
If you just want to run some custom Python code, using the existing :doc:`Python Script </modules/pyscript>`
module is a much easier solution.

1. Module Location
==================

First, decice on a short, lower-case name for your modue. The name will be the unique identifier
for modules of your new type and must not be changed in future (or otherwise existing configurations might break).

If you are compiling Syntalos manually, you can add your new Python module directly to the source tree in
the ``modules/`` directory, and then add a new ``subdir`` directive for it to the toplevel
`modules/meson.build <https://github.com/bothlab/syntalos/blob/master/modules/meson.build>`_ file.

Alternatively, you can also have Python modules loaded from your home directory. Syntalos will look
in the following locations:

* ``~/.local/share/DraguhnLab/Syntalos/modules`` for normal installations
* ``~/.var/app/io.github.bothlab.syntalos/data/modules`` if installed as Flatpak bundle

Any modules copied there will be automatically loaded.


2. Copy a Template
==================

The easiest way to start building a new module is to copy a template to have any boilerplate present.
A minimal Python module exists in the form of `example-py <https://github.com/bothlab/syntalos/tree/master/modules/example-py>`_.
Copy its directory to the location where you develop your module, and rename it to your chosen ID name.


3. Adjust Metadata
==================

Open the copied ``module.toml`` file in your new module directory:

.. code-block:: toml

    [syntalos_module]
    type = "python"

    name = "Python Module Example"
    description = "Example & template for a Syntalos Python module."
    icon = "penrose-py.svg"

    main = "mod-main.py"
    use_venv = false

    devel = true

    [ports]

        [[ports.in]]
        data_type = 'Frame'
        id = 'frames-in'
        title = 'Frames'

        [[ports.out]]
        data_type = 'TableRow'
        id = 'rows-out'
        title = 'Indices'

        [[ports.out]]
        data_type = 'Frame'
        id = 'frames-out'
        title = 'Marked Frames'

Using the ``name`` and ``description`` fields, you can set a human-readable name and short description for your module.
The ``icon`` field denotes the filename of an icon file to visually represent your module relative to the module's root directory.
It is recommended to pick a vector graphic in SVG/SVGZ format here, but a PNG raster graphic will also work.

You can delete the ``devel = true`` field, as that hides the module by default and makes it only visible when Syntalos' developer
mode is active.

By specifying a Python file in ``main``, you can select which Python file will be the main entrypoint for your module. By setting
``use_venv`` to ``true``, Syntalos will also run your module in its own virtual environment, and will install any Python dependencies
from a ``requirements.txt`` file in the module's folder.

Lastly, you need to define the module's input/output ports in the ``ports`` list. The ``ports.in`` key denotes an input port, while
``ports.out`` denotes an output port.
Each port must have a ``data_type`` with the unique name of the data that it transfers. refer to the Python Script module for a full list.
The ``id`` of a port is a unique ID that can be used to reference the port in Python code, while ``title`` is the actual human-readable
name that is displayed in the Syntalos GUI.

4. Write your code
==================

After setting all metadata, it is time to actually write your module's code!
Open ``mod-main.py`` for an example. The Python module has the same familiar ``prepare()``, ``start()``, ``loop()`` and ``stop()``
functions like a Python Script module, that Syntalos will call at the appropriate time.
Ports are also accessed the same way, and data is also submitted the same way. Refer to the :doc:`syntalos_mlink API documentation </pysy-mlink-api>`
for a full reference of all available methods.

In addition to the known methods, a Python module also has a ``set_settings(settings: bytes)`` and ``change_settings(old_settings: bytes) -> bytes``
entry point. The former is called before a run is started with the module's settings serialized as bytes, in order for them to be applied before
the run is launched.
The latter is invoked by the user wanting to change module settings. Syntalos will provide the old settings, and it is up to the module to
draw a GUI to enable the user to change any of its settings

The example module will draw a simple GUI using tkInter, but for visual compatibility with the rest of the Syntalos UI, using PyQt5 or PySide
is recommended.


5. Test
=======

If your module is located in one of Syntalos' recognized locations, it should now show up in the module list, along all other modules,
and you should be able to use it as normal and test its functions.
