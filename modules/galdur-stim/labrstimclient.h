/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LABRSTIMCLIENT_H
#define LABRSTIMCLIENT_H

#include <QObject>
#include <QString>
#include <QSerialPort>

class LabrstimClient : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        ModeUnknown,
        ModeTheta,
        ModeSwr,
        ModeTrain
    };

    explicit LabrstimClient(QObject *parent = 0);
    ~LabrstimClient();

    QString lastError() const;
    QString sendRequest(const QString &req, bool expectReply = true);
    QString clientVersion() const;

    void setMode(Mode mode);

    double trialDuration() const;
    void setTrialDuration(double val);

    double pulseDuration() const;
    void setPulseDuration(double val);

    double laserIntensity() const;
    void setLaserIntensity(double val);

    int samplingFrequency() const;
    void setSamplingFrequency(int hz);

    bool randomIntervals() const;
    void setRandomIntervals(bool random);

    double minimumInterval() const;
    void setMinimumInterval(double min);
    double maximumInterval() const;
    void setMaximumInterval(double max);

    double swrRefractoryTime() const;
    void setSwrRefractoryTime(double val);

    double swrPowerThreshold() const;
    void setSwrPowerThreshold(double val);

    double convolutionPeakThreshold() const;
    void setConvolutionPeakThreshold(double val);

    bool swrDelayStimulation() const;
    void setSwrDelayStimulation(bool delay);

    double thetaPhase() const;
    void setThetaPhase(double val);

    double trainFrequency() const;
    void setTrainFrequency(double val);

public slots:
    bool open(const QString &portName);
    void close();

    bool runStimulation();
    bool stopStimulation();

    void rebootDevice();
    void shutdownDevice();

private slots:
    void readData();
    void handleError(QSerialPort::SerialPortError error);

signals:
    void newRawData(const QString &text);
    void error(const QString &message);
    void stimulationFinished();

private:
    QString getLastResult();
    void emitError(const QString &message);

    QSerialPort *m_serial;
    QString m_lastError;
    QString m_lastResultBuf;
    QString m_lastResult;

    QString m_clientVersion;

    bool m_running;

    // stimulation settings
    Mode m_mode;
    int m_samplingFrequency;
    double m_trialDuration;
    double m_pulseDuration;
    double m_laserIntensity;

    bool m_randomIntervals;
    double m_minimumInterval;
    double m_maximumInterval;

    double m_swrRefractoryTime;
    double m_swrPowerThreshold;
    double m_convolutionPeakThreshold;
    bool m_swrDelayStimulation;

    double m_thetaPhase;

    double m_trainFrequency;
};

#endif // LABRSTIMCLIENT_H
