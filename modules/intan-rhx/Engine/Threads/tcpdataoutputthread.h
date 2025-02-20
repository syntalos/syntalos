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

#ifndef TCPDATAOUTPUTTHREAD_H
#define TCPDATAOUTPUTTHREAD_H

#include <QThread>
#include <stdint.h>

#include "systemstate.h"
#include "waveformfifo.h"
#include "tcpcommunicator.h"

class TCPDataOutputThread : public QThread
{
    Q_OBJECT
public:
    explicit TCPDataOutputThread(WaveformFifo *waveformFifo_, const double sampleRate_, SystemState *state_, QObject *parent = nullptr);
    ~TCPDataOutputThread();

    void run() override; // QThread 'run()' method that is called when thread is started
    void startRunning(); // Enter run loop.
    void stopRunning(); // Exit run loop.
    bool isActive() const; // Is this thread running?
    void closeExternal(); // Close thread from outside this thread.

    void prepareToClose();
    bool isReadyToClose();

signals:
    void outputData(QByteArray *array, qint64 len);

private:
    void closeInternal(); // Close thread from inside this thread.
    void updateEnabledChannels();

    TCPCommunicator *tcpWaveformDataCommunicator;
    TCPCommunicator *tcpSpikeDataCommunicator;

    std::vector<std::string> channelNames;
    QVector<QString> enabledChannelNames;
    QVector<QString> enabledStimChannelNames;

    QStringList previousEnabledBands;

    int totalEnabledBands;
    int numAuxChannels;
    int numVddChannels;
    int numAdcChannels;
    int numDacChannels;
    int numDigitalInChannels;
    int numDigitalOutChannels;

    uint16_t* previousSample;

    int digInWordPresent;
    int digOutWordPresent;
    int numBytesPerFrame;
    int numBytesPerDataBlock;

    QByteArray waveformArray;
    qint64 waveformArrayIndex;

    QByteArray spikeArray;
    qint64 spikeArrayIndex;

    int numBytesPerSpikeChunk;
    int maxChunksPerDataBlock;

    WaveformFifo *waveformFifo;

    SignalSources *signalSources;

    double sampleRate;

    std::vector<uint8_t> posStimAmplitudes;
    std::vector<uint8_t> negStimAmplitudes;

    bool closeRequested;
    bool closeCompleted;

    volatile bool keepGoing;
    volatile bool running;
    volatile bool stopThread;

    QObject *parentObject;

    bool connected;

    SystemState* state;
};

#endif // TCPDATAOUTPUTTHREAD_H
