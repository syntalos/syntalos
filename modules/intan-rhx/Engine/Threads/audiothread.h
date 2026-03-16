//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.0
//
//  Copyright (c) 2020-2026 Intan Technologies
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
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef AUDIOTHREAD_H
#define AUDIOTHREAD_H

#include <QThread>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <cstdint>
#include <mutex>
#include "systemstate.h"
#include "waveformfifo.h"

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
    const double ClipLevel = 600.0;
    const double VolumeBoost = 10.0;

    const int NumSoundSamples = 8000;
    int numSoundBytes;

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

    float *rawDatauV;
    float *interpFloats;
    QByteArray finalSoundBytesBuffer;

    double dataRatio;

    int originalSamplesCopied;
    int soundSamplesCopied;

    std::atomic_bool keepGoing;
    std::atomic_bool running;
    std::atomic_bool stopThread;

    float currentValue;
    float nextValue;
    double interpRatio;
    int interpLength;

    QAudioFormat mFormat;
    std::unique_ptr<QAudioSink> mAudioSink;
    QDataStream* s;

    QString currentChannelString;

    void initialize();
    bool fillBufferFromWaveformFifo();
    void processAudioData();
};

#endif // AUDIOTHREAD_H
