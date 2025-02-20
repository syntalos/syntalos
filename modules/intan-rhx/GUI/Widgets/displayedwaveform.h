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

#ifndef DISPLAYEDWAVEFORM_H
#define DISPLAYEDWAVEFORM_H

#include <QString>
#include <QColor>
#include "signalsources.h"

enum WaveformType {
    AmplifierWaveform,
    AuxInputWaveform,
    SupplyVoltageWaveform,
    BoardAdcWaveform,
    BoardDacWaveform,
    BoardDigInWaveform,
    BoardDigOutWaveform,
    RasterWaveform,
    UnknownWaveform,
    WaveformDivider
};

const int DividerSectionID = 99999;    // Must be larger than all other section IDs.

struct DisplayedWaveform
{
public:
    DisplayedWaveform(const QString& waveName_, WaveformType waveformType_, Channel* channel_) :
        waveName(waveName_),
        waveformType(waveformType_),
        channel(channel_),
        spacingAbove(3),
        spacingBelow(3),
        yCoord(-1),
        yTop(-1),
        yBottom(-1),
        yTopLimit(-1),
        yBottomLimit(-1),
        sectionID(-1),
        isCurrentlyVisible(false)
    {}

    QString waveName;
    WaveformType waveformType;
    Channel* channel;
    int spacingAbove;
    int spacingBelow;
    int yCoord;
    int yTop;
    int yBottom;
    int yTopLimit;
    int yBottomLimit;
    int sectionID;   // DividerId for dividers, otherwise unique number common to all waveforms in one division
    bool isCurrentlyVisible;

    bool isDivider() const { return waveformType == WaveformDivider; }
    bool isAmplifier() const { return waveformType == AmplifierWaveform || waveformType == RasterWaveform; }
    bool isSelected() const { if (channel) { return channel->isSelected(); } else return false; }
    bool isEnabled() const { if (channel) { return channel->isEnabled(); } else return true; }  // Default to true for dividers.
    void setEnabled(bool enable) { if (channel) { channel->setEnabled(enable); } }
    QColor getColor() const { if (channel) { return channel->getColor(); } else return QColor(255, 255, 255); }
    int getGroupID() const { if (channel) { return channel->getGroupID(); } else return 0; }
    QString waveNameWithoutFilter() const { return waveName.section('|', 0, 0); }

    static WaveformType translateSignalTypeToWaveformType(SignalType signalType);
    static bool positiveOnlyType(WaveformType waveformType);
};

#endif // DISPLAYEDWAVEFORM_H
