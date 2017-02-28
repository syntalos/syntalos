
import os
import numpy as np
from .rhd import *


def chan_names(result, key):
    if key in result:
        return [chan['native_channel_name'] for chan in result[key]]
    else:
        return []

amp_chan_names = lambda result: chan_names(result, 'amplifier_channels')
dig_in_chan_names = lambda result: chan_names(result, 'board_dig_in_channels')

def rhd2dat(rhd_fname, dat_fname):

    result = read_data(rhd_fname, no_floats=True)

    amplifier_channels = amp_chan_names(result)
    data_array = result['amplifier_data']

    dig_in_channels = dig_in_chan_names(result)
    if dig_in_channels:
        data_array = np.vstack((data_array, result['board_dig_in_data']))

    data_array = data_array.astype(np.uint16)
    with open(dat_fname, 'wb') as fp:
        fp.write(data_array.T.tobytes())

    return result
