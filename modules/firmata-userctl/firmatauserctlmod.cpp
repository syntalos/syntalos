/*
 * Copyright (C) 2010-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "firmatauserctlmod.h"

#include "firmatactldialog.h"
#include <QTimer>

SYNTALOS_MODULE(FirmataUserCtlModule)

class FirmataUserCtlModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort<FirmataData>> m_fmInPort;
    std::shared_ptr<DataStream<FirmataControl>> m_fmCtlStream;
    FirmataCtlDialog *m_ctlDialog;
    QTimer *m_evTimer;
    std::shared_ptr<StreamSubscription<FirmataData>> m_fmInSub;

public:
    explicit FirmataUserCtlModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_fmInPort = registerInputPort<FirmataData>(QStringLiteral("firmata-in"), QStringLiteral("Firmata Input"));
        m_fmCtlStream = registerOutputPort<FirmataControl>(
            QStringLiteral("firmata-out"), QStringLiteral("Firmata Control"));
        m_ctlDialog = new FirmataCtlDialog(m_fmCtlStream);
        addDisplayWindow(m_ctlDialog);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(50); // we only fetch new values every 50msec
        connect(m_evTimer, &QTimer::timeout, this, &FirmataUserCtlModule::readFirmataEvents);
    }

    ~FirmataUserCtlModule() override {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    bool prepare(const TestSubject &) override
    {
        m_fmCtlStream->start();
        return true;
    }

    void start() override
    {
        QTimer::singleShot(1500, [&] {
            m_ctlDialog->initializeAllPins();
        });

        // we only need to read data if we have an input subscription
        if (m_fmInPort->hasSubscription()) {
            m_fmInSub = m_fmInPort->subscription();
            m_evTimer->start();
        }
    }

    void stop() override
    {
        m_evTimer->stop();
        m_fmCtlStream->stop();
    }

    void readFirmataEvents()
    {
        const auto maybeData = m_fmInSub->peekNext();
        if (!maybeData.has_value())
            return;
        m_ctlDialog->pinValueChanged(maybeData.value());
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings = m_ctlDialog->serializeSettings();
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_ctlDialog->restoreFromSettings(settings);
        return true;
    }

private:
};

QString FirmataUserCtlModuleInfo::id() const
{
    return QStringLiteral("firmata-userctl");
}

QString FirmataUserCtlModuleInfo::name() const
{
    return QStringLiteral("Firmata User Control");
}

QString FirmataUserCtlModuleInfo::description() const
{
    return QStringLiteral("A simple control panel to manually change Firmata output and view raw input data.");
}

AbstractModule *FirmataUserCtlModuleInfo::createModule(QObject *parent)
{
    return new FirmataUserCtlModule(parent);
}
