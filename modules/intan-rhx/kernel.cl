//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

typedef struct _hoop_info
{
    float t_a;
    float y_a;
    float t_b;
    float y_b;
} hoop_info_struct;

typedef struct _unit_hoops
{
    hoop_info_struct hoop_info[4];
} unit_hoops_struct;

typedef struct _channel_hoops
{
    unit_hoops_struct unit_hoops[4];
    float threshold;
    char use_hoops; // Ideally would be bool, but bool can't be passed to OpenCL. 1 = true, 0 = false.
} channel_hoops_struct;

typedef struct _gl_params
{
    ushort words_per_frame;
    char type;
    char num_streams;
    char snippet_size;
    float sample_rate;
    float spike_max;
    char spike_max_enabled; // Ideally would  be bool, but bool can't be passed to OpenCL. 1 = true, 0 = false.
} global_param_struct;


typedef struct _filter_iteration_params
{
    float b2;
    float b1;
    float b0;
    float a2;
    float a1;
} filter_iteration_param_struct;

// 2nd order, 2nd order, 2nd order, 1st/2nd order

typedef struct _filter_params
{
    char low_order;
    char high_order;

    filter_iteration_param_struct notch_params;
    filter_iteration_param_struct low_params[4];
    filter_iteration_param_struct high_params[4];
} filter_param_struct;

typedef struct _unit_detection
{
    bool hoops[4];
} hoop_detection_struct;


typedef struct _channel_detection
{
    hoop_detection_struct units[4];
    bool max_surpassed;
} channel_detection_struct;

// There's no way for const variables (must be const to create an array of that length) to be communicated as args.
#define samples_per_block 128

//__kernel void process_block(const global_param_struct global_parameters, const filter_param_struct filter_parameters,
__kernel void process_block(__global global_param_struct* restrict global_parameters,
                            __global filter_param_struct* restrict filter_parameters,
                            __global channel_hoops_struct* restrict hoops,
                            __global ushort* raw_block, __global float* prev_last_2, __global ushort* prev_high,
                            __global ushort* low, __global ushort* wide, __global ushort* high, __global uint* spike,
                            __global uchar* spikeIDs, __global ushort* start_search_pos)
{
    ushort words_per_frame = global_parameters->words_per_frame;
    char type = global_parameters->type;
    char num_streams = global_parameters->num_streams;
    float sample_rate = global_parameters->sample_rate;
    float sample_period = 1.0f / sample_rate;
    char snippet_size = global_parameters->snippet_size;
    bool spike_max_enabled = global_parameters->spike_max_enabled;
    float spike_max = global_parameters->spike_max;


    float notch_b2 = filter_parameters->notch_params.b2;
    float notch_b1 = filter_parameters->notch_params.b1;
    float notch_b0 = filter_parameters->notch_params.b0;
    float notch_a2 = filter_parameters->notch_params.a2;
    float notch_a1 = filter_parameters->notch_params.a1;

    float low_b2[4];
    float low_b1[4];
    float low_b0[4];
    float low_a2[4];
    float low_a1[4];

    bool low_order_odd = (filter_parameters->low_order % 2) == 1;
    bool high_order_odd = (filter_parameters->high_order % 2) == 1;

    for (uint filter_index = 0; filter_index < 4; ++filter_index) {
        low_b2[filter_index] = filter_parameters->low_params[filter_index].b2;
        low_b1[filter_index] = filter_parameters->low_params[filter_index].b1;
        low_b0[filter_index] = filter_parameters->low_params[filter_index].b0;
        low_a2[filter_index] = filter_parameters->low_params[filter_index].a2;
        low_a1[filter_index] = filter_parameters->low_params[filter_index].a1;
    }

    float high_b2[4];
    float high_b1[4];
    float high_b0[4];
    float high_a2[4];
    float high_a1[4];

    for (uint filter_index = 0; filter_index < 4; ++filter_index) {
        high_b2[filter_index] = filter_parameters->high_params[filter_index].b2;
        high_b1[filter_index] = filter_parameters->high_params[filter_index].b1;
        high_b0[filter_index] = filter_parameters->high_params[filter_index].b0;
        high_a2[filter_index] = filter_parameters->high_params[filter_index].a2;
        high_a1[filter_index] = filter_parameters->high_params[filter_index].a1;
    }

    uint channel_index = get_global_id(0);
    const int snippets_per_block = (int) ceil((float) ((float) samples_per_block / (float) snippet_size) + 1.0f);

    float in_float[samples_per_block];
    float prev_high_float[samples_per_block];
    float low_float[4][samples_per_block];
    float wide_float[samples_per_block];
    float high_float[4][samples_per_block];

    uint low_2nd_to_last_index[4];
    uint low_last_index[4];
    uint high_2nd_to_last_index[4];
    uint high_last_index[4];

    uint last_data_start = channel_index * 20;

    for (int i = 0; i < 4; ++i) {
        low_2nd_to_last_index[i] = (last_data_start) + i;
        low_last_index[i] = (last_data_start) + 4 + i;
        high_2nd_to_last_index[i] = (last_data_start) + 8 + i;
        high_last_index[i] = (last_data_start) + 12 + i;
    }

    uint in_2nd_to_last_index = (last_data_start) + 16;
    uint in_last_index = (last_data_start) + 17;

    uint wide_2nd_to_last_index = (last_data_start) + 18;
    uint wide_last_index = (last_data_start) + 19;

    uint out_index;
    uint s;
    int snippet_index = 0;

    float low_2nd_to_last[4];
    float low_last[4];
    float high_2nd_to_last[4];
    float high_last[4];

    for (int i = 0; i < 4; ++i) {
        low_2nd_to_last[i] = prev_last_2[low_2nd_to_last_index[i]];
        low_last[i] = prev_last_2[low_last_index[i]];
        high_2nd_to_last[i] = prev_last_2[high_2nd_to_last_index[i]];
        high_last[i] = prev_last_2[high_last_index[i]];
    }

    float in_2nd_to_last = prev_last_2[in_2nd_to_last_index];
    float in_last = prev_last_2[in_last_index];

    float wide_2nd_to_last = prev_last_2[wide_2nd_to_last_index];
    float wide_last = prev_last_2[wide_last_index];

    float threshold = hoops[channel_index].threshold;
    bool use_hoops = (hoops[channel_index].use_hoops == 1) ? true : false;

    for (s = 0; s < snippets_per_block; ++s) {
        spike[s * get_global_size(0) + channel_index] = 0;
        spikeIDs[s * get_global_size(0) + channel_index] = 0;
    }

    for (s = 0; s < snippet_size; ++s) {
        prev_high_float[s] = (float) (0.195f * (((float)prev_high[s * get_global_size(0) + channel_index]) - 32768));
    }

    int in_index_stream;
    int in_index_channel;
    //if (type == ControllerRecordUSB2 || type == ControllerRecordUSB3) {
    if (type == 0 || type == 1) {
        in_index_stream = channel_index / 32;
        in_index_channel = channel_index % 32;
    } else {
        in_index_stream = channel_index / 16;
        in_index_channel = channel_index % 16;
    }

    // (0) Index this channel's input data from the raw_block and convert it to float.
    int startingIndex;
    //if (type == ControllerStimRecord) {
    if (type == 2) {
        startingIndex = 6 + (num_streams * 3 * 2) + (in_index_channel * num_streams * 2) + (2 * in_index_stream + 1);
    } else {
        startingIndex = 6 + (num_streams * 3) + in_index_channel * num_streams + in_index_stream;
    }

    for (int frame = 0; frame < samples_per_block; ++frame) {
        float ac_sample = (float) raw_block[words_per_frame * frame + startingIndex];
        in_float[frame] = (float)(0.195f * ((ac_sample) - 32768));
    }

    int num_low_filter_iterations = floor((float)(filter_parameters->low_order - 1) / 2.0f) + 1;
    int num_high_filter_iterations = floor((float)(filter_parameters->high_order - 1) / 2.0f) + 1;

    // s == 0 condition
    // (1) IIR notch filter into wide_float
    wide_float[0] = notch_b2 * in_2nd_to_last + notch_b1 * in_last + notch_b0 * in_float[0] - notch_a2 * wide_2nd_to_last - notch_a1 * wide_last;

    // (2) IIR Nth-order low-pass
    // 1st iteration: use wide_float as input.
    low_float[0][0] = low_b2[0] * wide_2nd_to_last + low_b1[0] * wide_last + low_b0[0] * wide_float[0] - low_a2[0] * low_2nd_to_last[0] -low_a1[0] * low_last[0];

    // All other iterations: use low_float[filter_index - 1] as input.
    for (uint filter_index = 1; filter_index < num_low_filter_iterations; ++filter_index) {
        low_float[filter_index][0] = low_b2[filter_index] * low_2nd_to_last[filter_index - 1] +
                low_b1[filter_index] * low_last[filter_index - 1] +
                low_b0[filter_index] * low_float[filter_index - 1][0] -
                low_a2[filter_index] * low_2nd_to_last[filter_index] -
                low_a1[filter_index] * low_last[filter_index];
    }

    // (3) IIR Nth-order high-pass
    // 1st iteration: use wide_float as input.
    high_float[0][0] = high_b2[0] * wide_2nd_to_last + high_b1[0] * wide_last + high_b0[0] * wide_float[0] - high_a2[0] * high_2nd_to_last[0] - high_a1[0] * high_last[0];

    // All other iterations: use high-float[filter_index - 1] as input.
    for (uint filter_index = 1; filter_index < num_high_filter_iterations; ++filter_index) {
        high_float[filter_index][0] = high_b2[filter_index] * high_2nd_to_last[filter_index - 1] +
                high_b1[filter_index] * high_last[filter_index - 1] +
                high_b0[filter_index] * high_float[filter_index - 1][0] -
                high_a2[filter_index] * high_2nd_to_last[filter_index] -
                high_a1[filter_index] * high_last[filter_index];
    }


    // s == 1 condition
    // (1) IIR notch filter into wide_float
    wide_float[1] = notch_b2 * in_last + notch_b1 * in_float[0] + notch_b0 * in_float[1] - notch_a2 * wide_last - notch_a1 * wide_float[0];

    // (2) IIR Nth-order low-pass
    // 1st iteration: use wide_float as input.
    low_float[0][1] = low_b2[0] * wide_last + low_b1[0] * wide_float[0] + low_b0[0] * wide_float[1] - low_a2[0] * low_last[0] - low_a1[0] * low_float[0][0];

    // All other iterations: use low_float[filter_index - 1] as input.
    for (uint filter_index = 1; filter_index < num_low_filter_iterations; ++filter_index) {
        low_float[filter_index][1] = low_b2[filter_index] * low_last[filter_index - 1] +
                low_b1[filter_index] * low_float[filter_index - 1][0] +
                low_b0[filter_index] * low_float[filter_index - 1][1] -
                low_a2[filter_index] * low_last[filter_index] -
                low_a1[filter_index] * low_float[filter_index][0];
    }

    // (3) IIR Nth-order high-pass
    // 1st iteration: use wide_float as input.
    high_float[0][1] = high_b2[0] * wide_last + high_b1[0] * wide_float[0] + high_b0[0] * wide_float[1] - high_a2[0] * high_last[0] - high_a1[0] * high_float[0][0];

    // All other iterations: use high_float[filter_index - 1] as input.
    for (uint filter_index = 1; filter_index < num_high_filter_iterations; ++filter_index) {
        high_float[filter_index][1] = high_b2[filter_index] * high_last[filter_index - 1] +
                high_b1[filter_index] * high_float[filter_index - 1][0] +
                high_b0[filter_index] * high_float[filter_index - 1][1] -
                high_a2[filter_index] * high_last[filter_index] -
                high_a1[filter_index] * high_float[filter_index][0];
    }


    for (s = 2; s < samples_per_block; ++s) {

        // (1) IIR notch filter into wide_float
        wide_float[s] = notch_b2 * in_float[s - 2] + notch_b1 * in_float[s - 1] + notch_b0 * in_float[s] - notch_a2 * wide_float[s - 2] - notch_a1 * wide_float[s - 1];

        // (2) IIR Nth-order low-pass
        // 1st iteration: use wide_float as input.
        low_float[0][s] = low_b2[0] * wide_float[s - 2] + low_b1[0] * wide_float[s - 1] + low_b0[0] * wide_float[s] - low_a2[0] * low_float[0][s - 2] - low_a1[0] * low_float[0][s - 1];

        // All other iterations: use low_float[filter_index - 1] as input.
        for (uint filter_index = 1; filter_index < num_low_filter_iterations; ++filter_index) {
            low_float[filter_index][s] = low_b2[filter_index] * low_float[filter_index - 1][s - 2] +
                    low_b1[filter_index] * low_float[filter_index - 1][s - 1] +
                    low_b0[filter_index] * low_float[filter_index - 1][s] -
                    low_a2[filter_index] * low_float[filter_index][s - 2] -
                    low_a1[filter_index] * low_float[filter_index][s - 1];
        }

        // (3) IIR Nth-order high-pass
        // 1st iteration: use wide_float as input.
        high_float[0][s] = high_b2[0] * wide_float[s - 2] + high_b1[0] * wide_float[s - 1] + high_b0[0] * wide_float[s] - high_a2[0] * high_float[0][s - 2] - high_a1[0] * high_float[0][s - 1];

        // All other iterations: use high_float[filter_index - 1] as input.
        for (uint filter_index = 1; filter_index < num_high_filter_iterations; ++filter_index) {
            high_float[filter_index][s] = high_b2[filter_index] * high_float[filter_index - 1][s - 2] +
                    high_b1[filter_index] * high_float[filter_index - 1][s - 1] +
                    high_b0[filter_index] * high_float[filter_index - 1][s] -
                    high_a2[filter_index] * high_float[filter_index][s - 2] -
                    high_a1[filter_index] * high_float[filter_index][s - 1];
        }
    }

    float filtered_high[samples_per_block];
    float filtered_low[samples_per_block];

    for (int s = 0; s < samples_per_block; ++s) {
        filtered_high[s] = high_float[num_high_filter_iterations - 1][s];
        filtered_low[s] = low_float[num_low_filter_iterations - 1][s];
    }

    // Across this block, look for any valid rectangle and look back to this block and the previous block to
    // determine valid t0. Add earliest t0 for each rectangle to 'spike' output.
    snippet_index = 0;

    // Start with thresh_s = start_search_pos[channel_index]. This is 0 unless the previous data block ended with a spike
    // In that case, thresh_s is a non-zero offset to avoid double-detecting a snippet.
    for (int thresh_s = start_search_pos[channel_index] - snippet_size; thresh_s < samples_per_block - snippet_size; ++thresh_s) {

        start_search_pos[channel_index] = 0;

        // Look to both this data block and the previous block to determine if the threshold was surpassed.
        bool surpassed = false;

        if (threshold >= 0) {  // If threshold was positive:
            if (thresh_s >= 0) {
                if (filtered_high[thresh_s] > threshold) surpassed = true;
            } else {
                if (prev_high_float[snippet_size + thresh_s] > threshold) surpassed = true;
            }
        } else {  // If threshold was negative:
            if (thresh_s >= 0) {
                if (filtered_high[thresh_s] < threshold) surpassed = true;
            } else {
                if (prev_high_float[snippet_size + thresh_s] < threshold) surpassed = true;
            }
        }

        // Threshold was surpassed.
        if (surpassed) {

            // For ease of understanding, move the samples from [thresh_s, thresh_s + snippet_size] to [0, snippet_size].
            float this_snippet[samples_per_block];
            for (int i = 0; i < snippet_size; ++i) {
                int this_s = thresh_s + i;
                if (this_s < 0) {
                    this_snippet[i] = prev_high_float[snippet_size + this_s];
                } else {
                    this_snippet[i] = filtered_high[this_s];
                }
            }

            // Create a struct to hold this channel's hoop info.
            channel_detection_struct detection;
            for (int unit = 0; unit < 4; ++unit) {
                for (int hoop = 0; hoop < 4; ++hoop) {
                    detection.units[unit].hoops[hoop] = false;
                }
            }
            detection.max_surpassed = false;

            // If spike_max_enabled is true, then see if any samples in this snippet surpass spike_max. If they do,
            // then mark detection.max_surpassed as true and save which sample.
            if (spike_max_enabled) {
                for (int i = 0; i < snippet_size; ++i) {
                    if (spike_max >= 0 && this_snippet[i] >= spike_max) {
                        detection.max_surpassed = true;
                        break;
                    }
                    if (spike_max < 0 && this_snippet[i] <= spike_max) {
                        detection.max_surpassed = true;
                        break;
                    }
                }
            }


            // If use_hoops is true, then go through all units populating detection.units[unit].hoops[hoop].
            if (use_hoops) {
                // Go through all units.
                for (int unit = 0; unit < 4; ++unit) {

                    // If this unit has no valid hoops (all t_a values are -1.0f), then this is an inactive unit which
                    // should be treated as having no intersect; just go on to the next unit.
                    if (hoops[channel_index].unit_hoops[unit].hoop_info[0].t_a == -1.0f &&
                            hoops[channel_index].unit_hoops[unit].hoop_info[1].t_a == -1.0f &&
                            hoops[channel_index].unit_hoops[unit].hoop_info[2].t_a == -1.0f &&
                            hoops[channel_index].unit_hoops[unit].hoop_info[3].t_a == -1.0f) {
                        continue;
                    }

                    // Go through all hoops.
                    for (int hoop = 0; hoop < 4; ++hoop) {

                        hoop_info_struct this_hoop = hoops[channel_index].unit_hoops[unit].hoop_info[hoop];

                        // If this hoop info is invalid (t_a is -1.0f), then this is an inactive hoop, which by default passes.
                        // Set true and continue. If all hoops are inactive, then we would have already passed on to the next
                        // unit without flagging an intersect.
                        if (this_hoop.t_a == -1.0f) {
                            detection.units[unit].hoops[hoop] = true;
                            continue;
                        }

                        float t_a = this_hoop.t_a;
                        float y_a = this_hoop.y_a;
                        float t_b = this_hoop.t_b;
                        float y_b = this_hoop.y_b;

                        // In range [t_a, t_b], does line segment from (t_1, y_1) to (t_2, y_2) intersect user-defined hoop?
                        // If so, mark hoop as jumped through by setting intersect to true.
                        bool intersect = false;

                        // Round t_a down and t_b up to the nearest discrete sample.
                        int s_a = floor(sample_rate * t_a);
                        int s_b = ceil(sample_rate * t_b);

                        if (s_a == s_b) {  // Special case: vertical hoop
                            float y_1_data = this_snippet[s_a];
                            if (y_b > y_a) {
                                intersect = (y_1_data < y_b && y_1_data > y_a);
                            } else {
                                intersect = (y_1_data > y_b && y_1_data < y_a);
                            }
                        } else {  // General case: non-vertical hoop
                            float slope = (y_b - y_a) / (t_b - t_a);
                            // Examine every two adjacent samples in the range [s_a, s_b] and determine if they intersect the hoop.
                            for (int s_1 = s_a; s_1 < s_b - 1; ++s_1) {
                                int s_2 = s_1 + 1;
                                float y_1_data = this_snippet[s_1];
                                float y_2_data = this_snippet[s_2];

                                // Convert s_1 and s_2 to the float t_1 and t_2 domain.
                                float t_1 = ((float) s_1) * sample_period;
                                float t_2 = ((float) s_2) * sample_period;

                                float y_1_hoop = y_a + slope * (t_1 - t_a);
                                float y_2_hoop = y_a + slope * (t_2 - t_a);

                                // If the data transitions from below to above the hoop (or vice versa), then an intersection
                                // occurred. Break the loop for checking this hoop.
                                if ((y_1_data >= y_1_hoop && y_2_data <= y_2_hoop) ||
                                        (y_1_data <= y_1_hoop && y_2_data >= y_2_hoop)) {
                                    intersect = true;
                                    break;
                                }

                                // Otherwise, keep looking over the course of this hoop.
                            }

                        }

                        if (intersect) {
                            // If intersect occurred, mark this hoop as jumped through.
                            detection.units[unit].hoops[hoop] = true;
                        } else {
                            // If not, exit the hoop loop (default value is false, so effectively setting it false)
                            // and move on to the next unit.
                            break;
                        }

                    } // End loop across all hoops.

                } // End loop across all units.

            } else {
                // If use_hoops is false, then just populate detection.units[unit].hoops[hoop] with true.
                for (int unit = 0; unit < 4; ++unit) {
                    for (int hoop = 0; hoop < 4; ++hoop) {
                        detection.units[unit].hoops[hoop] = true;
                    }
                }
            }

            uchar ID = 0;
            /* Determine correct ID */

            if (detection.max_surpassed) {
                // If max has been detected, ID is 128 for max surpassing.
                ID = 128;
            } else if (!use_hoops) {
                // If use_hoops is false, ID is 1 to signify threshold crossing.
                ID = 1;
            } else {
                // If use_hoops is true, ID is either (a) an active unit or (b) just a threshold crossing.
                // (a) If a unit is active, ID is either 1, 2, 4, or 8 for the unit.
                for (ushort unit = 0; unit < 4; ++unit) {
                    if (detection.units[unit].hoops[0] && detection.units[unit].hoops[1] &&
                            detection.units[unit].hoops[2] && detection.units[unit].hoops[3]) {
                        ID = (ushort) pow(2.0f, (float) unit);
                        break;
                    }
                }

                // (b) If no unit is active, ID is 64 to signify threshold crossing.
                if (ID == 0) ID = 64;
            }

            /* Populate spike with timestamp */
            // Extract the timestamp of the first frame in this data block.
            uint timestamp_LSW = raw_block[4]; // Timestamp is always the bytes 8-11 of the datablock (16-bit words 4-5).
            uint timestamp_MSW = raw_block[5];
            uint timestamp = (timestamp_MSW << 16) + timestamp_LSW;

            // Add thresh_s to this timestamp to index right (for positive thresh_s) or left (for negative thresh_s).
            timestamp += thresh_s;

            // Write spike detection at this timestamp.
            spike[snippet_index * get_global_size(0) + channel_index] = timestamp;

            // Populate spikeID with correct ID.
            spikeIDs[snippet_index * get_global_size(0) + channel_index] = ID;

            // Advance by snippet_size samples since activity up until then will already be flagged as a spike.
            thresh_s += snippet_size;

            // Continue detection, preparing for another spike in this block to take the next snippet_index.
            ++snippet_index;

            // If the end of this spike snippet is encroaching on the territory of the next data block
            // (with snippet_size of the next block's start), populate start_search_pos[channel] with
            // the end position of this snippet. This allows the next block to start at a later sample,
            // so there's no risk of double-counting a spike.
            if (thresh_s > samples_per_block - snippet_size) {
                start_search_pos[channel_index] = thresh_s - (samples_per_block - snippet_size);
            }
        }
    }

    for (s = 0; s < samples_per_block; ++s) {
        // Boundary check to make sure result will fit in a uint16_t.

        if (wide_float[s] > 6389.0f) wide_float[s] = 6389.0f;
        else if (wide_float[s] < -6389.0f) wide_float[s] = -6389.0f;

        if (filtered_low[s] > 6389.0f) filtered_low[s] = 6389.0f;
        else if (filtered_low[s] < -6389.0f) filtered_low[s] = -6389.0f;

        if (filtered_high[s] > 6389.0f) filtered_high[s] = 6389.0f;
        else if (filtered_high[s] < -6389.0f) filtered_high[s] = -6389.0f;

        // (4) Convert outputs to uint16_t.
        out_index = s * get_global_size(0) + channel_index;

        //  = round((low_float[s] / 0.195f) + 32768) with optimizations for speed
        low[out_index] = (ushort)(fma(filtered_low[s], 5.1282f, 32768.5f)); // TEMP disable low to use for debugging
        wide[out_index] = (ushort)(fma(wide_float[s], 5.1282f, 32768.5f));
        high[out_index] = (ushort)(fma(filtered_high[s], 5.1282f, 32768.5f));
    }


    // Update 'prev_last_2' array with this block's samples.
    for (int filter_index = 0; filter_index < 4; ++filter_index) {
        prev_last_2[low_2nd_to_last_index[filter_index]] = low_float[filter_index][samples_per_block - 2];
        prev_last_2[low_last_index[filter_index]] = low_float[filter_index][samples_per_block - 1];
        prev_last_2[high_2nd_to_last_index[filter_index]] = high_float[filter_index][samples_per_block - 2];
        prev_last_2[high_last_index[filter_index]] = high_float[filter_index][samples_per_block - 1];
    }

    prev_last_2[in_2nd_to_last_index] = in_float[samples_per_block - 2];
    prev_last_2[in_last_index] = in_float[samples_per_block - 1];

    prev_last_2[wide_2nd_to_last_index] = wide_float[samples_per_block - 2];
    prev_last_2[wide_last_index] = wide_float[samples_per_block - 1];
}
