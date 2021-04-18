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

#ifndef SAVETODISKTHREAD_H
#define SAVETODISKTHREAD_H

#include <QObject>
#include <QThread>
#include <atomic>
#include "waveformfifo.h"
#include "systemstate.h"
#include "signalsources.h"
#include "rhxdatablock.h"
#include "savemanager.h"

using namespace std;

class SaveToDiskThread : public QThread
{
    Q_OBJECT
public:
    explicit SaveToDiskThread(WaveformFifo *waveformFifo_, SystemState *state_, QObject *parent = nullptr);
    ~SaveToDiskThread();

    void run() override;
    void startRunning();
    void stopRunning();
    bool isActive() const;
    void close();

    int64_t getTotalRecordedSamples() const { return totalRecordedSamples; }

    enum FindTriggerMode {
        FindTriggerBegin,
        FindTriggerEnd
    };

signals:
    void setStatusBar(QString);
    void setTimeLabel(QString);
    void sendSetCommand(QString, QString);
    void error(QString);

public slots:
    void saveLiveNote(const QString& note);
    void setPosStimAmplitude(int stream, int channel, int amplitude);
    void setNegStimAmplitude(int stream, int channel, int amplitude);

private:
    WaveformFifo* waveformFifo;
    SystemState* state;
    SaveManager* saveManager;

    volatile bool keepGoing;
    volatile bool running;
    volatile bool stopThread;

    vector<float*> boardAdcWaveform;
    uint16_t* boardDigitalInWaveform;

    bool digitalTrigger;
    int triggerChannel;
    bool triggerOnHigh;
    float analogTriggerThreshold;

    atomic<int64_t> totalRecordedSamples;

    int findTrigger(int numSamples, FindTriggerMode mode);
    void setStatusBarRecording(double bytesPerMinute, const QString& dateTimeStamp, int64_t totalBytesSaved);
    void setStatusBarWaitForTrigger();
};

#endif // SAVETODISKTHREAD_H
