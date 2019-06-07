//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.5.2
//  Copyright (C) 2013-2017 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef IntanUI_H
#define IntanUI_H

#include <QWidget>
#include <queue>
#include <QTime>
#include "rhd2000datablock.h"
#include "globalconstants.h"

#include "barrier.h"

class QAction;
class QPushButton;
class QButtonGroup;
class QRadioButton;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QSlider;
class QLineEdit;
class QLabel;
class QFile;
class QMenu;
class WavePlot;
class SignalProcessor;
class Rhd2000EvalBoard;
class SignalSources;
class SignalGroup;
class SignalChannel;
class SpikeScopeDialog;
class KeyboardShortcutDialog;
class HelpDialogChipFilters;
class HelpDialogComparators;
class HelpDialogDacs;
class HelpDialogHighpassFilter;
class HelpDialogNotchFilter;
class HelpDialogFastSettle;
class WaitForTriggerDialog;
class Rhd2000Module;

using namespace std;

class IntanUI : public QWidget
{
    Q_OBJECT
    friend class Rhd2000Module;

public:
    IntanUI(Rhd2000Module *maModule, QWidget *parent = nullptr);
    ~IntanUI();

    void setNumWaveformsComboBox(int index);

    QComboBox *yScaleComboBox;
    QComboBox *tScaleComboBox;

    QVector<int> yScaleList;
    QVector<int> tScaleList;
    QVector<QVector<bool> > channelVisible;

    int getEvalBoardMode();
    bool isRecording();

    QWidget *displayWidget() const;

    void runInterfaceBoard();
    void triggerRecordInterfaceBoard();
    void stopInterfaceBoard();

    void interfaceBoardPrepareRecording();
    void interfaceBoardInitRun();
    bool interfaceBoardRunCycle();
    void interfaceBoardStopFinalize();

    void setBaseFileName(const QString& fname);
    void loadSettings(const QByteArray &data);
    void exportSettings(QDataStream &outStream);

    WavePlot *getWavePlot() const;

    bool isRunning() const;

    int currentFifoPercentageFull() const;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void about();
    void keyboardShortcutsHelp();
    void chipFiltersHelp();
    void comparatorsHelp();
    void dacsHelp();
    void highpassFilterHelp();
    void notchFilterHelp();
    void fastSettleHelp();
    void openIntanWebsite();
    void changeNumFrames(int index);
    void changeYScale(int index);
    void changeTScale(int index);
    void changeSampleRate(int sampleRateIndex);
    void changeNotchFilter(int notchFilterIndex);
    void enableHighpassFilter(bool enable);
    void highpassFilterLineEditChanged();
    void changeBandwidth();
    void changeImpedanceFrequency();
    void changePort(int port);
    void changeDacGain(int index);
    void changeDacNoiseSuppress(int index);
    void dacEnable(bool enable);
    void dacSetChannel();
    void dacSelected(int dacChannel);
    void renameChannel();
    void sortChannelsByNumber();
    void sortChannelsByName();
    void restoreOriginalChannelOrder();
    void alphabetizeChannels();
    void toggleChannelEnable();
    void enableAllChannels();
    void disableAllChannels();
    void spikeScope();
    void newSelectedChannel(SignalChannel* newChannel);
    void scanPorts();
    void enableFastSettle(int enabled);
    void enableExternalFastSettle(bool enabled);
    void setExternalFastSettleChannel(int channel);
    void showImpedances(bool enabled);
    void saveImpedances();
    void runImpedanceMeasurement();
    void configDigOutControl();
    void manualCableDelayControl();
    void plotPointsMode(bool enabled);
    void setSaveFormatDialog();
    void setDacThreshold1(int threshold);
    void setDacThreshold2(int threshold);
    void setDacThreshold3(int threshold);
    void setDacThreshold4(int threshold);
    void setDacThreshold5(int threshold);
    void setDacThreshold6(int threshold);
    void setDacThreshold7(int threshold);
    void setDacThreshold8(int threshold);

private:
    void createActions();
    void createMenus();
    void createStatusBar();
    void createLayout();

    void openInterfaceBoard();
    void findConnectedAmplifiers();
    int deviceId(Rhd2000DataBlock *dataBlock, int stream, int &register59Value);

    void updateImpedanceFrequency();
    void setDacGainLabel(int gain);
    void setDacNoiseSuppressLabel(int noiseSuppress);
    void setDacChannelLabel(int dacChannel, QString channel, QString name);

    void writeSaveFileHeader(QDataStream &outStream, QDataStream &infoStream, SaveFormat format, int numTempSensors);
    void factorOutParallelCapacitance(double &trueMagnitude, double &impedancePhase,
                                      double frequency, double parasiticCapacitance);
    void empiricalResistanceCorrection(double &impedanceMagnitude, double &impedancePhase,
                                       double boardSampleRate);

    void setStatusBarReady();
    void setStatusBarRunning();
    void setStatusBarRecording(double bytesPerMinute);
    void setStatusBarWaitForTrigger();

    void setSaveFormat(SaveFormat format);
    void startNewSaveFile(SaveFormat format);
    void closeSaveFile(SaveFormat format);

    void setHighpassFilterCutoff(double cutoff);

    void updateAuxDigOut();

    int ttlOut[16];
    int evalBoardMode;

    bool running;
    bool recording;
    bool triggerSet;
    bool triggered;

    bool saveTemp;
    bool saveTtlOut;
    bool validFilename;
    bool synthMode;

    QString saveBaseFileName;
    QString saveFileName;
    QFile *saveFile;
    QDataStream *saveStream;

    QString infoFileName;
    QFile *infoFile;
    QDataStream *infoStream;

    SaveFormat saveFormat;
    int newSaveFilePeriodMinutes;

    unsigned int numUsbBlocksToRead;

    Rhd2000EvalBoard *evalBoard;
    SignalSources *signalSources;

    double cableLengthPortA;  // in meters
    double cableLengthPortB;  // in meters
    double cableLengthPortC;  // in meters
    double cableLengthPortD;  // in meters

    double desiredDspCutoffFreq;
    double actualDspCutoffFreq;
    double desiredUpperBandwidth;
    double actualUpperBandwidth;
    double desiredLowerBandwidth;
    double actualLowerBandwidth;
    bool dspEnabled;
    double notchFilterFrequency;
    double notchFilterBandwidth;
    bool notchFilterEnabled;
    double highpassFilterFrequency;
    bool highpassFilterEnabled;
    bool fastSettleEnabled;
    double desiredImpedanceFreq;
    double actualImpedanceFreq;
    bool impedanceFreqValid;

    int recordTriggerChannel;
    int recordTriggerPolarity;
    int recordTriggerBuffer;
    int postTriggerTime;
    bool saveTriggerChannel;

    QVector<bool> auxDigOutEnabled;
    QVector<int> auxDigOutChannel;
    QVector<bool> manualDelayEnabled;
    QVector<int> manualDelay;

    double boardSampleRate;

    QVector<double> sampleRateList;

    QVector<SignalChannel*> dacSelectedChannel;
    QVector<bool> dacEnabled;
    QVector<int> chipId;

    queue<Rhd2000DataBlock> dataQueue;
    queue<Rhd2000DataBlock> filteredDataQueue;

    WavePlot *wavePlot;
    SignalProcessor *signalProcessor;

    SpikeScopeDialog *spikeScopeDialog;
    KeyboardShortcutDialog *keyboardShortcutDialog;
    HelpDialogChipFilters *helpDialogChipFilters;
    HelpDialogComparators *helpDialogComparators;
    HelpDialogDacs *helpDialogDacs;
    HelpDialogHighpassFilter *helpDialogHighpassFilter;
    HelpDialogNotchFilter *helpDialogNotchFilter;
    HelpDialogFastSettle *helpDialogFastSettle;

    QAction *originalOrderAction;
    QAction *alphaOrderAction;
    QAction *aboutAction;
    QAction *intanWebsiteAction;
    QAction *keyboardHelpAction;
    QAction *renameChannelAction;
    QAction *toggleChannelEnableAction;
    QAction *enableAllChannelsAction;
    QAction *disableAllChannelsAction;

    QMenu *fileMenu;
    QMenu *channelMenu;
    QMenu *optionsMenu;
    QMenu *helpMenu;

    QPushButton *spikeScopeButton;
    QPushButton *changeBandwidthButton;
    QPushButton *impedanceFreqSelectButton;
    QPushButton *runImpedanceTestButton;
    QPushButton *dacSetButton;
    QPushButton *scanButton;
    QPushButton *digOutButton;
    QPushButton *saveImpedancesButton;
    QPushButton *setSaveFormatButton;
    QPushButton *helpDialogChipFiltersButton;
    QPushButton *helpDialogComparatorsButton;
    QPushButton *helpDialogDacsButton;
    QPushButton *helpDialogHighpassFilterButton;
    QPushButton *helpDialogNotchFilterButton;
    QPushButton *helpDialogSettleButton;
    QPushButton *setCableDelayButton;

    QCheckBox *dacEnableCheckBox;
    QCheckBox *dacLockToSelectedBox;
    QCheckBox *fastSettleCheckBox;
    QCheckBox *externalFastSettleCheckBox;
    QCheckBox *showImpedanceCheckBox;
    QCheckBox *plotPointsCheckBox;
    QCheckBox *highpassFilterCheckBox;

    QRadioButton *displayPortAButton;
    QRadioButton *displayPortBButton;
    QRadioButton *displayPortCButton;
    QRadioButton *displayPortDButton;
    QRadioButton *displayAdcButton;
    QRadioButton *displayDigInButton;

    QButtonGroup *dacButtonGroup;
    QRadioButton *dacButton1;
    QRadioButton *dacButton2;
    QRadioButton *dacButton3;
    QRadioButton *dacButton4;
    QRadioButton *dacButton5;
    QRadioButton *dacButton6;
    QRadioButton *dacButton7;
    QRadioButton *dacButton8;

    QComboBox *numFramesComboBox;
    QComboBox *sampleRateComboBox;
    QComboBox *notchFilterComboBox;

    QSpinBox *dac1ThresholdSpinBox;
    QSpinBox *dac2ThresholdSpinBox;
    QSpinBox *dac3ThresholdSpinBox;
    QSpinBox *dac4ThresholdSpinBox;
    QSpinBox *dac5ThresholdSpinBox;
    QSpinBox *dac6ThresholdSpinBox;
    QSpinBox *dac7ThresholdSpinBox;
    QSpinBox *dac8ThresholdSpinBox;
    QSpinBox *externalFastSettleSpinBox;

    QSlider *dacGainSlider;
    QSlider *dacNoiseSuppressSlider;

    QLineEdit *highpassFilterLineEdit;
    QLineEdit *note1LineEdit;
    QLineEdit *note2LineEdit;
    QLineEdit *note3LineEdit;

    QLabel *statusBarLabel;
    QLabel *fifoLagLabel;
    QLabel *fifoFullLabel;
    QLabel *dspCutoffFreqLabel;
    QLabel *upperBandwidthLabel;
    QLabel *lowerBandwidthLabel;
    QLabel *desiredImpedanceFreqLabel;
    QLabel *actualImpedanceFreqLabel;
    QLabel *dacGainLabel;
    QLabel *dacNoiseSuppressLabel;

    Rhd2000Module *maModule;
    QWidget *liveDisplayWidget;

    // data used in a data acquisition run
    struct {
        bool runInitialized = false;

        int triggerIndex;
        QTime timer;

        double fifoPercentageFull;
        double fifoCapacity;
        double samplePeriod;
        double latency;

        long long totalBytesWritten = 0;
        unsigned int wordsInFifo;
        unsigned int dataBlockSize;

        int timestampOffset = 0;

        queue<Rhd2000DataBlock> bufferQueue;
        unsigned int preTriggerBufferQueueLength = 0;

        double bytesPerMinute;
        double totalRecordTimeSeconds = 0.0;
        double recordTimeIncrementSeconds;

        int fifoNearlyFull = 0; // static
        int triggerEndCounter = 0; // static

        int triggerEndThreshold;

        int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};
        int ledIndex = 0;
    } crd;
};

#endif // IntanUI_H
