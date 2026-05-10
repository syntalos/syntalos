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
#include "../ImpedanceMeter.h"

#include "rhythm-api/rhd2000ONIboard.h"

#include <string>
#include <vector>

#include "fabric/logging.h"

class HeadstageONI : public Headstage
{
public:
    HeadstageONI(Rhd2000ONIBoard::BoardDataSource dataSource, int MAX_NUM_HEADSTAGES);
    ~HeadstageONI() override = default;

    int getNumActiveChannels() const override;
    std::string getChannelName(int ch) const override;

    bool hasValidImpedance(int ch) const override;
    float getImpedanceMagnitude(int ch) const override;
    float getImpedancePhase(int ch) const override;

    void generateChannelNames(ChannelNamingScheme scheme) override;

    /** Index of this headstage's data stream (offset = 0 or 1). */
    int getStreamIndex(int offset) const;

    /** Set the index of this headstage's data stream. */
    void setFirstStreamIndex(int streamIndex);

    /** Set the number of channels per stream. */
    void setChannelsPerStream(int nchan);

    /** Number of channels the headstage supports (does not account for half channels). */
    int getNumChannels() const;

    /** Number of streams this headstage sends. */
    int getNumStreams() const;

    /** Set the number of streams (1 or 2). */
    void setNumStreams(int num);

    /** Whether a BNO IMU is present on this headstage. */
    void setHasBno(bool hasBno);

    bool isConnected() const override;

    Rhd2000ONIBoard::BoardDataSource getDataStream(int index) const;

    /** Number of half-channels; mainly used for the 16-ch RHD2132 board. */
    void setHalfChannels(bool half);
    bool getHalfChannels() const
    {
        return halfChannels;
    }

    /** Apply impedance results for this headstage. */
    void setImpedances(const Impedances &impedances);

private:
    Rhd2000ONIBoard::BoardDataSource dataSource;
    int streamIndex;
    int numStreams;
    int channelsPerStream;
    bool halfChannels;
    bool m_hasBno;
    int MAX_NUM_HEADSTAGES;

    Syntalos::QuillLogger *m_log;

    std::vector<float> impedanceMagnitudes;
    std::vector<float> impedancePhases;
};
