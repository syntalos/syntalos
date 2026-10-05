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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clockmodule.h"

#include <glib.h>
#include <time.h>

#include "clocksettingsdialog.h"
#include "datactl/tsyncfile.h"
#include "utils/misc.h"

#define NSEC_IN_SEC 1000000000

SYNTALOS_MODULE(DevelClockModule)

static inline struct timespec timespecAdd(const struct timespec &t1, const struct timespec &t2)
{
    long sec = t2.tv_sec + t1.tv_sec;
    long nsec = t2.tv_nsec + t1.tv_nsec;
    if (nsec >= NSEC_IN_SEC) {
        nsec -= NSEC_IN_SEC;
        sec++;
    }
    return (struct timespec){.tv_sec = sec, .tv_nsec = nsec};
}

class ClockModule : public AbstractModule
{
private:
    std::shared_ptr<DataStream<ControlCommand>> m_ctlOut;
    std::shared_ptr<DataStream<TableRow>> m_tabOut;

    ClockSettingsDialog *m_settingsDlg;

public:
    explicit ClockModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_ctlOut = registerOutputPort<ControlCommand>(QStringLiteral("pulse-out"), QStringLiteral("Pulses"));
        m_tabOut = registerOutputPort<TableRow>(QStringLiteral("table-out"), QStringLiteral("Time Rows"));

        m_settingsDlg = new ClockSettingsDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~ClockModule() override {}

    ModuleFeatures features() const override
    {
        ModuleFeatures flags = ModuleFeature::SHOW_SETTINGS;

        if (m_settingsDlg->highPriorityThread())
            flags |= ModuleFeature::REQUEST_CPU_AFFINITY | ModuleFeature::REALTIME;
        return flags;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool prepare(const RunInfo &) override
    {
        m_tabOut->setSuggestedDataName(QStringLiteral("table-%1/time-pulses").arg(datasetNameSuggestion()));
        m_tabOut->setMetadataValue("table_header", MetaArray{"Time (µs)"});

        // start the streams
        m_ctlOut->start();
        m_tabOut->start();

        // set nanosleep request
        const long long interval_ns = m_settingsDlg->pulseIntervalUs() * 1000;
        struct timespec interval;
        interval.tv_sec = interval_ns / NSEC_IN_SEC;
        interval.tv_nsec = interval_ns % NSEC_IN_SEC;

        // prepare pulse info writer
        auto tsWriter = std::make_unique<TimeSyncFileWriter>();
        tsWriter->setSyncMode(TSyncFileMode::CONTINUOUS);
        tsWriter->setTimeNames("no", "master-time");
        tsWriter->setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
        tsWriter->setTimeDataTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64);
        tsWriter->setChunkSize((m_settingsDlg->pulseIntervalUs() / 1000 / 1000) * 60 * 2); // new chunk about every 2min

        // prepare dataset
        auto dstore = createDefaultDataset(name());
        if (dstore.get() == nullptr)
            return false;
        tsWriter->setFileName(dstore->setDataFile("time-pulses.tsync"));
        MetaStringMap userData;
        userData["interval_us"] = m_settingsDlg->pulseIntervalUs();

        // open writer
        if (!tsWriter->open(name().toStdString(), dstore->collectionId(), userData)) {
            raiseError(std::format("Unable to open timesync file {}", tsWriter->fileName()));
            return false;
        }

        setWorker(
            Worker{
                .interval = interval,
                .ctlOut = m_ctlOut,
                .tabOut = m_tabOut,
                .tsWriter = std::move(tsWriter),
            });

        setStateReady();
        return true;
    }

    struct Worker {
        WorkerContext mod{};
        struct timespec interval;
        std::shared_ptr<DataStream<ControlCommand>> ctlOut;
        std::shared_ptr<DataStream<TableRow>> tabOut;
        std::unique_ptr<TimeSyncFileWriter> tsWriter;

        std::expected<void, std::string> run()
        {
            struct timespec ts;
            int r;
            long index;

            ControlCommand cmd;
            cmd.kind = ControlCommandKind::STEP;

            std::vector<std::string> row;
            row.push_back(std::string());

            mod.waitForStart();

            r = clock_gettime(CLOCK_MONOTONIC, &ts);
            if (G_UNLIKELY(r != 0))
                return std::unexpected(
                    std::format("Unable to obtain initial monotonic clock time: {}", std::strerror(errno)));
            ts = timespecAdd(ts, interval);

            index = 0;
            while (mod.running()) {
                r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
                if (G_UNLIKELY(r == -EINTR)) {
                    r = clock_gettime(CLOCK_MONOTONIC, &ts);
                    if (G_UNLIKELY(r != 0))
                        return std::unexpected(
                            std::format("Unable to obtain monotonic clock time: {}", std::strerror(errno)));
                    continue;
                }
                if (G_UNLIKELY(r != 0))
                    return std::unexpected(std::format("Unable to nanosleep: {}", std::strerror(errno)));
                r = clock_gettime(CLOCK_MONOTONIC, &ts);
                if (G_UNLIKELY(r != 0))
                    return std::unexpected(
                        std::format("Unable to obtain monotonic clock time: {}", std::strerror(errno)));

                // set future expected clock time
                ts = timespecAdd(ts, interval);

                const auto tsUsec = mod.timer->timeSinceStartUsec().count();

                ctlOut->push(cmd);
                row[0] = numToString(tsUsec);
                tabOut->push(TableRow(row));
                tsWriter->writeTimes(++index, tsUsec);
            }

            return {};
        }
    };

    void stop() override
    {
        // our thread has finished at this point, so the file can be completed
        if (auto worker = takeWorker<Worker>())
            worker->tsWriter->close();
        AbstractModule::stop();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("high_priority"), m_settingsDlg->highPriorityThread());
        settings.insert(QStringLiteral("interval_us"), QVariant::fromValue(m_settingsDlg->pulseIntervalUs()));
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setHighPriorityThread(settings.value(QStringLiteral("high_priority"), false).toBool());
        m_settingsDlg->setPulseIntervalUs(settings.value(QStringLiteral("interval_us"), 500 * 1000).toLongLong());
        return true;
    }

private:
};

QString DevelClockModuleInfo::id() const
{
    return QStringLiteral("devel.clock");
}

QString DevelClockModuleInfo::name() const
{
    return QStringLiteral("Devel: Clock");
}

QString DevelClockModuleInfo::description() const
{
    return QStringLiteral("Developer module emiting clock pulses at precise (as much as possible) intervals.");
}

ModuleCategories DevelClockModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV;
}

AbstractModule *DevelClockModuleInfo::createModule(QObject *parent)
{
    return new ClockModule(parent);
}
