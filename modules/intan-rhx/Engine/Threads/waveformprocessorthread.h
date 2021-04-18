//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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
#ifndef WAVEFORMPROCESSORTHREAD_H
#define WAVEFORMPROCESSORTHREAD_H

#include <QObject>
#include <QThread>
#include <vector>
#include "datastreamfifo.h"
#include "waveformfifo.h"
#include "systemstate.h"
#include "xpucontroller.h"

using namespace std;

class WaveformProcessorThread : public QThread
{
    Q_OBJECT
public:
    explicit WaveformProcessorThread(SystemState* state_, int numDataStreams_, double sampleRate_,
                                     DataStreamFifo* usbFifo_, WaveformFifo* waveformFifo_,
                                     XPUController* xpuController_, QObject *parent = nullptr);

    void run() override;
    void startRunning(int numDataStreams_);
    void stopRunning();
    bool isActive() const;
    void close();

signals:
    void cpuLoadPercent(double percent);

private:
    SystemState* state;
    SignalSources* signalSources;
    ControllerType type;
    double sampleRate;
    DataStreamFifo* usbFifo;
    WaveformFifo* waveformFifo;
    int numDataStreams;

    vector<double> cpuLoadHistory;

    XPUController* xpuController;

    volatile bool keepGoing;
    volatile bool running;
    volatile bool stopThread;
};

#endif // WAVEFORMPROCESSORTHREAD_H
