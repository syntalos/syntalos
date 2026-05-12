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

#include "HeadstageSim.h"

#include <format>

HeadstageSim::HeadstageSim(int port_index)
    : Headstage(port_index),
      numChannels(0)
{
}

int HeadstageSim::getNumActiveChannels() const
{
    return numChannels;
}

std::string HeadstageSim::getChannelName(int ch) const
{
    if (ch >= 0 && ch < static_cast<int>(channelNames.size()))
        return channelNames[ch];
    return std::string(" ");
}

bool HeadstageSim::hasValidImpedance(int /*ch*/) const
{
    return true;
}

float HeadstageSim::getImpedanceMagnitude(int /*ch*/) const
{
    return 1.0f;
}

float HeadstageSim::getImpedancePhase(int /*ch*/) const
{
    return 1.0f;
}

void HeadstageSim::setChannelCount(int numChannels_)
{
    numChannels = numChannels_;
    generateChannelNames(channelNamingScheme);
}

void HeadstageSim::generateChannelNames(ChannelNamingScheme scheme)
{
    channelNamingScheme = scheme;
    channelNames.clear();

    switch (scheme) {
    case GLOBAL_INDEX:
        for (int i = 0; i < getNumActiveChannels(); i++)
            channelNames.push_back(std::format("CH{:02}", firstChannelIndex + i + 1));
        break;
    case STREAM_INDEX:
        for (int i = 0; i < getNumActiveChannels(); i++)
            channelNames.push_back(std::format("{}_CH{:02}", prefix, i + 1));
        break;
    }
}
