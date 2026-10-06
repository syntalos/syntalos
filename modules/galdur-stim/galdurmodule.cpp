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

#include "streams/subscriptionnotifier.h"

#include "labrstimclient.h"
#include "galdursettingsdialog.h"

SYNTALOS_MODULE(GaldurModule)

class GaldurModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;

    GaldurSettingsDialog *m_settingsDlg;

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
        return ModuleFeature::SHOW_SETTINGS;
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

    bool prepare(const RunInfo &) override
    {
        m_settingsDlg->setRunning(true);

        std::shared_ptr<StreamSubscription<ControlCommand>> ctlSub;
        if (m_ctlPort->hasSubscription())
            ctlSub = m_ctlPort->subscription();

        setWorker(
            Worker{
                .ctlSub = ctlSub,
                .showRawData = mainCallback([this](const QString &data) {
                    m_settingsDlg->addRawData(data);
                }),
                .serialPort = m_settingsDlg->serialPort(),
                .startImmediately = m_settingsDlg->startImmediately(),
                .mode = m_settingsDlg->mode(),
                .pulseDuration = m_settingsDlg->pulseDuration(),
                .laserIntensity = m_settingsDlg->laserIntensity(),
                .samplingFrequency = m_settingsDlg->samplingFrequency(),
                .randomIntervals = m_settingsDlg->randomIntervals(),
                .minimumInterval = m_settingsDlg->minimumInterval(),
                .maximumInterval = m_settingsDlg->maximumInterval(),
                .swrRefractoryTime = m_settingsDlg->swrRefractoryTime(),
                .swrPowerThreshold = m_settingsDlg->swrPowerThreshold(),
                .convolutionPeakThreshold = m_settingsDlg->convolutionPeakThreshold(),
                .thetaPhase = m_settingsDlg->thetaPhase(),
                .trainFrequency = m_settingsDlg->trainFrequency(),
                .spikeDetectionWindow = m_settingsDlg->spikeDetectionWindow(),
                .spikeTriggerFrequency = m_settingsDlg->spikeTriggerFrequency(),
                .spikeStimCooldownTime = m_settingsDlg->spikeStimCooldownTime(),
                .spikeThresholdValue = m_settingsDlg->spikeThresholdValue(),
            });

        return true;
    }

    void start() override {}

    /**
     * Controls the stimulation device, in a dedicated thread with an event loop of its own.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<StreamSubscription<ControlCommand>> ctlSub;
        MainCallback<QString> showRawData;

        // settings, as selected by the user when the run was prepared
        QString serialPort;
        bool startImmediately;
        LabrstimClient::Mode mode;
        double pulseDuration;
        double laserIntensity;
        int samplingFrequency;
        bool randomIntervals;
        double minimumInterval;
        double maximumInterval;
        double swrRefractoryTime;
        double swrPowerThreshold;
        double convolutionPeakThreshold;
        double thetaPhase;
        double trainFrequency;
        uint spikeDetectionWindow;
        uint spikeTriggerFrequency;
        uint spikeStimCooldownTime;
        int spikeThresholdValue;

        void run()
        {
            // event loop for this thread
            QEventLoop loop;

            auto lsClient = std::make_unique<LabrstimClient>();
            lsClient->setTrialDuration(-1); // infinite trial duration
            lsClient->setMode(mode);
            lsClient->setPulseDuration(pulseDuration);
            lsClient->setLaserIntensity(laserIntensity);
            lsClient->setSamplingFrequency(samplingFrequency);
            lsClient->setRandomIntervals(randomIntervals);
            lsClient->setMinimumInterval(minimumInterval);
            lsClient->setMaximumInterval(maximumInterval);
            lsClient->setSwrRefractoryTime(swrRefractoryTime);
            lsClient->setSwrPowerThreshold(swrPowerThreshold);
            lsClient->setConvolutionPeakThreshold(convolutionPeakThreshold);
            lsClient->setThetaPhase(thetaPhase);
            lsClient->setTrainFrequency(trainFrequency);

            lsClient->setSpikeDetectionWindow(spikeDetectionWindow);
            lsClient->setSpikeTriggerFrequency(spikeTriggerFrequency);
            lsClient->setSpikeStimCooldownTime(spikeStimCooldownTime);
            lsClient->setSpikeThresholdValue(spikeThresholdValue);

            QObject::connect(lsClient.get(), &LabrstimClient::newRawData, [this, &loop](const QString &data) {
                // have the main thread display the message
                showRawData(data);

                // quit the loop if we stopped running
                if (!mod.running())
                    loop.quit();
            });

            QObject::connect(lsClient.get(), &LabrstimClient::error, [this, &loop](const QString &message) {
                mod.raiseError(message);
                loop.quit();
            });

            if (lsClient->open(serialPort)) {
                mod.setStatusMessage(QStringLiteral("Connected to %1 (%2)").arg(serialPort, lsClient->clientVersion()));

                // stop, just in case a previous run did not stop properly
                lsClient->stopStimulation();
            } else {
                mod.raiseError(QStringLiteral("Unable to connect: %1").arg(lsClient->lastError()));
                return;
            }

            // trigger if we have new input data
            std::unique_ptr<SubscriptionNotifier> notifier;
            if (ctlSub) {
                notifier = std::make_unique<SubscriptionNotifier>(ctlSub);
                QObject::connect(notifier.get(), &SubscriptionNotifier::dataReceived, [this, &loop, &lsClient]() {
                    while (true) {
                        const auto maybeCtlCmd = ctlSub->peekNext();
                        if (!maybeCtlCmd.has_value())
                            break;
                        const auto &ctlCmd = maybeCtlCmd.value();

                        if (ctlCmd.kind == ControlCommandKind::START) {
                            mod.setStatusMessage("Stimulating...");
                            if (!lsClient->runStimulation()) {
                                mod.raiseError(lsClient->lastError());
                                break;
                            }
                        } else if (ctlCmd.kind == ControlCommandKind::STOP) {
                            mod.setStatusMessage("Waiting.");
                            if (!lsClient->stopStimulation()) {
                                mod.raiseError(lsClient->lastError());
                                break;
                            }
                        }
                    }

                    // quit the loop if we stopped running
                    if (!mod.running())
                        loop.quit();
                });
            }

            // periodically check if we have to quit
            QTimer quitTimer;
            quitTimer.setInterval(200);
            quitTimer.setSingleShot(false);
            quitTimer.start();
            QObject::connect(&quitTimer, &QTimer::timeout, [&loop, this]() {
                if (!mod.running())
                    loop.quit();
            });

            // wait until experiment starts
            mod.waitForStart();

            if (startImmediately) {
                if (!lsClient->runStimulation()) {
                    mod.raiseError(lsClient->lastError());
                    return;
                }
            } else {
                mod.setStatusMessage("Waiting for start command.");
            }

            // run our internal event loop
            loop.exec();

            if (lsClient->isRunning())
                lsClient->stopStimulation();

            lsClient->close();
            mod.setStatusMessage("Disconnected");
        }
    };

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

        settings.insert("spike_detection_window", m_settingsDlg->spikeDetectionWindow());
        settings.insert("spike_trigger_frequency", m_settingsDlg->spikeTriggerFrequency());
        settings.insert("spike_stim_cooldown_time", m_settingsDlg->spikeStimCooldownTime());
        settings.insert("spike_threshold_value", m_settingsDlg->spikeThresholdValue());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) final
    {
        m_settingsDlg->setMode(static_cast<LabrstimClient::Mode>(settings.value("mode").toInt()));
        m_settingsDlg->setSerialPort(settings.value("serial_port").toString());
        m_settingsDlg->setStartImmediately(settings.value("start_immediately").toBool());
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

        m_settingsDlg->setSpikeDetectionWindow(settings.value("spike_detection_window").toUInt());
        m_settingsDlg->setSpikeTriggerFrequency(settings.value("spike_trigger_frequency").toUInt());
        m_settingsDlg->setSpikeStimCooldownTime(settings.value("spike_stim_cooldown_time").toUInt());
        m_settingsDlg->setSpikeThresholdValue(settings.value("spike_threshold_value").toInt());

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
