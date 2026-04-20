import syntalos_mlink as syl

# NOTE: Check https://syntalos.org/docs/modules/pyscript/ for documentation
# on how to write a useful script.
syl.raise_error(
    (
        'You are running the Python example script - please edit it to work for your experiment!\n'
        '(Check https://syntalos.org/docs/modules/pyscript/ for help)'
    )
)


def prepare() -> bool:
    """This function is called before a run is started.
    You can use it for (slow) initializations."""
    return True


def start():
    """This function is called immediately when a run is started.
    This function should complete extremely quickly."""
    pass


def stop():
    """This function is called once a run is stopped."""
    pass
