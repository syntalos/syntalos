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

#include "labrstimclient.h"

#include <QSerialPort>
#include <QCoreApplication>
#include <QThread>
#include <QDebug>

LabrstimClient::LabrstimClient(QObject *parent)
    : QObject(parent),
      m_running(false)
{
    m_serial = new QSerialPort(this);

    connect(
        m_serial,
        static_cast<void (QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error),
        this,
        &LabrstimClient::handleError,
        Qt::DirectConnection);

    connect(m_serial, &QSerialPort::readyRead, this, &LabrstimClient::readData);

    // default values
    m_trialDuration = 0;
    m_pulseDuration = 0;
    m_laserIntensity = 0;

    m_randomIntervals = 0;
    m_minimumInterval = 0;
    m_maximumInterval = 0;

    m_swrRefractoryTime = 0;
    m_swrPowerThreshold = 0;
    m_convolutionPeakThreshold = 0;
    m_swrDelayStimulation = false;
}

LabrstimClient::~LabrstimClient()
{
    // ensure we are stopped
    this->disconnect();
    if (m_serial->isOpen())
        sendRequest("STOP");
}

QString LabrstimClient::lastError() const
{
    return m_lastError;
}

QString LabrstimClient::sendRequest(const QString &req, bool expectReply)
{
    m_lastError = QString();
    m_lastResult = QString();
    m_serial->write(QStringLiteral("%1\n").arg(req).toUtf8());
    emit newRawData(QStringLiteral("=> %1\n").arg(req));

    if (!expectReply)
        return QString();

    for (uint i = 1; i < 20; i++) {
        QCoreApplication::processEvents();

        if (!m_lastResult.isEmpty()) {
            return getLastResult();
        }

        QThread::msleep(50 * i);
    }

    // we didn't get a reply
    emitError(QStringLiteral("No reply received in time (Request: %1).").arg(req));
    return QString();
}

bool LabrstimClient::open(const QString &portName)
{
    // just make sure the port isn't already open
    close();

    // set general port settings suitable for the device
    m_serial->setPortName(portName);
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setStopBits(QSerialPort::OneStop);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emitError("Connection error");
        return false;
    }

    // send some stuff to check the connection
    m_serial->write("NOOP\n"); // we expect no reply to this
    if (sendRequest("PING") != "PONG") {
        emitError("Unable to communicate with the device.");
        close();
        return false;
    }

    // request device software version
    auto ver = sendRequest("VERSION");
    if (ver.isEmpty()) {
        emitError("Could not determine client version.");
        close();
        return false;
    }
    if (!ver.startsWith("VERSION ")) {
        emitError("Version check failed: The reply was invalid.");
        close();
        return false;
    }
    m_clientVersion = ver.remove(0, 8);

    return true;
}

void LabrstimClient::close()
{
    if (m_serial->isOpen())
        m_serial->close();
}

bool LabrstimClient::runStimulation()
{
    if (m_running) {
        emitError("Already running.");
        return false;
    }

    auto command = QStringLiteral("RUN");
    switch (m_mode) {
    case ModeSwr:
        command.append(" swr");
        break;
    case ModeTheta:
        command.append(" theta");
        break;
    case ModeTrain:
        command.append(" train");
        break;
    case ModeSpikes:
        command.append(" spikedetect");
        break;
    default:
        emitError("No valid stimulation mode set.");
        return false;
    }

    if (m_mode == ModeSwr) {
        // SWR-specific settings

        if (m_swrRefractoryTime != 0)
            command.append(QString(" -f %1").arg(m_swrRefractoryTime));
        if (m_swrPowerThreshold != 0)
            command.append(QString(" -s %1").arg(m_swrPowerThreshold));

        command.append(QString(" -C %1").arg(m_convolutionPeakThreshold));

    } else if (m_mode == ModeTheta) {
        // Theta-specific stuff
        command.append(QString(" -t %1").arg(m_thetaPhase));
        if (m_randomIntervals)
            command.append(QString(" -R"));

    } else if (m_mode == ModeTrain) {
        // Train-specific stuff
        command.append(QString(" -T %1").arg(m_trainFrequency));
        if (m_randomIntervals)
            command.append(QString(" -R"));

    } else if (m_mode == ModeSpikes) {
        // Spike-detection specific stuff
        command.append(QString(" -t %1").arg(m_spikeTriggerFrequency));
        command.append(QString(" -w %1").arg(m_spikeDetectionWindow));
        command.append(QString(" -d %1").arg(m_spikeStimCooldownTime));
        command.append(QString(" -s %1").arg(m_spikeThresholdValue));
    }

    // random intervals count for all modes
    if (m_randomIntervals && m_mode != ModeSpikes) {
        command.append(QString(" -m %1").arg(m_minimumInterval));
        command.append(QString(" -M %1").arg(m_maximumInterval));
    }

    command.append(QStringLiteral(" --"));
    command.append(QString(" %1").arg(m_samplingFrequency));
    command.append(QString(" %1").arg(m_trialDuration));
    command.append(QString(" %1").arg(m_pulseDuration));
    command.append(QString(" %1").arg(m_laserIntensity));

    const auto res = sendRequest(command);
    if (res != "OK") {
        emitError(QStringLiteral("Unable to start stimulation. [%1]").arg(m_lastError));
        return false;
    }

    m_running = true;
    return true;
}

bool LabrstimClient::stopStimulation()
{
    auto res = sendRequest("STOP");
    if ((res != "OK") && (!res.startsWith("FINISHED"))) {
        if (m_running)
            emitError(QStringLiteral("Unable to stop stimulation."));
        return false;
    }

    m_running = false;
    return true;
}

void LabrstimClient::rebootDevice()
{
    if (m_running)
        stopStimulation();

    sendRequest("REBOOT", false);
}

void LabrstimClient::shutdownDevice()
{
    if (m_running)
        stopStimulation();

    sendRequest("SHUTDOWN", false);
}

void LabrstimClient::readData()
{
    auto data = m_serial->readAll();
    emit newRawData(data);

    // quick & dirty
    m_lastResultBuf.append(QString(data));
    if (m_lastResultBuf.endsWith("\n")) {
        // we might read multiple lines, process them individually
        auto replies = m_lastResultBuf.split("\n", Qt::SkipEmptyParts);
        foreach (auto reply, replies) {
            m_lastResult = reply;

            if (m_lastResult.startsWith("ERROR")) {
                emitError(getLastResult());
                if (m_running)
                    emit stimulationFinished();
                m_running = false;
            } else if (m_lastResult.startsWith("FINISHED") || m_lastResult == "STARTUP") {
                if (m_running)
                    emit stimulationFinished();
                m_running = false;
            }
        }

        m_lastResultBuf = QString();
    }
}

void LabrstimClient::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        emitError(m_serial->errorString());
        close();
    }
}

QString LabrstimClient::getLastResult()
{
    auto res = m_lastResult.replace("\n", "").replace("\\n", "\n");
    m_lastResult = QString();

    return res;
}

void LabrstimClient::emitError(const QString &message)
{
    m_lastError = message;
    emit error(m_lastError);
}

QString LabrstimClient::clientVersion() const
{
    return m_clientVersion;
}

bool LabrstimClient::isRunning() const
{
    return m_running;
}

void LabrstimClient::setMode(LabrstimClient::Mode mode)
{
    m_mode = mode;
}

double LabrstimClient::trialDuration() const
{
    return m_trialDuration;
}

void LabrstimClient::setTrialDuration(double val)
{
    m_trialDuration = val;
}

double LabrstimClient::pulseDuration() const
{
    return m_pulseDuration;
}

void LabrstimClient::setPulseDuration(double val)
{
    m_pulseDuration = val;
}

double LabrstimClient::laserIntensity() const
{
    return m_laserIntensity;
}

void LabrstimClient::setLaserIntensity(double val)
{
    m_laserIntensity = val;
}

int LabrstimClient::samplingFrequency() const
{
    return m_samplingFrequency;
}

void LabrstimClient::setSamplingFrequency(int hz)
{
    m_samplingFrequency = hz;
}

bool LabrstimClient::randomIntervals() const
{
    return m_randomIntervals;
}

void LabrstimClient::setRandomIntervals(bool random)
{
    m_randomIntervals = random;
}

double LabrstimClient::minimumInterval() const
{
    return m_minimumInterval;
}

void LabrstimClient::setMinimumInterval(double min)
{
    m_minimumInterval = min;
}

double LabrstimClient::maximumInterval() const
{
    return m_maximumInterval;
}

void LabrstimClient::setMaximumInterval(double max)
{
    m_maximumInterval = max;
}

double LabrstimClient::swrRefractoryTime() const
{
    return m_swrRefractoryTime;
}

void LabrstimClient::setSwrRefractoryTime(double val)
{
    m_swrRefractoryTime = val;
}

double LabrstimClient::swrPowerThreshold() const
{
    return m_swrPowerThreshold;
}

void LabrstimClient::setSwrPowerThreshold(double val)
{
    m_swrPowerThreshold = val;
}

double LabrstimClient::convolutionPeakThreshold() const
{
    return m_convolutionPeakThreshold;
}

void LabrstimClient::setConvolutionPeakThreshold(double val)
{
    m_convolutionPeakThreshold = val;
}

bool LabrstimClient::swrDelayStimulation() const
{
    return m_swrDelayStimulation;
}

void LabrstimClient::setSwrDelayStimulation(bool delay)
{
    m_swrDelayStimulation = delay;
}

double LabrstimClient::thetaPhase() const
{
    return m_thetaPhase;
}

void LabrstimClient::setThetaPhase(double val)
{
    m_thetaPhase = val;
}

double LabrstimClient::trainFrequency() const
{
    return m_trainFrequency;
}

void LabrstimClient::setTrainFrequency(double val)
{
    m_trainFrequency = val;
}

uint LabrstimClient::spikeDetectionWindow() const
{
    return m_spikeDetectionWindow;
}

void LabrstimClient::setSpikeDetectionWindow(uint val)
{
    m_spikeDetectionWindow = val;
}

uint LabrstimClient::spikeTriggerFrequency() const
{
    return m_spikeTriggerFrequency;
}

void LabrstimClient::setSpikeTriggerFrequency(uint val)
{
    m_spikeTriggerFrequency = val;
}

uint LabrstimClient::spikeStimCooldownTime() const
{
    return m_spikeStimCooldownTime;
}

void LabrstimClient::setSpikeStimCooldownTime(uint val)
{
    m_spikeStimCooldownTime = val;
}

int LabrstimClient::spikeThresholdValue() const
{
    return m_spikeThresholdValue;
}

void LabrstimClient::setSpikeThresholdValue(int val)
{
    m_spikeThresholdValue = val;
}
