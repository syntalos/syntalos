/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "galdurmodule.h"

#include <QEventLoop>
#include <QTimer>
#include <QQueue>

#include "streams/subscriptionnotifier.h"

#include "labrstimclient.h"
#include "galdursettingsdialog.h"

SYNTALOS_MODULE(GaldurModule)

class GaldurModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;

    GaldurSettingsDialog *m_settingsDlg;

    QQueue<QString> m_rawMessages;
    std::mutex m_rawMsgMutex;

public:
    explicit GaldurModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_ctlPort = registerInputPort<ControlCommand>(QStringLiteral("control-in"), QStringLiteral("Control"));

        m_settingsDlg = new GaldurSettingsDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~GaldurModule() override {}

    ModuleFeatures features() const final
    {
        return ModuleFeature::SHOW_SETTINGS | ModuleFeature::CALL_UI_EVENTS;
    }

    ModuleDriverKind driver() const final
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        if (m_running)
            return;
        m_settingsDlg->updatePortList();
    }

    bool prepare(const TestSubject &) override
    {
        m_settingsDlg->setRunning(true);

        m_ctlSub.reset();
        if (m_ctlPort->hasSubscription())
            m_ctlSub = m_ctlPort->subscription();

        m_rawMessages.clear();

        return true;
    }

    void start() override {}

    void runThread(OptionalWaitCondition *waitCondition) final
    {
        // event loop for this thread
        QEventLoop loop;

        const bool startImmediately = m_settingsDlg->startImmediately();

        auto lsClient = std::make_unique<LabrstimClient>();
        lsClient->setTrialDuration(-1); // infinite trial duration
        lsClient->setMode(m_settingsDlg->mode());
        lsClient->setPulseDuration(m_settingsDlg->pulseDuration());
        lsClient->setLaserIntensity(m_settingsDlg->laserIntensity());
        lsClient->setSamplingFrequency(m_settingsDlg->samplingFrequency());
        lsClient->setRandomIntervals(m_settingsDlg->randomIntervals());
        lsClient->setMinimumInterval(m_settingsDlg->minimumInterval());
        lsClient->setMaximumInterval(m_settingsDlg->maximumInterval());
        lsClient->setSwrRefractoryTime(m_settingsDlg->swrRefractoryTime());
        lsClient->setSwrPowerThreshold(m_settingsDlg->swrPowerThreshold());
        lsClient->setConvolutionPeakThreshold(m_settingsDlg->convolutionPeakThreshold());
        lsClient->setThetaPhase(m_settingsDlg->thetaPhase());
        lsClient->setTrainFrequency(m_settingsDlg->trainFrequency());

        connect(lsClient.get(), &LabrstimClient::newRawData, [this, &loop](const QString &data) {
            const std::lock_guard<std::mutex> lock(m_rawMsgMutex);
            m_rawMessages.enqueue(data);

            // quit the loop if we stopped running
            if (!m_running)
                loop.quit();
        });

        connect(lsClient.get(), &LabrstimClient::error, [this, &loop](const QString &message) {
            raiseError(message);
            loop.quit();
        });

        if (lsClient->open(m_settingsDlg->serialPort())) {
            setStatusMessage(
                QStringLiteral("Connected to %1 (%2)").arg(m_settingsDlg->serialPort(), lsClient->clientVersion()));

            // stop, just in case a previous run did not stop properly
            lsClient->stopStimulation();
        } else {
            raiseError(QStringLiteral("Unable to connect: %1").arg(lsClient->lastError()));
            return;
        }

        // trigger if we have new input data
        std::unique_ptr<SubscriptionNotifier> notifier;
        if (m_ctlSub) {
            notifier = std::make_unique<SubscriptionNotifier>(m_ctlSub);
            connect(notifier.get(), &SubscriptionNotifier::dataReceived, [this, &loop, &lsClient]() {
                while (true) {
                    const auto maybeCtlCmd = m_ctlSub->peekNext();
                    if (!maybeCtlCmd.has_value())
                        break;
                    const auto ctlCmd = maybeCtlCmd.value();

                    if (ctlCmd.kind == ControlCommandKind::START) {
                        setStatusMessage("Stimulating...");
                        if (!lsClient->runStimulation()) {
                            raiseError(lsClient->lastError());
                            break;
                        }
                    } else if (ctlCmd.kind == ControlCommandKind::STOP) {
                        setStatusMessage("Waiting.");
                        if (!lsClient->stopStimulation()) {
                            raiseError(lsClient->lastError());
                            break;
                        }
                    }
                }

                // quit the loop if we stopped running
                if (!m_running)
                    loop.quit();
            });
        }

        // periodically check if we have to quit
        QTimer quitTimer;
        quitTimer.setInterval(200);
        quitTimer.setSingleShot(false);
        quitTimer.start();
        connect(&quitTimer, &QTimer::timeout, [&loop, this]() {
            if (!m_running)
                loop.quit();
        });

        // wait until experiment starts
        waitCondition->wait(this);

        if (startImmediately) {
            if (!lsClient->runStimulation()) {
                raiseError(lsClient->lastError());
                if (m_ctlSub)
                    m_ctlSub->disableNotify();
                return;
            }
        } else {
            setStatusMessage("Waiting for start command.");
        }

        // run our internal event loop
        loop.exec();

        if (lsClient->isRunning())
            lsClient->stopStimulation();

        if (m_ctlSub)
            m_ctlSub->disableNotify();
        lsClient->close();
        setStatusMessage("Disconnected");
    }

    void processUiEvents() override
    {
        const std::lock_guard<std::mutex> lock(m_rawMsgMutex);
        if (m_rawMessages.isEmpty())
            return;
        m_settingsDlg->addRawData(m_rawMessages.dequeue());
    }

    void stop() override
    {
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) final
    {
        settings.insert("serial_port", m_settingsDlg->serialPort());
        settings.insert("start_immediately", m_settingsDlg->startImmediately());
        settings.insert("mode", static_cast<int>(m_settingsDlg->mode()));
        settings.insert("pulse_duration", m_settingsDlg->pulseDuration());
        settings.insert("laser_intensity", m_settingsDlg->laserIntensity());
        settings.insert("sampling_frequency", m_settingsDlg->samplingFrequency());
        settings.insert("random_intervals", m_settingsDlg->randomIntervals());
        settings.insert("minimum_interval", m_settingsDlg->minimumInterval());
        settings.insert("maximum_interval", m_settingsDlg->maximumInterval());
        settings.insert("swr_refractory_time", m_settingsDlg->swrRefractoryTime());
        settings.insert("swr_power_threshold", m_settingsDlg->swrPowerThreshold());
        settings.insert("convolution_peak_threshold", m_settingsDlg->convolutionPeakThreshold());
        settings.insert("theta_phase", m_settingsDlg->thetaPhase());
        settings.insert("train_frequency", m_settingsDlg->trainFrequency());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) final
    {
        m_settingsDlg->setSerialPort(settings.value("serial_port").toString());
        m_settingsDlg->setStartImmediately(settings.value("start_immediately").toBool());
        m_settingsDlg->setMode(static_cast<LabrstimClient::Mode>(settings.value("mode").toInt()));
        m_settingsDlg->setPulseDuration(settings.value("pulse_duration").toDouble());
        m_settingsDlg->setLaserIntensity(settings.value("laser_intensity").toDouble());
        m_settingsDlg->setSamplingFrequency(settings.value("sampling_frequency").toInt());
        m_settingsDlg->setRandomIntervals(settings.value("random_intervals").toBool());
        m_settingsDlg->setMinimumInterval(settings.value("minimum_interval").toDouble());
        m_settingsDlg->setMaximumInterval(settings.value("maximum_interval").toDouble());
        m_settingsDlg->setSwrRefractoryTime(settings.value("swr_refractory_time").toDouble());
        m_settingsDlg->setSwrPowerThreshold(settings.value("swr_power_threshold").toDouble());
        m_settingsDlg->setConvolutionPeakThreshold(settings.value("convolution_peak_threshold").toDouble());
        m_settingsDlg->setThetaPhase(settings.value("theta_phase").toDouble());
        m_settingsDlg->setTrainFrequency(settings.value("train_frequency").toDouble());

        return true;
    }
};

QString GaldurModuleInfo::id() const
{
    return QStringLiteral("galdur-stim");
}

QString GaldurModuleInfo::name() const
{
    return QStringLiteral("GALDUR Stimulator");
}

QString GaldurModuleInfo::description() const
{
    return QStringLiteral("React to brain waves (theta, SWR) in real-time and emit stimulation pulses.");
}

ModuleCategories GaldurModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

QColor GaldurModuleInfo::color() const
{
    return QColor("#80002f");
}

AbstractModule *GaldurModuleInfo::createModule(QObject *parent)
{
    return new GaldurModule(parent);
}

#include "galdurmodule.moc"
