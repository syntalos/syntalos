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
    QAudioDevice defaultDevice(QMediaDevices::defaultAudioOutput());
    mFormat = defaultDevice.preferredFormat();
    mFormat.setChannelConfig(QAudioFormat::ChannelConfigMono);
    mFormat.setSampleFormat(QAudioFormat::Float);
    numSoundBytes = NumSoundSamples * mFormat.bytesPerSample();


    // Initialize variables.
    currentValue = 0.0F;
    nextValue = 0.0F;
    interpRatio = 0.0;
    interpLength = 0;
    dataRatio = sampleRate / mFormat.sampleRate();
    rawBlockSampleSize = ceil(dataRatio * NumSoundSamples);

    // Initialize raw data (microVolts) array.
    rawDatauV = new float[rawBlockSampleSize];
    for (int i = 0; i < rawBlockSampleSize; ++i) {
        rawDatauV[i] = 0.0F;
    }

    // Initialize interp float array.
    interpFloats = new float[NumSoundSamples];
    for (int i = 0; i < NumSoundSamples; ++i) {
        interpFloats[i] = 0.0F;
    }

    // Initialize buffer.
    finalSoundBytesBuffer.resize(numSoundBytes * mFormat.channelCount());
    for (int i = 0; i < numSoundBytes; ++i) {
        finalSoundBytesBuffer[i] = 0;
    }

    // Create audio IO and output device.
    mAudioSink = std::make_unique<QAudioSink>(defaultDevice, mFormat);
    connect(mAudioSink.get(), SIGNAL(stateChanged(QAudio::State)), this, SLOT(catchError()));
    qreal linearVolume = QAudio::convertVolume(volume / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
    mAudioSink->setVolume(linearVolume);
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

                if (!state->running) {
                    qApp->processEvents();
                    continue;
                }

                // Start audio (if it's not started already)
                if (mAudioSink->state() != QAudio::ActiveState) {
                    mAudioSink->start(&m_buffer);
                }

                // Wait for samples to arrive from WaveformFifo (enough where, when scaled to audio sample rate, NumSoundSamples can be written)
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

           delete [] rawDatauV;
           delete [] interpFloats;

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
    // Out of an abundance of caution, bound these values read from state in case of any glitches due to threading issues.
    if (volume != state->audioVolume->getValue()) {
        volume = qBound(minVolume, state->audioVolume->getValue(), maxVolume);
        qreal linearVolume = QAudio::convertVolume(volume / qreal(100), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        mAudioSink->setVolume(linearVolume);
    }
    if (threshold != state->audioThreshold->getValue()) {
        threshold = qBound(minThreshold, state->audioThreshold->getValue(), maxThreshold);
    }

    // Fill rawDatauV with samples from waveformFifo, based on single selected channel.
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
            rawDatauV[i] = waveformFifo->getGpuAmplifierData(WaveformFifo::ReaderAudio, waveformAddress, i);
        }
    } else {
        newChannelString = "";
        for (int i = 0; i < rawBlockSampleSize; ++i) {
            rawDatauV[i] = 0.0F;
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
        nextValue = rawDatauV[originalSamplesCopied];

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

    float myMin = 0.0f;
    float myMax = 0.0f;
    for (int i = 0; i < soundSamplesCopied; ++i) {
        if (interpFloats[i] > myMax) {
            myMax = interpFloats[i];
        }
        if (interpFloats[i] < myMin) {
            myMin = interpFloats[i];
        }
    }

    // Scale from -ClipLevel:+ClipLevel to -1.0:1.0
    // Default: ClipLevel = 6000.0;
    for (int i = 0; i < soundSamplesCopied; ++i) {
        interpFloats[i] = interpFloats[i] / ClipLevel;

        // Clamp any values outside this range to max/min values
        if (interpFloats[i] > 1.0) {
            interpFloats[i] = 1.0;
        } else if (interpFloats[i] < -1.0) {
            interpFloats[i] = -1.0;
        }

        // Additional volume scaling (+-1 to +-VolumeBoost)
        // Default: VolumeBoost = 10.0;
        interpFloats[i] = interpFloats[i] * VolumeBoost;
    }

    // Convert to correct format for audio output
    // Adapted from Qt example "audiooutput", version 6.8.2
    char *ptr = finalSoundBytesBuffer.data();
    for (int i = 0; i < soundSamplesCopied; ++i) {
        for (int j = 0; j < mFormat.channelCount(); ++j) {
            switch (mFormat.sampleFormat()) {
            case QAudioFormat::UInt8:
                qToLittleEndian<quint8>((1.0 + interpFloats[i] / 2 * 255), ptr);
                ptr += sizeof(quint8);
                break;
            case QAudioFormat::Int16:
                qToLittleEndian<qint16>(interpFloats[i] * 32767);
                ptr += sizeof(qint16);
                break;
            case QAudioFormat::Int32:
                qToLittleEndian<int32_t>(interpFloats[i] * std::numeric_limits<qint32>::max());
                ptr += sizeof(qint32);
                break;
            case QAudioFormat::Float:
                qToLittleEndian<float>(interpFloats[i], ptr);
                ptr += sizeof(float);
                break;
            default:
                break;
            }
        }
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
