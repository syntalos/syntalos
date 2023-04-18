/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <time.h>
#include <glib.h>

#include "clocksettingsdialog.h"
#include "tsyncfile.h"

#define NSEC_IN_SEC 1000000000

SYNTALOS_MODULE(DevelClockModule)

static inline
struct timespec timespecAdd(const struct timespec &t1, const struct timespec &t2)
{
    long sec = t2.tv_sec + t1.tv_sec;
    long nsec = t2.tv_nsec + t1.tv_nsec;
    if (nsec >= NSEC_IN_SEC) {
        nsec -= NSEC_IN_SEC;
        sec++;
    }
    return (struct timespec){ .tv_sec = sec, .tv_nsec = nsec };
}

class ClockModule : public AbstractModule
{
private:
    std::shared_ptr<DataStream<ControlCommand>> m_ctlOut;
    std::shared_ptr<DataStream<TableRow>> m_tabOut;

    ClockSettingsDialog *m_settingsDlg;
    TimeSyncFileWriter m_tsWriter;

    struct timespec m_interval;
    bool m_stopped;

public:
    explicit ClockModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_ctlOut = registerOutputPort<ControlCommand>(QStringLiteral("pulse-out"), QStringLiteral("Pulses"));
        m_tabOut = registerOutputPort<TableRow>(QStringLiteral("table-out"), QStringLiteral("Time Rows"));

        m_settingsDlg = new ClockSettingsDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~ClockModule() override
    {

    }

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

    bool prepare(const TestSubject &) override
    {
        m_stopped = true;
        m_tabOut->setSuggestedDataName(QStringLiteral("table-%1/time-pulses").arg(datasetNameSuggestion()));
        m_tabOut->setMetadataValue("table_header", QStringList() << QStringLiteral("Time (µs)"));

        // start the streams
        m_ctlOut->start();
        m_tabOut->start();

        // set nanosleep request
        const long long interval_ns = m_settingsDlg->pulseIntervalUs() * 1000;
        m_interval.tv_sec = interval_ns / NSEC_IN_SEC;
        m_interval.tv_nsec = interval_ns % NSEC_IN_SEC;

        // prepare pulse info writer
        m_tsWriter.close();
        m_tsWriter.setSyncMode(TSyncFileMode::CONTINUOUS);
        m_tsWriter.setTimeNames(QStringLiteral("no"), QStringLiteral("master-time"));
        m_tsWriter.setTimeUnits(TSyncFileTimeUnit::INDEX, TSyncFileTimeUnit::MICROSECONDS);
        m_tsWriter.setTimeDataTypes(TSyncFileDataType::UINT32, TSyncFileDataType::UINT64);
        m_tsWriter.setChunkSize((m_settingsDlg->pulseIntervalUs() / 1000 / 1000) * 60 * 2); // new chunk about every 2min

        // prepare dataset
        auto dstore = getOrCreateDefaultDataset(name());
        m_tsWriter.setFileName(dstore->setDataFile("time-pulses.tsync"));
        QVariantHash userData;
        userData["interval_us"] = QVariant::fromValue(m_settingsDlg->pulseIntervalUs());

        // open writer
        if (!m_tsWriter.open(name(), dstore->collectionId(), userData)) {
            raiseError(QStringLiteral("Unable to open timesync file %1").arg(m_tsWriter.fileName()));
            return false;
        }

        setStateReady();
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        struct timespec ts;
        int r;
        long index;

        ControlCommand cmd;
        cmd.kind = ControlCommandKind::STEP;

        QStringList row;
        row.append(QString());

        startWaitCondition->wait(this);

        r = clock_gettime(CLOCK_MONOTONIC, &ts);
        if (G_UNLIKELY(r != 0)) {
            m_running = false;
            raiseError(QStringLiteral("Unable to obtain initial monotonic clock time: %1").arg(QString::fromStdString(std::strerror(errno))));
            return;
        }
        ts = timespecAdd(ts, m_interval);

        m_stopped = false;
        index = 0;
        while (m_running) {
            r = clock_nanosleep(CLOCK_MONOTONIC,
                                TIMER_ABSTIME,
                                &ts,
                                nullptr);
            if (G_UNLIKELY(r == -EINTR)) {
                r = clock_gettime(CLOCK_MONOTONIC, &ts);
                if (G_UNLIKELY(r != 0)) {
                    m_running = false;
                    raiseError(QStringLiteral("Unable to obtain monotonic clock time: %1").arg(QString::fromStdString(std::strerror(errno))));
                    break;
                }
                continue;
            }
            if (G_UNLIKELY(r != 0)) {
                m_running = false;
                raiseError(QStringLiteral("Unable to nanosleep: %1").arg(QString::fromStdString(std::strerror(errno))));
                break;
            }
            r = clock_gettime(CLOCK_MONOTONIC, &ts);
            if (G_UNLIKELY(r != 0)) {
                m_running = false;
                raiseError(QStringLiteral("Unable to obtain monotonic clock time: %1").arg(QString::fromStdString(std::strerror(errno))));
                break;
            }

            // set future expected clock time
            ts = timespecAdd(ts, m_interval);

            const auto tsUsec = m_syTimer->timeSinceStartUsec().count();

            m_ctlOut->push(cmd);
            row[0] = QString::number(tsUsec);
            m_tabOut->push(row);
            m_tsWriter.writeTimes(++index, tsUsec);
        }

        m_stopped = true;
    }

    void stop() override
    {
        m_running = false;
        while (!m_stopped) { usleep(1000); }
        m_tsWriter.close();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("high_priority"), m_settingsDlg->highPriorityThread());
        settings.insert(QStringLiteral("interval_us"), QVariant::fromValue(m_settingsDlg->pulseIntervalUs()));
    }

    bool loadSettings(const QString&, const QVariantHash &settings, const QByteArray& ) override
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

bool DevelClockModuleInfo::devel() const
{
    return true;
}

AbstractModule *DevelClockModuleInfo::createModule(QObject *parent)
{
    return new ClockModule(parent);
}
