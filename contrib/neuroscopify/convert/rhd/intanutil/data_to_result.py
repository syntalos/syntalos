#! /bin/env python
#
# Michael Gibson 27 April 2015

def data_to_result(header, data, data_present):
    """Moves the header and data (if present) into a common object."""

    result = {}
    result['notes'] = header['notes']
    result['frequency_parameters'] = header['frequency_parameters']

    if header['num_amplifier_channels'] > 0:
        result['amplifier_channels'] = header['amplifier_channels']
        if data_present:
            result['amplifier_data'] = data['amplifier_data']
            result['t_amplifier'] = data['t_amplifier']
            result['spike_triggers'] = header['spike_triggers']

    if header['num_aux_input_channels'] > 0:
        result['aux_input_channels'] = header['aux_input_channels']
        if data_present:
            result['aux_input_data'] = data['aux_input_data']
            result['t_aux_input'] = data['t_aux_input']

    if header['num_supply_voltage_channels'] > 0:
        result['supply_voltage_channels'] = header['supply_voltage_channels']
        if data_present:
            result['supply_voltage_data'] = data['supply_voltage_data']
            result['t_supply_voltage'] = data['t_supply_voltage']

    if header['num_board_adc_channels'] > 0:
        result['board_adc_channels'] = header['board_adc_channels']
        if data_present:
            result['board_adc_data'] = data['board_adc_data']
            result['t_board_adc'] = data['t_board_adc']

    if header['num_board_dig_in_channels'] > 0:
        result['board_dig_in_channels'] = header['board_dig_in_channels']
        if data_present:
            result['board_dig_in_data'] = data['board_dig_in_data']
            result['t_dig'] = data['t_dig']

    if header['num_board_dig_out_channels'] > 0:
        result['board_dig_out_channels'] = header['board_dig_out_channels']
        if data_present:
            result['board_dig_out_data'] = data['board_dig_out_data']
            result['t_dig'] = data['t_dig']

    if header['num_temp_sensor_channels'] > 0:
        if data_present:
            result['temp_sensor_data'] = data['temp_sensor_data']
            result['t_temp_sensor'] = data['t_temp_sensor']

    return result
