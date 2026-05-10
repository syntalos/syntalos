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

#include "../Headstage.h"

#include <string>

class HeadstageSim : public Headstage
{
public:
    explicit HeadstageSim(int port_index);
    ~HeadstageSim() override = default;

    int getNumActiveChannels() const override;
    std::string getChannelName(int ch) const override;

    bool hasValidImpedance(int ch) const override;
    float getImpedanceMagnitude(int ch) const override;
    float getImpedancePhase(int ch) const override;

    void generateChannelNames(ChannelNamingScheme scheme) override;

    bool isConnected() const override
    {
        return numChannels > 0;
    }

    void setChannelCount(int numChannels_);

private:
    int numChannels;
};
