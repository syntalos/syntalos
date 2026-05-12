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

#include "HeadstageONI.h"

HeadstageONI::HeadstageONI(Rhd2000ONIBoard::BoardDataSource dataSource_, int MAX_H)
    : Headstage(int(dataSource_)),
      dataSource(dataSource_),
      streamIndex(-1),
      numStreams(0),
      channelsPerStream(32),
      halfChannels(false),
      m_hasBno(false),
      MAX_NUM_HEADSTAGES(MAX_H),
      m_log(Syntalos::getLogger("oeacq.headstage-oni"))
{
}

int HeadstageONI::getNumStreams() const
{
    return numStreams;
}

void HeadstageONI::setNumStreams(int num)
{
    LOG_DEBUG(m_log, "Headstage {} setting num streams to {}", prefix, num);

    if (num == 2)
        halfChannels = false;

    if (numStreams != num) {
        numStreams = num;
        generateChannelNames(channelNamingScheme);
    }
}

void HeadstageONI::setHasBno(bool hasBno)
{
    m_hasBno = hasBno;
}

void HeadstageONI::setChannelsPerStream(int nchan)
{
    LOG_DEBUG(m_log, "Headstage {} setting channels per stream to {}", prefix, nchan);

    if (channelsPerStream != nchan) {
        channelsPerStream = nchan;
        generateChannelNames(channelNamingScheme);
    }
}

void HeadstageONI::setFirstStreamIndex(int streamIndex_)
{
    streamIndex = streamIndex_;
}

int HeadstageONI::getStreamIndex(int offset) const
{
    return streamIndex + offset;
}

int HeadstageONI::getNumChannels() const
{
    return channelsPerStream * numStreams;
}

void HeadstageONI::setHalfChannels(bool half)
{
    if (getNumChannels() == 64)
        return;

    if (halfChannels != half) {
        halfChannels = half;
        generateChannelNames(channelNamingScheme);
    }
}

int HeadstageONI::getNumActiveChannels() const
{
    return getNumChannels() / (halfChannels ? 2 : 1);
}

Rhd2000ONIBoard::BoardDataSource HeadstageONI::getDataStream(int index) const
{
    if (index < 0 || index > 1)
        index = 0;
    return static_cast<Rhd2000ONIBoard::BoardDataSource>(dataSource + MAX_NUM_HEADSTAGES * index);
}

bool HeadstageONI::isConnected() const
{
    return numStreams > 0 || m_hasBno;
}

std::string HeadstageONI::getChannelName(int ch) const
{
    std::string name;

    if (ch > -1 && ch < static_cast<int>(channelNames.size()))
        name = channelNames[ch];
    else
        name = " ";

    if (ch == 0)
        LOG_DEBUG(m_log, "Headstage {} channel {} name: {}", prefix, ch, name);

    return name;
}

void HeadstageONI::generateChannelNames(ChannelNamingScheme scheme)
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

bool HeadstageONI::hasValidImpedance(int ch) const
{
    return ch >= 0 && ch < static_cast<int>(impedanceMagnitudes.size());
}

void HeadstageONI::setImpedances(const Impedances &impedances)
{
    impedanceMagnitudes.clear();
    impedancePhases.clear();

    for (size_t i = 0; i < impedances.streams.size(); ++i) {
        if (impedances.streams[i] == streamIndex) {
            impedanceMagnitudes.push_back(impedances.magnitudes[i]);
            impedancePhases.push_back(impedances.phases[i]);
        }
        if (numStreams == 2 && impedances.streams[i] == streamIndex + 1) {
            impedanceMagnitudes.push_back(impedances.magnitudes[i]);
            impedancePhases.push_back(impedances.phases[i]);
        }
    }
}

float HeadstageONI::getImpedanceMagnitude(int channel) const
{
    if (channel >= 0 && channel < static_cast<int>(impedanceMagnitudes.size()))
        return impedanceMagnitudes[channel];
    return 0.0f;
}

float HeadstageONI::getImpedancePhase(int channel) const
{
    if (channel >= 0 && channel < static_cast<int>(impedancePhases.size()))
        return impedancePhases[channel];
    return 0.0f;
}
