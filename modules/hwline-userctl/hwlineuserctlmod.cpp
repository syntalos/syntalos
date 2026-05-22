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

#include "hwlineuserctlmod.h"

#include "hwlinectldialog.h"
#include <QTimer>

SYNTALOS_MODULE(HWLineUserCtlModule)

class HWLineUserCtlModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort<LineReading>> m_fmInPort;
    std::shared_ptr<DataStream<LineCommand>> m_fmCtlStream;
    HWLineCtlDialog *m_ctlDialog;
    QTimer *m_evTimer;
    std::shared_ptr<StreamSubscription<LineReading>> m_fmInSub;

public:
    explicit HWLineUserCtlModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_fmInPort = registerInputPort<LineReading>(QStringLiteral("hwline-in"), QStringLiteral("Line Readings"));
        m_fmCtlStream = registerOutputPort<LineCommand>(QStringLiteral("hwline-out"), QStringLiteral("Line Control"));
        m_ctlDialog = new HWLineCtlDialog(m_fmCtlStream);
        addDisplayWindow(m_ctlDialog);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(50); // we only fetch new values every 50msec
        connect(m_evTimer, &QTimer::timeout, this, &HWLineUserCtlModule::readHWLineEvents);
    }

    ~HWLineUserCtlModule() override = default;

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

    void readHWLineEvents()
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

QString HWLineUserCtlModuleInfo::id() const
{
    return QStringLiteral("hwline-userctl");
}

QString HWLineUserCtlModuleInfo::name() const
{
    return QStringLiteral("Manual Line Control");
}

QString HWLineUserCtlModuleInfo::description() const
{
    return QStringLiteral("A simple control panel to manually drive hardware-line outputs and view input readings.");
}

ModuleCategories HWLineUserCtlModuleInfo::categories() const
{
    return ModuleCategory::SCRIPTING | ModuleCategory::PROCESSING;
}

AbstractModule *HWLineUserCtlModuleInfo::createModule(QObject *parent)
{
    return new HWLineUserCtlModule(parent);
}
