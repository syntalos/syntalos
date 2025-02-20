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

#include <QMediaDevices>
#include "signalsources.h"
#include "audiothread.h"

AudioThread::AudioThread(SystemState *state_, WaveformFifo *waveformFifo_, const double sampleRate_, QObject *parent) :
    QThread(parent),
    state(state_),
    waveformFifo(waveformFifo_),
    volume(0),
    threshold(0),
    minVolume(state_->audioVolume->getMinValue()),
    maxVolume(state->audioVolume->getMaxValue()),
    minThreshold(state->audioThreshold->getMinValue()),
    maxThreshold(state->audioThreshold->getMaxValue()),
    sampleRate(sampleRate_),
    keepGoing(false),
    running(false),
    stopThread(false)
{
}

void AudioThread::initialize()
{
    // Initialize variables.
    currentValue = 0.0F;
    nextValue = 0.0F;
    interpRatio = 0.0;
    interpLength = 0;
    dataRatio = sampleRate / 44100.0;
    rawBlockSampleSize = ceil(dataRatio * NumSoundSamples);

    // Initialize raw data array.
    rawData = new float[rawBlockSampleSize];
    for (int i = 0; i < rawBlockSampleSize; ++i) {
        rawData[i] = 0.0F;
    }

    // Initialize interp float array.
    interpFloats = new float[NumSoundSamples];
    for (int i = 0; i < NumSoundSamples; ++i) {
        interpFloats[i] = 0.0F;
    }

    // Initialize interp data array.
    interpInts = new int32_t[NumSoundSamples];
    for (int i = 0; i < NumSoundSamples; ++i) {
        interpInts[i] = 0;
    }

    // Initialize buffer.
    finalSoundBytesBuffer.resize(NumSoundBytes);
    for (int i = 0; i < NumSoundBytes; ++i) {
        finalSoundBytesBuffer[i] = 0;
    }

    // Set up audio format.
    mFormat.setSampleRate(44100);
    mFormat.setChannelCount(1);
    mFormat.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice defaultDevice(QMediaDevices::defaultAudioOutput());
    if (!defaultDevice.isFormatSupported(mFormat)) {
        qWarning() << "Default format not supported - trying to use preferred";
        mFormat = defaultDevice.preferredFormat();
        mFormat.setChannelCount(1);
    }

    // Create audio IO and output device.
    mAudioSink = std::make_unique<QAudioSink>(defaultDevice, mFormat);
    mAudioSink->setBufferSize(NumSoundBytes);
    connect(mAudioSink.get(), SIGNAL(stateChanged(QAudio::State)), this, SLOT(catchError()));
}

void AudioThread::run()
{
    while (!stopThread) {
        if (keepGoing) {
            running = true;

            // Any 'start up' code goes here.
            initialize();
            QBuffer m_buffer;
            m_buffer.setBuffer(&finalSoundBytesBuffer);
            m_buffer.open(QIODevice::ReadOnly);

            while (keepGoing && !stopThread) {

                // Start audio (if it's not started already)
                if (mAudioSink->state() != QAudio::ActiveState) {
                    mAudioSink->start(&m_buffer);
                }

                // Wait for samples to arrive from WaveformFifo (enough where, when scaled to 44.1 kHz audio, NumSoundSamples can be written)
                if (waveformFifo->requestReadNewData(WaveformFifo::ReaderAudio, rawBlockSampleSize)) {
                    m_buffer.seek(0);

                    // Populate rawData from WaveformFifo
                    if (!fillBufferFromWaveformFifo()) {
                        qDebug() << "Error with fillBufferFromWaveformFifo()";
                    }
                    waveformFifo->freeOldData(WaveformFifo::ReaderAudio);

                    // Populate finalSoundBytesBuffer from rawData
                    processAudioData();

                } else {
                    // Probably could sleep here for a while
                    qApp->processEvents();
                }
           }

           // Any 'finish up' code goes here.
           mAudioSink->stop();

           delete [] rawData;
           delete [] interpFloats;
           delete [] interpInts;

           running = false;
        } else {
            usleep(1000);
        }
    }
}

void AudioThread::startRunning()
{
    keepGoing = true;
}

void AudioThread::stopRunning()
{
    keepGoing = false;
}

void AudioThread::close()
{
    keepGoing = false;
    stopThread = true;
}

bool AudioThread::fillBufferFromWaveformFifo()
{
    // Update volume and noise slicer threshold values.
    volume = state->audioVolume->getValue();
    threshold = state->audioThreshold->getValue();

    // Out of an abundance of caution, bound these values read from state in case of any glitches due to threading issues.
    volume = qBound(minVolume, volume, maxVolume);
    threshold = qBound(minThreshold, threshold, maxThreshold);

    // Fill rawData with samples from waveformFifo, based on single selected channel.
    bool validAudioSource = true;
    QString selectedChannelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (selectedChannelName.isEmpty()) validAudioSource = false;
    QString selectedChannelFilterName = selectedChannelName + "|" + state->audioFilter->getDisplayValueString();
    if (!waveformFifo->gpuWaveformPresent(selectedChannelFilterName.toStdString())) {
        qDebug() << "Failure... channel name: " << selectedChannelFilterName;
        validAudioSource = false;
    }
    GpuWaveformAddress waveformAddress = waveformFifo->getGpuWaveformAddress(selectedChannelFilterName.toStdString());
    if (waveformAddress.waveformIndex < 0) validAudioSource = false;

    QString newChannelString;
    if (validAudioSource) {
        newChannelString = selectedChannelFilterName;
        for (int i = 0; i < rawBlockSampleSize; ++i) {
            rawData[i] = waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderAudio, waveformAddress, i);
        }
    } else {
        newChannelString = "";
        for (int i = 0; i < rawBlockSampleSize; ++i) {
            rawData[i] = 0.0F;
        }
    }
    if (newChannelString != currentChannelString) {
        emit newChannel(newChannelString);
        currentChannelString = newChannelString;
    }

    return validAudioSource;
}

void AudioThread::processAudioData()
{
    // Do floating point interpolation.
    originalSamplesCopied = 0;
    soundSamplesCopied = 0;
    while (originalSamplesCopied < rawBlockSampleSize) {
        currentValue = nextValue;
        nextValue = rawData[originalSamplesCopied];

        while (interpRatio < 1.0) {
            // Ensure that no writing beyond allocated memory for interpFloats occurs
            if (soundSamplesCopied >= NumSoundSamples) {
                break;
            }
            interpFloats[soundSamplesCopied++] = currentValue + interpRatio * (nextValue - currentValue);
            interpRatio += dataRatio;
        }

        interpRatio = interpRatio - floor(interpRatio);
        ++originalSamplesCopied;
    }

    // Extra while loop that will execute occasionally to make sure no samples are dropped due to remainder in interp ratio
    while (soundSamplesCopied < NumSoundSamples) {
        interpFloats[soundSamplesCopied] = interpFloats[soundSamplesCopied - 1];
        ++soundSamplesCopied;
    }

    // Do thresholding.
    for (int i = 0; i < soundSamplesCopied; ++i) {
        if (interpFloats[i] > threshold) {
            interpFloats[i] -= threshold;
        } else if (interpFloats[i] < -threshold) {
            interpFloats[i] += threshold;
        } else {
            interpFloats[i] = 0.0F;
        }
    }

    // Do audio volume, and numerically convert to int.
    for (int i = 0; i < soundSamplesCopied; ++i) {
        interpFloats[i] = (volume / 2) * (interpFloats[i] / 0.195F);
    }

    // As float, trim to max of +- 32767.
    for (int i = 0; i < soundSamplesCopied; ++i) {
        interpFloats[i] = qBound(-32768.0F, interpFloats[i], 32767.0F);
    }

    // Convert to int.
    for (int i = 0; i < soundSamplesCopied; ++i) {
        interpInts[i] = (int32_t) round(interpFloats[i]);
    }

    // Fill buffer with final data.
    char *ptr = finalSoundBytesBuffer.data();
    for (int i = 0; i < NumSoundSamples; ++i) {
        qToLittleEndian<int16_t>(interpInts[i], ptr);
        ptr += 2;
    }
}

void AudioThread::catchError()
{
    QAudio::Error errorValue = mAudioSink->error();
    switch (errorValue) {
    case QAudio::NoError:
        break;
    case QAudio::OpenError:
        qDebug().noquote() << "rhx-audio: Open Error";
        break;
    case QAudio::IOError:
        qDebug().noquote() << "rhx-audio: IO Error";
        break;
    case QAudio::UnderrunError:
        qDebug().noquote() << "rhx-audio: Underrun Error";
        break;
    case QAudio::FatalError:
        qDebug().noquote() << "rhx-audio: Fatal Error";
        break;
    }
}
