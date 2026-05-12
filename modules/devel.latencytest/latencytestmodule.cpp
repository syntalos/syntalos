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
    std::shared_ptr<StreamInputPort<LineReading>> m_lrInPort;
    std::shared_ptr<StreamSubscription<LineReading>> m_lrSub;

    std::shared_ptr<DataStream<TableRow>> m_tabStream;
    std::shared_ptr<DataStream<LineCommand>> m_lcStream;

public:
    explicit LatencyTestModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_lrInPort = registerInputPort<LineReading>(QStringLiteral("firmata-in"), QStringLiteral("Line Readings"));
        m_tabStream = registerOutputPort<TableRow>(QStringLiteral("table-out"), QStringLiteral("Table Rows"));
        m_lcStream = registerOutputPort<LineCommand>(QStringLiteral("firmata-out"), QStringLiteral("Line Control"));
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

        m_tabStream->setMetadataValue("table_header", MetaArray{"RecTime", "State", "ProcTime"});
        m_tabStream->setMetadataValue("data_name_proposal", "events/table");
        m_tabStream->start();

        m_lcStream->start();

        if (m_lrInPort->hasSubscription())
            m_lrSub = m_lrInPort->subscription();
        else
            setStateDormant();

        return true;
    }

    static constexpr uint16_t kTestInLine = 7;
    static constexpr uint16_t kTestOutLine = 8;

    void start() final
    {
        LineCommand inSetup(LineCommandKind::SetMode, kTestInLine);
        inSetup.flags = LineModeFlags::IsInput;
        m_lcStream->push(inSetup);

        LineCommand outSetup(LineCommandKind::SetMode, kTestOutLine);
        outSetup.flags = LineModeFlags::IsOutput;
        m_lcStream->push(outSetup);

        AbstractModule::start();
    }

    void runThread(OptionalWaitCondition *startWaitCondition) final
    {
        startWaitCondition->wait(this);

        // do nothing if we have no connection
        if (!m_lrSub) {
            setStateDormant();
            return;
        }

        uint32_t lastValue = 0;
        while (m_running) {
            const auto data = m_lrSub->next();
            if (!data.has_value())
                continue;
            if (lastValue == data->value)
                continue;
            if (data->lineId != kTestInLine)
                continue;

            if (data->value) {
                LineCommand ctl(LineCommandKind::WriteDigitalPulse, kTestOutLine, 1);
                ctl.duration = std::chrono::duration_cast<microseconds_t>(milliseconds_t(50));
                m_lcStream->push(ctl);
            }

            m_tabStream->push(TableRow(
                std::vector<std::string>{
                    numToString(data->time.count()),
                    numToString(data->value),
                    numToString(m_syTimer->timeSinceStartMsec().count())}));
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
