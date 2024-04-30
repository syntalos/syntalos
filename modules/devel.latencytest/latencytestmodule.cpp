/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "latencytestmodule.h"

#include "utils/misc.h"

SYNTALOS_MODULE(DevelLatencyTestModule)

class LatencyTestModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<FirmataData>> m_fmDataInPort;
    std::shared_ptr<StreamSubscription<FirmataData>> m_fmDataSub;

    std::shared_ptr<DataStream<TableRow>> m_tabStream;
    std::shared_ptr<DataStream<FirmataControl>> m_fmCtlStream;

public:
    explicit LatencyTestModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_fmDataInPort = registerInputPort<FirmataData>(QStringLiteral("firmata-in"), QStringLiteral("Firmata Data"));
        m_tabStream = registerOutputPort<TableRow>(QStringLiteral("table-out"), QStringLiteral("Table Rows"));
        m_fmCtlStream = registerOutputPort<FirmataControl>(
            QStringLiteral("firmata-out"), QStringLiteral("Firmata Control"));
    }

    ~LatencyTestModule() override {}

    ModuleDriverKind driver() const final
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const final
    {
        return ModuleFeature::NONE;
    }

    bool prepare(const TestSubject &) final
    {
        m_tabStream->setMetadataValue(
            "table_header",
            QStringList() << "RecTime"
                          << "State"
                          << "ProcTime");
        m_tabStream->setMetadataValue("data_name_proposal", "events/table");
        m_tabStream->start();

        m_fmCtlStream->start();

        if (m_fmDataInPort->hasSubscription())
            m_fmDataSub = m_fmDataInPort->subscription();
        else
            setStateDormant();

        return true;
    }

    void start() final
    {
        auto ctl = FirmataControl(FirmataCommandKind::NEW_DIG_PIN, 7, "testIn");
        ctl.isOutput = false;
        m_fmCtlStream->push(ctl);

        ctl = FirmataControl(FirmataCommandKind::NEW_DIG_PIN, 8, "testOut");
        ctl.isOutput = true;
        m_fmCtlStream->push(ctl);

        AbstractModule::start();
    }

    void runThread(OptionalWaitCondition *startWaitCondition) final
    {
        startWaitCondition->wait(this);

        uint16_t lastValue = 0;
        while (m_running) {
            const auto data = m_fmDataSub->next();
            if (!data.has_value())
                continue;
            if (!data->isDigital)
                continue;
            if (lastValue == data->value)
                continue;
            if (data->pinName != QStringLiteral("testIn"))
                continue;

            if (data->value) {
                auto ctl = FirmataControl(FirmataCommandKind::WRITE_DIGITAL_PULSE, "testOut");
                m_fmCtlStream->push(ctl);
            }

            m_tabStream->push(TableRow(
                QStringList() << QString::number(data->time.count()) << QString::number(data->value)
                              << QString::number(m_syTimer->timeSinceStartMsec().count())));
            lastValue = data->value;
        }
    }
};

QString DevelLatencyTestModuleInfo::id() const
{
    return QStringLiteral("devel.latencycheck");
}

QString DevelLatencyTestModuleInfo::name() const
{
    return QStringLiteral("Devel: LatencyTest");
}

QString DevelLatencyTestModuleInfo::description() const
{
    return QStringLiteral("A very simple hardware latency test using a pure C++ module.");
}

QIcon DevelLatencyTestModuleInfo::icon() const
{
    return QIcon(":/module/devel");
}

ModuleCategories DevelLatencyTestModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV;
}

AbstractModule *DevelLatencyTestModuleInfo::createModule(QObject *parent)
{
    return new LatencyTestModule(parent);
}

#include "latencytestmodule.moc"
