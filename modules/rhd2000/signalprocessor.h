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

#ifndef SIGNALPROCESSOR_H
#define SIGNALPROCESSOR_H

#include <queue>
#include "intanui.h"
#include "qtincludes.h"

using namespace std;

class QDataStream;
class SignalSources;
class Rhd2000DataBlock;
class RandomNumber;

class SignalProcessor
{
public:
    SignalProcessor();

    void allocateMemory(int numStreams);
    void setNotchFilter(double notchFreq, double bandwidth, double sampleFreq);
    void setNotchFilterEnabled(bool enable);
    void setHighpassFilter(double cutoffFreq, double sampleFreq);
    void setHighpassFilterEnabled(bool enable);
    int loadAmplifierData(queue<Rhd2000DataBlock> &dataQueue, int numBlocks,
                          bool lookForTrigger, int triggerChannel, int triggerPolarity,
                          int &triggerIndex, bool addToBuffer,
                          queue<Rhd2000DataBlock> &bufferQueue, bool saveToDisk, QDataStream &out,
                          SaveFormat format, bool saveTemp, bool saveTtlOut, int timestampOffset,
                          const double &latencyMs = 0, const microseconds_t &dataRecvTimestamp = microseconds_t(0), Rhd2000Module *syMod = nullptr);
    int loadSyntheticData(int numBlocks, double sampleRate, bool saveToDisk,
                          QDataStream &out, SaveFormat format, bool saveTemp, bool saveTtlOut,
                          std::shared_ptr<SyncTimer> syncTimer, Rhd2000Module *syMod = nullptr);
    int saveBufferedData(queue<Rhd2000DataBlock> &bufferQueue, QDataStream &out, SaveFormat format,
                         bool saveTemp, bool saveTtlOut, int timestampOffset);
    void createSaveList(SignalSources *signalSources, bool addTriggerChannel, int triggerChannel);
    void createTimestampFilename(QString path);
    void openTimestampFile();
    void closeTimestampFile();
    void createSignalTypeFilenames(QString path);
    void openSignalTypeFiles(bool saveTtlOut);
    void closeSignalTypeFiles();
    void createFilenames(SignalSources *signalSources, QString path);
    void openSaveFiles(SignalSources *signalSources);
    void closeSaveFiles(SignalSources *signalSources);
    int bytesPerBlock(SaveFormat saveFormat, bool saveTemperature, bool saveTtlOut);
    void filterData(int numBlocks, const QVector<QVector<bool> > &channelVisible);
    void measureComplexAmplitude(QVector<QVector<QVector<double> > > &measuredMagnitude,
                           QVector<QVector<QVector<double> > > &measuredPhase,
                           int capIndex, int stream, int chipChannel,
                           int numBlocks, double sampleRate, double frequency, int numPeriods);
    int getNumTempSensors();
    void tempHistoryReset(int requestedLength);
    QVector<QVector<QVector<double> > > amplifierPreFilter;
    QVector<QVector<QVector<double> > > amplifierPostFilter;
    QVector<QVector<QVector<double> > > auxChannel;
    QVector<QVector<double> > supplyVoltage;
    QVector<double> tempAvg;
    QVector<double> tempRaw;
    QVector<QVector<double> > boardAdc;
    QVector<QVector<int> > boardDigIn;
    QVector<QVector<int> > boardDigOut;

private:
    QVector<QVector<QVector<double> > > prevAmplifierPreFilter;
    QVector<QVector<QVector<double> > > prevAmplifierPostFilter;
    QVector<QVector<double> > highpassFilterState;

    int numDataStreams;
    double a1;
    double a2;
    double b0;
    double b1;
    double b2;
    bool notchFilterEnabled;
    double aHpf;
    double bHpf;
    bool highpassFilterEnabled;

    bool saveListBoardDigIn;
    QVector<SignalChannel*> saveListAmplifier;
    QVector<SignalChannel*> saveListAuxInput;
    QVector<SignalChannel*> saveListSupplyVoltage;
    QVector<SignalChannel*> saveListBoardAdc;
    QVector<SignalChannel*> saveListBoardDigitalIn;
    QVector<SignalChannel*> saveListBoardDigitalOut;
    QVector<SignalChannel*> saveListTempSensor;

    QString timestampFileName;
    QFile *timestampFile;
    QDataStream *timestampStream;

    QString amplifierFileName;
    QFile *amplifierFile;
    QDataStream *amplifierStream;

    QString auxInputFileName;
    QFile *auxInputFile;
    QDataStream *auxInputStream;

    QString supplyFileName;
    QFile *supplyFile;
    QDataStream *supplyStream;

    QString adcInputFileName;
    QFile *adcInputFile;
    QDataStream *adcInputStream;

    QString digitalInputFileName;
    QFile *digitalInputFile;
    QDataStream *digitalInputStream;

    QString digitalOutputFileName;
    QFile *digitalOutputFile;
    QDataStream *digitalOutputStream;

    void allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                               int xSize, int ySize, int zSize);
    void allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                               int xSize, int ySize);
    void allocateIntArray2D(QVector<QVector<int> > &array2D,
                            int xSize, int ySize);
    void allocateDoubleArray1D(QVector<double> &array1D, int xSize);
    void fillZerosDoubleArray3D(QVector<QVector<QVector<double> > > &array3D);
    void fillZerosDoubleArray2D(QVector<QVector<double> > &array2D);
    void amplitudeOfFreqComponent(double &realComponent, double &imagComponent,
                                  const QVector<double> &data,
                                  int startIndex, int endIndex,
                                  double sampleRate, double frequency);

    void tempHistoryPush(QVector<double> &tempData);
    void tempHistoryCalcAvg();

    RandomNumber *random;
    QVector<QVector<QVector<double> > > synthSpikeAmplitude;
    QVector<QVector<QVector<double> > > synthSpikeDuration;
    QVector<QVector<double> > synthRelativeSpikeRate;
    QVector<QVector<double> > synthEcgAmplitude;
    double tPulse;
    unsigned int synthTimeStamp;

    QVector<QVector<double> > tempRawHistory;
    int tempHistoryLength;
    int tempHistoryMaxLength;

    // To speed up writing to disk, we must create a char array which we fill with bytes of raw data
    // and use QDataStream::writeRawData(const char *s, int len) to stream out more efficiently.
    char dataStreamBuffer[2 * MAX_NUM_DATA_STREAMS * 32 * SAMPLES_PER_DATA_BLOCK];
    char dataStreamBufferArray[MAX_NUM_DATA_STREAMS * 32][2 * SAMPLES_PER_DATA_BLOCK * 16]; // Assume maximum number of data blocks = 16
    int bufferArrayIndex[MAX_NUM_DATA_STREAMS * 32];
};

#endif // SIGNALPROCESSOR_H
