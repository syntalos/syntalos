#! /bin/env python
#
# Michael Gibson 23 April 2015


def get_bytes_per_data_block(header):
    """Calculates the number of bytes in each 60-sample datablock."""

    # Each data block contains 60 amplifier samples.
    bytes_per_block = 60 * 4  # timestamp data
    bytes_per_block = bytes_per_block + 60 * 2 * header['num_amplifier_channels']

    # Auxiliary inputs are sampled 4x slower than amplifiers
    bytes_per_block = bytes_per_block + 15 * 2 * header['num_aux_input_channels']

    # Supply voltage is sampled 60x slower than amplifiers
    bytes_per_block = bytes_per_block + 1 * 2 * header['num_supply_voltage_channels']

    # Board analog inputs are sampled at same rate as amplifiers
    bytes_per_block = bytes_per_block + 60 * 2 * header['num_board_adc_channels']

    # Board digital inputs are sampled at same rate as amplifiers
    if header['num_board_dig_in_channels'] > 0:
        bytes_per_block = bytes_per_block + 60 * 2

    # Board digital outputs are sampled at same rate as amplifiers
    if header['num_board_dig_out_channels'] > 0:
        bytes_per_block = bytes_per_block + 60 * 2

    # Temp sensor is sampled 60x slower than amplifiers
    if header['num_temp_sensor_channels'] > 0:
        bytes_per_block = bytes_per_block + 1 * 2 * header['num_temp_sensor_channels']

    return bytes_per_block
  