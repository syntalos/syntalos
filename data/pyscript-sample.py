import maio as io
from maio import InputWaitResult

# Get your port references by their ID here.
# Examples:
iport = io.get_input_port('video-in')
oport = io.get_output_port('video-out')

# Set appropriate metadata on output ports
oport.set_metadata_value_int('framerate', 200)
oport.set_metadata_value_dim('size', [800, 600])


def loop():
    '''
    This is executed by Syntalos continuously until you return False.
    Use this function to retrieve input and send it for processing.
    '''

    # wait for new input to arrive
    wait_result = io.await_new_input()
    if wait_result == InputWaitResult.CANCELLED:
        # the run has been cancelled - finalize data, then terminate
        # the loop will not be called again, even if True is returned
        print('Quitting PyOOPTestModule Loop!', wait_result)
        return False

    # retrieve data from our ports until we run out of data to process
    while True:
        frame = iport.next()
        if frame is None:
            # no more data, exit
            break

        # do something with the data here!

        # submit data to an output port
        oport.submit(frame)

    # return True, so the loop function is called again when
    # new data is available
    return True
