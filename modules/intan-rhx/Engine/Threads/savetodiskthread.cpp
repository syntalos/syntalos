//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.3.1
//
//  Copyright (c) 2020-2023 Intan Technologies
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
#include <QElapsedTimer>
#include "abstractrhxcontroller.h"
#include "intanfilesavemanager.h"
#include "filepersignaltypesavemanager.h"
#include "fileperchannelsavemanager.h"
#include "savetodiskthread.h"

SaveToDiskThread::SaveToDiskThread(WaveformFifo* waveformFifo_, SystemState* state_, QObject *parent) :
    QThread(parent),
    waveformFifo(waveformFifo_),
    state(state_),
    saveManager(nullptr)
{
    stopRunASAP = false;
    keepGoing = false;
    running = false;
    stopThread = false;
}

SaveToDiskThread::~SaveToDiskThread()
{
    if (saveManager) {
        delete saveManager;
        saveManager = nullptr;
    }
}

void SaveToDiskThread::run()
{
    const int NumSamples = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    int bytesPerMinute = 0;
    const QString saveFileErrorMessage = "Could not open save file(s). Please check that the provided filename is valid, the provided path location exists, and that the location doesn't require elevated permissions to write to.";

    boardDigitalInWaveform = waveformFifo->getDigitalWaveformPointer("DIGITAL-IN-WORD");
    boardAdcWaveform.resize(AbstractRHXController::numAnalogIO(state->getControllerTypeEnum(), state->expanderConnected->getValue()));
    for (int i = 0; i < (int) boardAdcWaveform.size(); ++i) {
        boardAdcWaveform[i] = waveformFifo->getAnalogWaveformPointer("ANALOG-IN-" +
                                                AbstractRHXController::getAnalogIOChannelNumber(state->getControllerTypeEnum(), i));
    }

    while (!stopThread) {
        digitalTrigger = state->triggerSource->getValue().left(3).toUpper() == "DIG";
        triggerChannel = (int) state->triggerSource->getNumericValue();
        triggerOnHigh = state->triggerPolarity->getValue() == "High";
        analogTriggerThreshold = state->triggerAnalogVoltageThreshold->getValue();

        bool isRecording = false;

        totalRecordedSamples = 0;
        int64_t totalSamplesInFile = 0;
        int64_t totalBytesWritten = 0;

        int64_t blocksWritten = 0;

        double glitchIgnoreInSeconds = 0.2;     // time period following trigger in which glitches in trigger are ignored
        int glitchThreshold = ceil(glitchIgnoreInSeconds * state->sampleRate->getNumericValue());

//        QElapsedTimer loopTimer, workTimer, reportTimer;
        QElapsedTimer statusBarUpdateTimer;

        if (keepGoing) {
            running = true;
            int triggerBeginCounter = 0;    // used to ignore glitches shortly after trigger is activated
            int triggerEndCounter = 0;      // used to time postTriggerBuffer
            int triggerEndSamples = ceil(state->postTriggerBuffer->getValue() * state->sampleRate->getNumericValue());

//            loopTimer.start();
//            workTimer.start();
//            reportTimer.start();
            statusBarUpdateTimer.start();
            while (keepGoing && !stopThread) {
//                workTimer.restart();
                int64_t playbackBlocks = state->getPlaybackBlocks();
                bool lastRead = (playbackBlocks - blocksWritten) == 1;
                //bool lastRead = true;

                if (!isRecording && state->recording) {     // Manual start recording.
//                    cout << "MANUAL START RECORD" << EndOfLine;
                    if (!saveManager->openAllSaveFiles()) {
                        emit error(saveFileErrorMessage);
                        emit sendSetCommand("RunMode", "Stop");
                        close();
                        break;
                    } else {
                        isRecording = true;
                        totalRecordedSamples = 0;
                        totalSamplesInFile = 0;
                        totalBytesWritten = 0;
                        bytesPerMinute = saveManager->bytesPerMinute();
                    }
                }

                //qDebug() << "Here. playbackBlocks: " << playbackBlocks << " total data blocks written: " << blocksWritten << " lastRead: " << lastRead;
                if (waveformFifo->requestReadNewData(WaveformFifo::ReaderDisk, NumSamples, lastRead)) {
                    blocksWritten++;
                    if (state->triggerSet && !state->triggered) {

                        // Watch for trigger begin event.
                        setStatusBarWaitForTrigger();
                        int triggerTimeIndex = findTrigger(NumSamples, FindTriggerBegin);
                        bool triggerBeginFound = triggerTimeIndex >= 0;

                        if (triggerBeginFound) {    // Triggered start recording.
//                            cout << "TRIGGER BEGIN FOUND" << endl;
                            state->triggered = true;
                            state->triggerSet = false;
                            state->recording = true;
                            if (!saveManager->openAllSaveFiles()) {
                                emit error(saveFileErrorMessage);
                                emit sendSetCommand("RunMode", "Stop");
                                close();
                                break;
                            } else {
                                isRecording = true;
                                triggerBeginCounter = 0;
                                totalRecordedSamples = 0;
                                totalSamplesInFile = 0;
                                totalBytesWritten = 0;
                                bytesPerMinute = saveManager->bytesPerMinute();

                                saveManager->setTimeStampOffset(waveformFifo->getTimeStamp(WaveformFifo::ReaderDisk, triggerTimeIndex));
                                int preTriggerBufferSamples =
                                        ceil(((double) state->preTriggerBuffer->getValue()) *
                                             state->sampleRate->getNumericValue());
                                int preTriggerIndex = triggerTimeIndex - preTriggerBufferSamples;

                                if (saveManager->mustSaveCompleteDataBlocks()) {
                                    // Round down to nearest data block boundary.
                                    int numWholeDataBlocks = floor((double)preTriggerIndex /
                                                                   (double)RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum()));
                                    preTriggerIndex = numWholeDataBlocks * RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
                                }

                                // If we don't have all the requested pre-trigger data in memory, just save what we have.
                                if (-preTriggerIndex > waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisk)) {
                                    preTriggerIndex = -waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisk);
                                    if (saveManager->mustSaveCompleteDataBlocks()) {
                                        // Round up to nearest data block boundary
                                        int numWholeDataBlocks = ceil((double)preTriggerIndex /
                                                                      (double)RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum()));
                                        preTriggerIndex = numWholeDataBlocks * RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
                                    }
                                }

                                // Save pre-trigger data.
                                saveManager->writeToSaveFiles(-preTriggerIndex, preTriggerIndex);
                            }
                        }
                    }

                    if (isRecording) {
                        // Save new data to disk.
                        totalBytesWritten = saveManager->writeToSaveFiles(NumSamples);
                        if (statusBarUpdateTimer.elapsed() >= 250) {  // Update status bar every 250 msec.
                            setStatusBarRecording(bytesPerMinute, saveManager->saveFileDateTimeStamp(), totalBytesWritten);
                            statusBarUpdateTimer.restart();
                        }

                        if (state->triggered) {     // A trigger has been found and we are recording
                            if (triggerEndCounter == 0) {   // No end trigger has yet been found
                                triggerBeginCounter += NumSamples;
                                if (triggerBeginCounter > glitchThreshold) {
                                    // Watch for trigger end event.
                                    int triggerTimeIndex = findTrigger(NumSamples, FindTriggerEnd);
                                    bool triggerEndFound = triggerTimeIndex >= 0;
                                    if (triggerEndFound) {
//                                        cout << "TRIGGER END FOUND" << endl;
                                        triggerEndCounter += NumSamples - triggerTimeIndex; // Trigger end found; start counting.
                                    }
                                }
                            } else {    // Trigger end has previously been found; keep counting.
                                if (findTrigger(NumSamples, FindTriggerBegin) >= 0) {
                                    triggerEndCounter = 0;  // Ignore brief trigger-off events
//                                    cout << "TRIGGER REASSERTED" << endl;
                                } else {
                                    triggerEndCounter += NumSamples;
                                }
                            }
                            if (triggerEndCounter > triggerEndSamples) {    // We have recorded sufficient post-trigger samples.
//                                cout << "CLOSING TRIGGERED FILE" << endl;
                                state->triggerSet = false;
                                state->recording = false;
                                saveManager->closeAllSaveFiles();
                                isRecording = false;
                            }
                        }

                        if (isRecording) {
                            totalRecordedSamples += NumSamples;
                            totalSamplesInFile += NumSamples;
                            if (saveManager->maxSamplesInFile() > 0) {
                                if (totalSamplesInFile >= saveManager->maxSamplesInFile()) {  // Time limit reached.  Start new file.
//                                    cout << "TIME LIMIT REACHED; STARTING NEW FILE" << endl;
                                    saveManager->closeAllSaveFiles();
                                    if (!saveManager->openAllSaveFiles()) {
                                        emit error(saveFileErrorMessage);
                                        emit sendSetCommand("RunMode", "Stop");
                                        close();
                                        break;
                                    } else {
                                        totalSamplesInFile = 0;
                                        bytesPerMinute = saveManager->bytesPerMinute();
                                    }
                                }
                            }
                        }

                    } else if (state->triggered && !state->triggerSet) {   // Ignore glitches immediately after trigger end.
                        triggerEndCounter += NumSamples;
                        if (triggerEndCounter > triggerEndSamples + glitchThreshold) {  // Retriggered
//                            cout << "RETRIGGERED" << endl;
                            state->triggered = false;
                            state->triggerSet = true;
                            triggerEndCounter = 0;
                        }
                    }

                    waveformFifo->freeOldData(WaveformFifo::ReaderDisk);

//                    double workTime = (double) workTimer.nsecsElapsed();
//                    double loopTime = (double) loopTimer.nsecsElapsed();
//                    workTimer.restart();
//                    loopTimer.restart();
//                    if (reportTimer.elapsed() >= 2000) {
//                        double cpuUsage = 100.0 * workTime / loopTime;
//                        cout << "                          SaveToDiskThread CPU usage: " << (int) cpuUsage << "%" << EndOfLine;
//                        reportTimer.restart();
//                    }
                } else {
                    if (stopRunASAP)
                        keepGoing = false;
                    usleep(1000);    // If new data is not ready, wait 1000 microseconds and try again.
                }
            }

            if (isRecording) {
//                cout << "MANUAL STOP RECORD; CLOSING SAVE FILE" << EndOfLine;
                saveManager->closeAllSaveFiles();
                isRecording = false;
            }
            running = false;
            state->recording = false;
            state->triggered = false;
            state->triggerSet = false;
        } else {
            if (saveManager) {
                delete saveManager;
                saveManager = nullptr;
            }
            usleep(1000);
        }
    }
    if (saveManager) {
        delete saveManager;
        saveManager = nullptr;
    }
}

void SaveToDiskThread::saveLiveNote(const QString& note)
{
    saveManager->writeLiveNote(note, totalRecordedSamples);
}

void SaveToDiskThread::setPosStimAmplitude(int stream, int channel, int amplitude)
{
    saveManager->setPosStimAmplitude(stream, channel, amplitude);
}

void SaveToDiskThread::setNegStimAmplitude(int stream, int channel, int amplitude)
{
    saveManager->setNegStimAmplitude(stream, channel, amplitude);
}

void SaveToDiskThread::startRunning()
{
    saveManager = nullptr;
    switch (state->getFileFormatEnum()) {
    case FileFormatIntan:
        saveManager = new IntanFileSaveManager(waveformFifo, state);
        break;
    case FileFormatFilePerSignalType:
        saveManager = new FilePerSignalTypeSaveManager(waveformFifo, state);
        break;
    case FileFormatFilePerChannel:
        saveManager = new FilePerChannelSaveManager(waveformFifo, state);
        break;
    default:
        cerr << "SaveToDiskThread::startRunning: invalid file format enum: " << state->getFileFormatEnum() << '\n';
        break;
    }

    stopRunASAP = false;
    keepGoing = true;
}

void SaveToDiskThread::stopRunning()
{
    stopRunASAP = true;
}

void SaveToDiskThread::close()
{
    stopRunASAP = true;
    stopThread = true;
}

bool SaveToDiskThread::isActive() const
{
    return running;
}

// Returns -1 if no trigger is found in numSamples.
int SaveToDiskThread::findTrigger(int numSamples, FindTriggerMode mode)
{
    int triggerTimeIndex = -1;
    bool triggerPolarityHigh = (mode == FindTriggerBegin) ? triggerOnHigh : !triggerOnHigh;

    if (digitalTrigger) {    // Search for digital input trigger
        uint16_t triggerMask = 0x0001u << triggerChannel;
        for (int t = 0; t < numSamples; ++t) {
            uint16_t digIn = waveformFifo->getDigitalData(WaveformFifo::ReaderDisk, boardDigitalInWaveform, t);
            if (triggerPolarityHigh) {  // Trigger on logic high
                if (digIn & triggerMask) {
                    triggerTimeIndex = t;
                    break;
                }
            } else {                    // Trigger on logic low
                if (!(digIn & triggerMask)) {
                    triggerTimeIndex = t;
                    break;
                }
            }
        }
    } else {    // Search for analog input trigger
        for (int t = 0; t < numSamples; ++t) {
            float anaIn = waveformFifo->getAnalogData(WaveformFifo::ReaderDisk, boardAdcWaveform[triggerChannel], t);
            if (triggerPolarityHigh) {  // Trigger on logic high
                if (anaIn >= analogTriggerThreshold) {
                    triggerTimeIndex = t;
                    break;
                }
            } else {                    // Trigger on logic low
                if (anaIn < analogTriggerThreshold) {
                    triggerTimeIndex = t;
                    break;
                }
            }
        }
    }
    return triggerTimeIndex;
}

void SaveToDiskThread::setStatusBarRecording(double bytesPerMinute, const QString& dateTimeStamp, int64_t totalBytesSaved)
{
    QTime recordTime(0, 0);
    int totalRecordTimeSeconds = round((double)totalRecordedSamples / state->sampleRate->getNumericValue());
    QString timeString = recordTime.addSecs(totalRecordTimeSeconds).toString("HH:mm:ss");

    QString statusFilename = state->filename->getFullFilename();
    QString suffix = state->getControllerTypeEnum() == ControllerStimRecord ? ".rhs" : ".rhd";

    switch (state->getFileFormatEnum()) {
    case FileFormatIntan:
        statusFilename += dateTimeStamp;
        if (!state->createNewDirectory->getValue()) {
            statusFilename += suffix;
        }
        break;
    case FileFormatFilePerSignalType:
    case FileFormatFilePerChannel:
        if (state->createNewDirectory->getValue()) {
            statusFilename += dateTimeStamp;
        }
        break;
    }

    emit setStatusBar(tr("Saving data to ") + statusFilename +
                      ".  (" + QString::number(bytesPerMinute / (1024.0 * 1024.0), 'f', 1) +
                      tr(" MB/minute.  File size may be reduced by disabling unused inputs.)  "
                         "Total data saved: ") + QString::number(totalBytesSaved / (1024.0 * 1024.0), 'f', 1) +
                      tr(" MB."));
    emit setTimeLabel(timeString);
}

void SaveToDiskThread::setStatusBarWaitForTrigger()
{
    QString polarity = state->triggerPolarity->getValue().toLower();
    emit setStatusBar(tr("Waiting for logic ") + polarity + tr(" trigger on ") +
                      state->triggerSource->getValue().toUpper() + "...");
    emit setTimeLabel("00:00:00");
}
