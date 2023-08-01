import syio as sy
from syio import InputWaitResult


# NOTE: Check https://syntalos.readthedocs.io/en/latest/modules/pyscript.html for documentation
# on how to write a useful script.
sy.raise_error(
    (
        'You are running the Python example script - please edit it to work for your experiment!\n'
        '(Check https://syntalos.readthedocs.io/en/latest/modules/pyscript.html for help)'
    )
)


def prepare():
    """This function is called before a run is started.
    You can use it for (slow) initializations."""
    pass


def start():
    """This function is called immediately when a run is started.
    This function should complete quickly."""
    pass


def loop() -> bool:
    """This function is executed by Syntalos continuously until it returns False.
    Use this function to retrieve input and process it"""

    # wait for new input to arrive
    wait_result = sy.await_new_input()
    if wait_result != InputWaitResult.NEWDATA:
        # stop looping if the run has been cancelled
        return wait_result != InputWaitResult.CANCELLED

    while True:
        # TODO: Retrieve data from our ports until we run out of data to process
        pass

    return True


def stop():
    """This function is called once a run is stopped."""
    pass
