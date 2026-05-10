/*
    ------------------------------------------------------------------
    Copyright (C) 2024 Open Ephys
    Copyright (C) 2026 Syntalos Project

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <array>
#include <string>
#include <vector>

enum ChannelNamingScheme {
    GLOBAL_INDEX = 1,
    STREAM_INDEX = 2
};

/**
 * A headstage object represents a data source containing one or more Intan
 * chips. Each headstage can send 1 or 2 data streams, each with up to 64
 * channels.
 *
 * A headstage can be identified in the following ways:
 * - dataSource   : port (A1-D2Ddr) that the headstage is connected to
 * - streamIndex  : location in the array of active streams
 * - startChannel : index of the first channel acquired by this headstage,
 *                  out of all actively acquired neural data channels
 */
class Headstage
{
public:
    explicit Headstage(int port_index)
        : prefix(headstage_names[port_index])
    {
    }

    virtual ~Headstage() = default;

    Headstage(const Headstage &) = delete;
    Headstage &operator=(const Headstage &) = delete;

    /** Number of channels this headstage sends. */
    virtual int getNumActiveChannels() const = 0;

    /** Name of a channel at a given index. */
    virtual std::string getChannelName(int ch) const = 0;

    /** True if impedances for a headstage channel have been measured. */
    virtual bool hasValidImpedance(int ch) const = 0;

    /** Impedance magnitude (Ohms) for a given channel. */
    virtual float getImpedanceMagnitude(int ch) const = 0;

    /** Impedance phase (degrees) for a given channel. */
    virtual float getImpedancePhase(int ch) const = 0;

    /** Generate names for each channel according to the scheme. */
    virtual void generateChannelNames(ChannelNamingScheme scheme) = 0;

    /** True if the headstage is connected. */
    virtual bool isConnected() const = 0;

    /** Set the index of this headstage's first neural data channel. */
    void setFirstChannel(int channelIndex)
    {
        firstChannelIndex = channelIndex;
    }

    /** Headstage stream prefix (used for naming AUX channels). */
    const std::string &getStreamPrefix() const
    {
        return prefix;
    }

    void setNamingScheme(ChannelNamingScheme scheme)
    {
        generateChannelNames(scheme);
    }

protected:
    /** Available headstage names (based on ports). */
    static constexpr std::array<const char *, 16> headstage_names{
        "A1", "A2", "B1", "B2", "C1", "C2", "D1", "D2",
        "E1", "E2", "F1", "F2", "G1", "G2", "H1", "H2"};

    /** Prefix for this headstage. */
    const std::string prefix;

    /** Names of individual channels. */
    std::vector<std::string> channelNames;

    /** Naming scheme for channels. */
    ChannelNamingScheme channelNamingScheme = GLOBAL_INDEX;

    /** Index of first channel. */
    int firstChannelIndex = 0;
};
