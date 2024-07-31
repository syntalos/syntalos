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

#include "labrstimclient.h"
#include "galdursettingsdialog.h"

SYNTALOS_MODULE(GaldurModule)

class GaldurModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlPort;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;

    LabrstimClient *m_lsClient;
    GaldurSettingsDialog *m_settingsDlg;

public:
    explicit GaldurModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_ctlPort = registerInputPort<ControlCommand>(QStringLiteral("control-in"), QStringLiteral("Control"));

        m_lsClient = new LabrstimClient();
        m_settingsDlg = new GaldurSettingsDialog(m_lsClient);
        addSettingsWindow(m_settingsDlg);
    }

    ~GaldurModule() override
    {
        delete m_lsClient;
    }

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

    bool prepare(const TestSubject &) override
    {
        m_settingsDlg->setRunning(true);

        m_ctlSub.reset();
        if (m_ctlPort->hasSubscription())
            m_ctlSub = m_ctlPort->subscription();

        return true;
    }

    void start() override {}

    void runThread(OptionalWaitCondition *waitCondition) final
    {
        const bool startImmediately = m_settingsDlg->startImmediately();

        if (m_lsClient->open(m_settingsDlg->serialPort())) {
            setStatusMessage(QStringLiteral("Connected to %1 (%2)")
                                 .arg(m_settingsDlg->serialPort())
                                 .arg(m_lsClient->clientVersion()));
        } else {
            raiseError(QStringLiteral("Unable to connect: %1").arg(m_lsClient->lastError()));
            return;
        }

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        if (startImmediately && !m_lsClient->runStimulation()) {
            raiseError(m_lsClient->lastError());
            return;
        }

        if (m_ctlSub) {
            while (m_running) {
                const auto ctlCmd = m_ctlSub->next();
                if (!ctlCmd.has_value())
                    break; // we can quit here, a nullopt means we should terminate

                if (ctlCmd->kind == ControlCommandKind::START) {
                    if (!m_lsClient->runStimulation()) {
                        raiseError(m_lsClient->lastError());
                        break;
                    }
                } else if (ctlCmd->kind == ControlCommandKind::STOP) {
                    if (!m_lsClient->stopStimulation()) {
                        raiseError(m_lsClient->lastError());
                        break;
                    }
                }
            }
        } else {
            while (m_running)
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        m_lsClient->close();
        setStatusMessage("Disconnected");
    }

    void stop() override
    {
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override {}

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {

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
