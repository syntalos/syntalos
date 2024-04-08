import syio as sy


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

def run():
    """This function is called once the experiment run has started."""

    # wait for new data to arrive and communicate with Syntalos
    while sy.is_running():
        sy.await_data()


def stop():
    """This function is called once a run is stopped."""
    pass
