Module syio
===========
Syntalos Interface

Functions
---------

    
`await_new_input() ‑> syio.InputWaitResult`
:   Wait for any new input to arrive via our input ports.

    
`get_input_port(arg0: str) ‑> object`
:   Get reference to input port with the give ID.

    
`get_output_port(arg0: str) ‑> object`
:   Get reference to output port with the give ID.

    
`new_firmatactl_with_id(arg0: syio.FirmataCommandKind, arg1: int) ‑> syio.FirmataControl`
:   Create new Firmata control command with a given pin ID.

    
`new_firmatactl_with_id_name(arg0: syio.FirmataCommandKind, arg1: int, arg2: str) ‑> syio.FirmataControl`
:   Create new Firmata control command with a given pin ID and registered name.

    
`new_firmatactl_with_name(arg0: syio.FirmataCommandKind, arg1: str) ‑> syio.FirmataControl`
:   Create new Firmata control command with a given pin name (the name needs to be registered previously).

    
`println(arg0: str)`
:   Print text to stdout.

    
`raise_error(arg0: str)`
:   Emit an error message string, immediately terminating the current action and (if applicable) the experiment.

    
`time_since_start_msec() ‑> int`
:   Get time since experiment started in milliseconds.

    
`time_since_start_usec() ‑> int`
:   Get time since experiment started in microseconds.

    
`wait(arg0: int)`
:   Wait (roughly) for the given amount of milliseconds without blocking communication with the master process.

    
`wait_sec(arg0: int)`
:   Wait (roughly) for the given amount of seconds without blocking communication with the master process.

Classes
-------

`ControlCommand(...)`
:   __init__(self: syio.ControlCommand) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `command`
    :   (self: syio.ControlCommand) -> QString

    `kind`
    :   (self: syio.ControlCommand) -> syio.ControlCommandKind

`ControlCommandKind(...)`
:   Members:
    
    UNKNOWN
    
    START
    
    PAUSE
    
    STOP
    
    STEP
    
    CUSTOM
    
    __init__(self: syio.ControlCommandKind, value: int) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Class variables

    `CUSTOM`
    :

    `PAUSE`
    :

    `START`
    :

    `STEP`
    :

    `STOP`
    :

    `UNKNOWN`
    :

    ### Instance variables

    `name`
    :   name(self: handle) -> str

`FirmataCommandKind(...)`
:   Members:
    
    UNKNOWN
    
    NEW_DIG_PIN
    
    NEW_ANA_PIN
    
    IO_MODE
    
    WRITE_ANALOG
    
    WRITE_DIGITAL
    
    WRITE_DIGITAL_PULSE
    
    SYSEX
    
    __init__(self: syio.FirmataCommandKind, value: int) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Class variables

    `IO_MODE`
    :

    `NEW_ANA_PIN`
    :

    `NEW_DIG_PIN`
    :

    `SYSEX`
    :

    `UNKNOWN`
    :

    `WRITE_ANALOG`
    :

    `WRITE_DIGITAL`
    :

    `WRITE_DIGITAL_PULSE`
    :

    ### Instance variables

    `name`
    :   name(self: handle) -> str

`FirmataControl(...)`
:   __init__(self: syio.FirmataControl) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `command`
    :   (self: syio.FirmataControl) -> syio.FirmataCommandKind

    `is_output`
    :   (self: syio.FirmataControl) -> bool

    `is_pullup`
    :   (self: syio.FirmataControl) -> bool

    `pin_id`
    :   (self: syio.FirmataControl) -> int

    `pin_name`
    :   (self: syio.FirmataControl) -> QString

    `value`
    :   (self: syio.FirmataControl) -> int

`FirmataData(...)`
:   __init__(self: syio.FirmataData) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `is_digital`
    :   (self: syio.FirmataData) -> bool

    `pin_id`
    :   (self: syio.FirmataData) -> int

    `pin_name`
    :   (self: syio.FirmataData) -> QString

    `time`
    :   (self: syio.FirmataData) -> datetime.timedelta

    `value`
    :   (self: syio.FirmataData) -> int

`Frame(...)`
:   __init__(self: syio.Frame) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `index`
    :   (self: syio.Frame) -> int

    `mat`
    :   (self: syio.Frame) -> numpy.ndarray

    `time_msec`
    :   (self: syio.Frame) -> datetime.timedelta

`InputPort(...)`
:   __init__(self: syio.InputPort, arg0: str, arg1: int) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `name`
    :   (self: syio.InputPort) -> str

    ### Methods

    `next(self: syio.InputPort) ‑> object`
    :   Retrieve the next element, return None if no element is available.

    `set_throttle_items_per_sec(self: syio.InputPort, items_per_sec: int, allow_more: bool = True)`
    :   Limit the amount of input received to a set amount of elements per second.

`InputWaitResult(...)`
:   Members:
    
    NONE
    
    NEWDATA
    
    CANCELLED
    
    __init__(self: syio.InputWaitResult, value: int) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Class variables

    `CANCELLED`
    :

    `NEWDATA`
    :

    `NONE`
    :

    ### Instance variables

    `name`
    :   name(self: handle) -> str

`OutputPort(...)`
:   __init__(self: syio.OutputPort, arg0: str, arg1: int) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Instance variables

    `name`
    :   (self: syio.OutputPort) -> str

    ### Methods

    `set_metadata_value(self: syio.OutputPort, arg0: str, arg1: object)`
    :   Set (immutable) metadata value for this port.

    `set_metadata_value_size(self: syio.OutputPort, arg0: str, arg1: list)`
    :   Set (immutable) metadata value for a 2D size type for this port.

    `submit(self: syio.OutputPort, arg0: object)`
    :

`SyntalosPyError(...)`
:   Common base class for all non-exit exceptions.

    ### Ancestors (in MRO)

    * builtins.Exception
    * builtins.BaseException

`VectorDouble(...)`
:   __init__(*args, **kwargs)
    Overloaded function.
    
    1. __init__(self: syio.VectorDouble) -> None
    
    2. __init__(self: syio.VectorDouble, arg0: syio.VectorDouble) -> None
    
    Copy constructor
    
    3. __init__(self: syio.VectorDouble, arg0: Iterable) -> None

    ### Ancestors (in MRO)

    * pybind11_builtins.pybind11_object

    ### Methods

    `append(self: syio.VectorDouble, x: float)`
    :   Add an item to the end of the list

    `clear(self: syio.VectorDouble)`
    :   Clear the contents

    `count(self: syio.VectorDouble, x: float) ‑> int`
    :   Return the number of times ``x`` appears in the list

    `extend(*args, **kwargs)`
    :   Overloaded function.
        
        1. extend(self: syio.VectorDouble, L: syio.VectorDouble) -> None
        
        Extend the list by appending all the items in the given list
        
        2. extend(self: syio.VectorDouble, L: Iterable) -> None
        
        Extend the list by appending all the items in the given list

    `insert(self: syio.VectorDouble, i: int, x: float)`
    :   Insert an item at a given position.

    `pop(*args, **kwargs)`
    :   Overloaded function.
        
        1. pop(self: syio.VectorDouble) -> float
        
        Remove and return the last item
        
        2. pop(self: syio.VectorDouble, i: int) -> float
        
        Remove and return the item at index ``i``

    `remove(self: syio.VectorDouble, x: float)`
    :   Remove the first item from the list whose value is x. It is an error if there is no such item.
