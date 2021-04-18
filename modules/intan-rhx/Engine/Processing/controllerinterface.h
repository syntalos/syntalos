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

#ifndef CONTROLLERINTERFACE_H
#define CONTROLLERINTERFACE_H

#include <QObject>
#include <QString>
#include "rhxcontroller.h"
#include "datafilereader.h"
#include "rhxglobals.h"
#include "datastreamfifo.h"
#include "usbdatathread.h"
#include "waveformprocessorthread.h"
#include "rhxregisters.h"
#include "rhxdatareader.h"
#include "multicolumndisplay.h"
#include "savetodiskthread.h"
#include "audiothread.h"
#include "tcpdataoutputthread.h"
#include "systemstate.h"
#include "signalsources.h"
#include "xpucontroller.h"
#include "isidialog.h"
#include "psthdialog.h"
#include "spectrogramdialog.h"
#include "spikesortingdialog.h"

class ControlPanel;

class ControllerInterface : public QObject
{
    Q_OBJECT
public:
    ControllerInterface(SystemState* state_, AbstractRHXController* rhxController_, const QString& boardSerialNumber,
                        DataFileReader* dataFileReader_ = nullptr, QObject* parent = nullptr);
    ~ControllerInterface();

    void rescanPorts(bool updateDisplay = false);

    void updateChipCommandLists(bool updateStimParams = false);
    void getCableDelay(vector<int> &delays) const { rhxController->getCableDelay(delays); }
    void setCableDelay(BoardPort port, int delay) { rhxController->setCableDelay(port, delay); }
    void enableExternalDigOut(BoardPort port, bool enable) { rhxController->enableExternalDigOut(port, enable); }
    void setExternalDigOutChannel(BoardPort port, int channel) { rhxController->setExternalDigOutChannel(port, channel); }
    void setManualCableDelays();

    void toggleAudioThread(bool enabled);
    void runTCPDataOutputThread();

    void runController();
    void runControllerSilently(double nSeconds, QProgressDialog* progress = nullptr);
    float measureRmsLevel(string waveName, double timeSec) const;
    void setAllSpikeDetectionThresholds();
    void sweepDisplay(double speed);
    bool rewindPossible() const { return waveformFifo->numWordsInMemory(WaveformFifo::ReaderDisplay) > 0; }
    bool fastForwardPossible() const { return currentSweepPosition < 0; }

    void setDisplay(MultiColumnDisplay* display_) { display = display_; }
    void setControlPanel(ControlPanel* controlPanel_) { controlPanel = controlPanel_; }
    void setISIDialog(ISIDialog* isiDialog_) { isiDialog = isiDialog_; }
    void setPSTHDialog(PSTHDialog* psthDialog_) { psthDialog = psthDialog_; }
    void setSpectrogramDialog(SpectrogramDialog* spectrogramDialog_) { spectrogramDialog = spectrogramDialog_; }
    void setSpikeSortingDialog(SpikeSortingDialog* spikeSortingDialog_) { spikeSortingDialog = spikeSortingDialog_; }

    QString getCurrentAudioChannel() const { return currentAudioChannel; }

    void setStimSequenceParameters(Channel* ampChannel);
    void setAnalogOutSequenceParameters(Channel* anOutChannel);
    void setDigitalOutSequenceParameters(Channel* digOutChannel);

    void setManualStimTrigger(int trigger, bool triggerOn);
    void setManualStimTrigger(QString keyName, bool triggerOn);

    void setChargeRecoveryParameters(bool mode, RHXRegisters::ChargeRecoveryCurrentLimit currentLimit,
                                     double targetVoltage);

    void setAmpSettleMode(bool useFastSettle) { rhxController->setAmpSettleMode(useFastSettle); }
    void setGlobalSettlePolicy(bool settleWholeHeadstageA, bool settleWholeHeadstageB, bool settleWholeHeadstageC,
                               bool settleWholeHeadstageD, bool settleAllHeadstages)
        { rhxController->setGlobalSettlePolicy( settleWholeHeadstageA, settleWholeHeadstageB, settleWholeHeadstageC,
                                                settleWholeHeadstageD, settleAllHeadstages); }

    void setDacGain(int dacGainIndex);
    void setAudioNoiseSuppress(int noiseSuppressIndex);
    void setDacHighpassFilterEnabled(bool enabled);
    void setDacHighpassFilterFrequency(double frequency);
    void setDacChannel(int dac, const QString& channelName);
    void setDacRefChannel(const QString& channelName);
    void setDacThreshold(int dac, int threshold);
    void setDacEnabled(int dac, bool enabled);
    void setTtlOutMode(bool mode1, bool mode2, bool mode3, bool mode4, bool mode5, bool mode6, bool mode7, bool mode8);

    void enableFastSettle(bool enabled);
    void enableExternalFastSettle(bool enabled);
    void setExternalFastSettleChannel(int channel);

    SaveToDiskThread* saveThread() const { return saveToDiskThread; }

    QString playbackFileName() const;
    QString currentTimePlaybackFile() const;
    QString startTimePlaybackFile() const;
    QString endTimePlaybackFile() const;

    void resetWaveformFifo();

    bool measureImpedances();
    bool saveImpedances();

    double swBufferPercentFull() const;
    double latestWaveformProcessorCpuLoad() const { return waveformProcessorCpuLoad; }

    void uploadAmpSettleSettings();
    void uploadChargeRecoverySettings();
    void uploadBandwidthSettings();
    void uploadStimParameters(Channel* channel);
    void uploadStimParameters();

signals:
    void setTimeLabel(QString text);
    void setTopStatusLabel(QString text);
    void haveStopped();
    void setHardwareFifoStatus(double percentFull);
    void cpuLoadPercent(double percent);
    void TCPErrorMessage(QString errorMessage);

public slots:
    void updateFromState();
    void updateCurrentAudioChannel(QString name);
    void manualStimTriggerOn(QString keyName);
    void manualStimTriggerOff(QString keyName);
    void manualStimTriggerPulse(QString keyName);

private slots:
    void updateHardwareFifo(double percentFull) { emit setHardwareFifoStatus(percentFull); }
    void updateWaveformProcessorCpuLoad(double percentLoad) { waveformProcessorCpuLoad = percentLoad; }

private:
    void openController(const QString& boardSerialNumber);
    void initializeController();
    int scanPorts(vector<ChipType> &chipType, vector<int> &portIndex, vector<int> &commandStream,
                  vector<int> &numChannelsOnPort);
    void addAmplifierChannels(const vector<ChipType> &chipType, const vector<int> &portIndex,
                              const vector<int> &commandStream, const vector<int> &numChannelsOnPort);
    void enablePlaybackChannels();
    void addPlaybackHeadstageChannels();

    void sendTCPError(QString errorMessage);

    SystemState* state;
    AbstractRHXController* rhxController;
    DataFileReader* dataFileReader;
    TCPDataOutputThread* tcpDataOutputThread;

    XPUController* xpuController;

    DataStreamFifo* usbStreamFifo;
    USBDataThread* usbDataThread;
    WaveformFifo* waveformFifo;
    WaveformProcessorThread* waveformProcessorThread;

    MultiColumnDisplay* display;
    ControlPanel* controlPanel;
    ISIDialog* isiDialog;
    PSTHDialog* psthDialog;
    SpectrogramDialog* spectrogramDialog;
    SpikeSortingDialog* spikeSortingDialog;

    AudioThread* audioThread;
    SaveToDiskThread* saveToDiskThread;

    int currentSweepPosition;

    bool audioEnabled;
    bool tcpDataOutputEnabled;

    QString currentAudioChannel;

    double hardwareFifoPercentFull;
    double waveformProcessorCpuLoad;
    vector<double> cpuLoadHistory;

    void outOfMemoryError(double memRequiredGB);
};

#endif // CONTROLLERINTERFACE_H
