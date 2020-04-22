import syio as sy
from syio import InputWaitResult

# Get references to your ports by their ID here.
# Examples:
iport = sy.get_input_port('video-in')
oport = sy.get_output_port('video-out')


def prepare():
    '''
    This function is called before a run is started.
    You can use it for (slow) initializations.
    NOTE: You are *not* able to send output to ports here, or access
    any valid master timer time. This function can be slow.
    '''

    # Set appropriate metadata on output ports
    oport.set_metadata_value('framerate', 200)
    oport.set_metadata_value_size('size', [800, 600])


def start():
    '''
    This function is called immediately when a run is started.
    Access to the timer is available, and data can be sent via ports.
    You can *not* change any port metadata anymore from this point onward.
    This function should be fast, many modules are already running at this point.
    '''
    pass


def loop() -> bool:
    '''
    This function is executed by Syntalos continuously until it returns False.
    Use this function to retrieve input and process it, or run any other
    repeatable action. Keep in mind that you will not receive any new input
    unless `sy.await_new_input()` is called.
    '''

    # wait for new input to arrive
    wait_result = sy.await_new_input()
    if wait_result == InputWaitResult.CANCELLED:
        # the run has been cancelled (by the user or an error),
        # so this function will not be called again
        return False

    # retrieve data from our ports until we run out of data to process
    while True:
        frame = iport.next()
        if frame is None:
            # no more data, exit
            break

        # TODO: do something with the data here!

        # submit data to an output port
        oport.submit(frame)

    # return True, so the loop function is called again when new data is available
    return True


def stop():
    '''
    This function is called once a run is stopped, by the user, and error or when
    the loop() function returned False.
    '''
    pass
