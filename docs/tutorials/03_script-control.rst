03. Customization with Python scripts
#####################################

Syntalos allows users to write custom `Python <https://docs.python.org/3/tutorial/>`_
scripts to control module behavior, to realize a wite array of different experiments and to
tailor module behavior to the user's need.
This tutorial is a basic introduction for using the `Python Script` module in Syntalos.

1. Preparations
===============

We want to create a project that emits a sound at regular intervals.
Create a new Syntalos project and add an `Audio Source` and a `Python Script` module.
Notice the *Control* input port on the `Audio Source` module: It looks like we can use it
to control the activity of this module via it! Unfortunately, the `Python Script` does not seem
to have any input or output ports!
We will change that in the next step.

.. image:: /graphics/syntalos-pyscript-audiosrc-raw.avif
  :width: 340
  :alt: A PyScript and AudioSource module

2. Adding ports
===============

In order to control other modules from our Python script module, we need it to have an output
port to even emit data. Double-click the Python module or click on *Settings* after selecting it).

A new window opens which lets you edit the Python code. We ignore that for now and click on
*Ports → Edit* in the window's main menu. A new *Port Editor* dialog opens, which allows for adding
new ports to our script module.
Since we want to add an output port, click on *Add Output Port*.

In the next step, you will be asked which kind of data the output port emits. Select `ControlCommand`
from the list. The next question is for an internal name of the port that we will use in our script.
Enter ``control-out`` and confirm. The next question is about a human name for the port that will be
displayed on Syntalos' user interface. Just enter ``Control`` there.
Then confirm all changes with *OK*:

.. image:: /graphics/pyscript-ports-dialog.avif
  :width: 400
  :alt: The ports configuration dialog

After that, a *Control* output port on the *Python Script* module should appear. Connect it to the *Control*
input port of your *Audio Source* as usual!

3. Module coding basics
=======================

Now we can look at the Python script itself! The default script is quite large due to its many annotations, but
using it is really quite simple: 4 functions exists that Syntalos may call at different stages of an experiment run.
The ``prepare()`` function is called when the experimence is started, but before all modules are ready, the
``start()`` function is called immediately before data starts being acquired, ``loop`` is called continuously during
the experiment until the function returns ``False``, and ``stop()`` is called when the experiment is stopped.
We are primarily concerned with the ``loop()`` function here, as we do not need to prepare any data or device
in our script. Interaction with Syntalos happens via the ``syio`` module, which is imported by default.

First, we need to get a Python reference to the ``control-out`` output port that we just defined in the port editor.
This needs to happen before even ``prepare()`` is called, so we put it at the top of the Python file:

.. code-block:: python

  oport = sy.get_output_port('control-out')

Using ``get_output_port()`` with the internal port name, we now can emit messages on this port.

4. Controlling a module
=======================

The ``ControlCommand`` datatype we selected for our output port is Syntalos' generic data structure for controlling
the state of most modules. You can start modules, stop them or let them run for a set amount of seconds.
This is useful for example if you only want to record a video for a selected amount of time.
For now though, we just want to emit a 2 sec audio cue every 5 seconds. This can be done with this simple snippet
of Python code (this is the whole script file):

.. code-block:: python
   :linenos:
   :emphasize-lines: 10,14

   import syio as sy
   from syio import InputWaitResult, ControlCommand, ControlCommandKind


   oport = sy.get_output_port('control-out')


   def loop() -> bool:
       ctl = ControlCommand()
       ctl.kind = ControlCommandKind.START
       ctl.duration = 2000  # run for 2 sec
       while True:
           oport.submit(ctl)
           sy.wait_sec(5)
           if not sy.check_running():
               return False

       return True

The ``loop()`` function is called permanently while the experiment runs. We first define a ``ControlCommand`` that we want to
send to the *Audio Source*, and tell it to be of kind ``START`` and instruct it to hold that state for ``2000`` milliseconds
before falling back to its previous state.
Then, we just loop endlessly and submit the control command on our predefined output port ``oport``, wait 5 seconds and then
repeat the process.
Any datatypes you can use with output ports, and commands you can use on input ports can be found in the
:doc:`syntalos_mlink API documentation </pysy-mlink-api>` for reference.

.. note::
    While using Python's own wait functions, like ``time.sleep()``, is possible for delays, it is recommended to use
    functions from ``syio`` for that purpose. That way Syntalos knows about the waiting state of the  module,
    and can disrupt a sleeping module to stop it instead of waiting for it. It also allows Syntalos to make smarter
    scheduling and queueing decisions.

By calling ``sy.check_running()`` in our endless loop, we can check if the Syntalos experiment is still running, and
terminate voluntarily in case it is not. Otherwise, Syntalos will interrupt script execution if a script does not react
in time to a stop request.

4. Run it!
==========

You can now run the Syntalos experiment! You should hear a beep sound every 5 seconds. If not (and if your speakers are fine),
you can inspect any Python script errors directly in the script window (it has a log at the bottom).

5. Expand it!
=============

This simple example can be easily expanded. For example, you can only record from a camera while a sound is played,
or only record while no sound is played.
To have finer control over modules, you may need to add multiple control output ports with different IDs.
Play around a bit and make the script work for your experiment!

.. image:: /graphics/pyscript-audiosrc-recording-example.avif
  :width: 360
  :alt: Controlling multiple modules from one port
