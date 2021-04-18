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

#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include <QThread>
#include <QAudioOutput>
#include <cstdint>
#include <mutex>
#include "systemstate.h"
#include "waveformfifo.h"

using namespace std;

class AudioThread : public QThread
{
    Q_OBJECT
public:
    explicit AudioThread(SystemState *state_, WaveformFifo *waveformFifo_, const double sampleRate_, QObject *parent = nullptr);

    void run() override;  // QThread 'run()' method that is called when thread is started
    void startRunning();  // Enter run loop.
    void stopRunning();  // Exit run loop.
    bool isActive() const { return running; }  // Is this thread running?
    void close();  // Close thread.

signals:
    void newChannel(QString name);

private slots:
    void catchError();

private:

    // For some reason, Mac seems to do better with larger audio write periods, and Windows better with smaller write periods.
    // These values don't completely eliminate audio popping, but seem to reduce them considerably, especially after running for a bit.
#if __APPLE__
    const int NumSoundSamples = 24000;
#else
    const int NumSoundSamples = 5000;
#endif
    const int NumSoundBytes = 2 * NumSoundSamples;

    SystemState* state;
    WaveformFifo* waveformFifo;

    int volume;
    int threshold;

    int minVolume;
    int maxVolume;
    int minThreshold;
    int maxThreshold;

    double sampleRate;

    int rawBlockSampleSize;

    float *rawData;
    float *interpFloats;
    int32_t *interpInts;
    char* finalSoundBytesBuffer;

    double dataRatio;

    int originalSamplesCopied;
    int soundSamplesCopied;

    volatile bool keepGoing;
    volatile bool running;
    volatile bool stopThread;

    float currentValue;
    float nextValue;
    double interpRatio;
    int interpLength;

    QAudioDeviceInfo mDevice;
    QByteArray* buf;
    QDataStream* s;
    QAudioOutput* mAudioOutput;
    QAudioFormat mFormat;

    QString currentChannelString;

    void initialize();
    bool fillBufferFromWaveformFifo();
    void processAudioData();
};

#endif // AUDIOTHREAD_H
