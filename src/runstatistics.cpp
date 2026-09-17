/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "runstatistics.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "utils/style.h"

using namespace Syntalos;

static QJsonObject usageToJson(const ThreadUsageStats &u)
{
    QJsonObject o;
    o.insert(QStringLiteral("tid"), static_cast<qint64>(u.tid));
    o.insert(QStringLiteral("user_time_sec"), u.userTimeSec);
    o.insert(QStringLiteral("system_time_sec"), u.systemTimeSec);
    o.insert(QStringLiteral("voluntary_ctx_switches"), static_cast<qint64>(u.voluntaryCtxSwitches));
    o.insert(QStringLiteral("involuntary_ctx_switches"), static_cast<qint64>(u.involuntaryCtxSwitches));
    o.insert(QStringLiteral("minor_page_faults"), static_cast<qint64>(u.minorPageFaults));
    o.insert(QStringLiteral("major_page_faults"), static_cast<qint64>(u.majorPageFaults));
    return o;
}

static QString heatToString(ConnectionHeatLevel heat)
{
    switch (heat) {
    case ConnectionHeatLevel::NONE:
        return QStringLiteral("none");
    case ConnectionHeatLevel::LOW:
        return QStringLiteral("low");
    case ConnectionHeatLevel::MEDIUM:
        return QStringLiteral("medium");
    case ConnectionHeatLevel::HIGH:
        return QStringLiteral("high");
    }
    return QStringLiteral("unknown");
}

/**
 * Whether a priority elevation took effect is only reported if one was requested (and we know the outcome).
 */
static void insertPriorityApplied(
    QJsonObject &o,
    bool realtimeRequested,
    int niceness,
    const std::optional<bool> &applied)
{
    if (!applied.has_value())
        return;
    if (realtimeRequested)
        o.insert(QStringLiteral("realtime_applied"), applied.value());
    else if (niceness != 0)
        o.insert(QStringLiteral("niceness_applied"), applied.value());
}

QJsonObject RunStatistics::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format_version"), FormatVersion);

    QJsonObject run;
    run.insert(QStringLiteral("id"), runId);
    run.insert(QStringLiteral("experiment_id"), experimentId);
    run.insert(QStringLiteral("started"), started.toString(Qt::ISODateWithMs));
    run.insert(QStringLiteral("duration_sec"), durationSec);
    run.insert(QStringLiteral("usage_window_sec"), usageWindowSec);
    run.insert(QStringLiteral("ephemeral"), ephemeral);
    run.insert(QStringLiteral("success"), !failed);
    if (failed)
        run.insert(QStringLiteral("failure_reason"), failReason);
    run.insert(QStringLiteral("export_dir"), exportDir);
    run.insert(QStringLiteral("bytes_written"), bytesWritten);
    run.insert(QStringLiteral("cpu_cores"), cpuCoreCount);
    run.insert(QStringLiteral("priority_budget"), static_cast<int>(priorityBudget));
    run.insert(QStringLiteral("threads_total"), threadsTotal);
    run.insert(QStringLiteral("threads_elevated"), threadsElevated);
    if (mainThread)
        run.insert(QStringLiteral("main_thread"), usageToJson(*mainThread));
    if (process)
        run.insert(QStringLiteral("process"), usageToJson(*process));
    run.insert(QStringLiteral("peak_rss_kib"), peakRssKiB);
    root.insert(QStringLiteral("run"), run);

    QJsonArray mods;
    for (const auto &m : modules) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), m.id);
        o.insert(QStringLiteral("name"), m.name);
        o.insert(QStringLiteral("driver"), driverKindToString(m.driver));
        o.insert(QStringLiteral("final_state"), QString::fromStdString(toString(m.finalState)));
        o.insert(QStringLiteral("out_of_process"), m.outOfProcess);
        if (!m.eventThreadKey.isEmpty())
            o.insert(QStringLiteral("event_thread"), m.eventThreadKey);
        if (!m.errorMessage.isEmpty())
            o.insert(QStringLiteral("error"), m.errorMessage);
        o.insert(QStringLiteral("realtime_requested"), m.realtimeRequested);
        o.insert(QStringLiteral("niceness"), m.niceness);
        insertPriorityApplied(o, m.realtimeRequested, m.niceness, m.priorityApplied);
        if (!m.cpuAffinity.empty()) {
            QJsonArray aff;
            for (const auto c : m.cpuAffinity)
                aff.append(static_cast<int>(c));
            o.insert(QStringLiteral("cpu_affinity"), aff);
        }
        if (m.thread)
            o.insert(QStringLiteral("thread"), usageToJson(*m.thread));
        if (m.workerPid > 0) {
            o.insert(QStringLiteral("worker_pid"), m.workerPid);
            if (m.worker)
                o.insert(QStringLiteral("worker"), usageToJson(*m.worker));
        }
        mods.append(o);
    }
    root.insert(QStringLiteral("modules"), mods);

    QJsonArray evts;
    for (const auto &e : eventThreads) {
        QJsonObject o;
        o.insert(QStringLiteral("key"), e.key);
        o.insert(QStringLiteral("modules"), QJsonArray::fromStringList(e.moduleNames));
        o.insert(QStringLiteral("realtime_requested"), e.realtimeRequested);
        o.insert(QStringLiteral("niceness"), e.niceness);
        insertPriorityApplied(o, e.realtimeRequested, e.niceness, e.priorityApplied);
        if (e.thread)
            o.insert(QStringLiteral("thread"), usageToJson(*e.thread));
        evts.append(o);
    }
    root.insert(QStringLiteral("event_threads"), evts);

    QJsonArray conns;
    for (const auto &c : connections) {
        QJsonObject o;
        o.insert(QStringLiteral("src_module"), c.srcModule);
        o.insert(QStringLiteral("src_port"), c.srcPort);
        o.insert(QStringLiteral("dst_module"), c.dstModule);
        o.insert(QStringLiteral("dst_port"), c.dstPort);
        o.insert(QStringLiteral("data_type"), c.dataType);
        o.insert(QStringLiteral("direct_ipc"), c.directIpc);
        if (!c.directIpc) {
            o.insert(QStringLiteral("items_received"), static_cast<qint64>(c.itemsReceived));
            o.insert(QStringLiteral("peak_pending"), static_cast<qint64>(c.peakPending));
            o.insert(QStringLiteral("pending_at_stop"), static_cast<qint64>(c.pendingAtStop));
            o.insert(QStringLiteral("max_heat"), heatToString(c.maxHeat));
        }
        conns.append(o);
    }
    root.insert(QStringLiteral("connections"), conns);

    QJsonObject sys;
    for (auto it = system.constBegin(); it != system.constEnd(); ++it)
        sys.insert(it.key(), it.value());
    root.insert(QStringLiteral("system"), sys);

    return root;
}

QByteArray RunStatistics::toJsonData() const
{
    return QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
}

bool RunStatistics::saveJson(const QString &path, QString *errorMessage) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = f.errorString();
        return false;
    }
    f.write(toJsonData());
    if (!f.commit()) {
        if (errorMessage)
            *errorMessage = f.errorString();
        return false;
    }
    return true;
}

static QString fmtSec(double sec)
{
    return QStringLiteral("%1 s").arg(sec, 0, 'f', 2);
}

static QString fmtBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("unknown");
    if (bytes < 1000 * 1000)
        return QStringLiteral("%1 kB").arg(static_cast<double>(bytes) / 1000.0, 0, 'f', 1);
    if (bytes < 1000LL * 1000 * 1000)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1000.0 * 1000.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1000.0 * 1000.0 * 1000.0), 0, 'f', 2);
}

/**
 * CPU load of a thread as a percentage of one core over the measurement window.
 */
static QString fmtLoad(const ThreadUsageStats &u, double windowSec)
{
    if (windowSec <= 0)
        return QStringLiteral("–");
    return QStringLiteral("%1 %").arg(100.0 * u.cpuTimeSec() / windowSec, 0, 'f', 1);
}

static QString fmtUsageCells(const std::optional<ThreadUsageStats> &u, double windowSec)
{
    if (!u)
        return QStringLiteral("<td>–</td><td>–</td><td>–</td><td>–</td>");
    return QStringLiteral(
               "<td align=\"right\">%1</td><td align=\"right\">%2</td><td align=\"right\">%3</td>"
               "<td align=\"right\">%4 / %5</td>")
        .arg(fmtLoad(*u, windowSec))
        .arg(fmtSec(u->userTimeSec))
        .arg(fmtSec(u->systemTimeSec))
        .arg(u->voluntaryCtxSwitches)
        .arg(u->involuntaryCtxSwitches);
}

/**
 * The requested priority elevation; only a request that was not granted is pointed out.
 */
static QString fmtPriority(bool realtimeRequested, int niceness, const std::optional<bool> &applied)
{
    QString prio;
    if (realtimeRequested)
        prio = QStringLiteral("realtime");
    else if (niceness != 0)
        prio = QStringLiteral("nice %1").arg(niceness);
    else
        return QStringLiteral("default");

    if (applied.value_or(true))
        return prio;
    return QStringLiteral("<span style=\"color:%1\">%2 (not applied)</span>").arg(SyColorWarning.name(), prio);
}

/**
 * Source, destination and data type cells of a connection.
 */
static QString fmtConnectionCells(const ConnectionRunStats &c)
{
    return QStringLiteral("<td>%1<br/><small>%2</small></td><td>%3<br/><small>%4</small></td><td>%5</td>")
        .arg(
            c.srcModule.toHtmlEscaped(),
            c.srcPort.toHtmlEscaped(),
            c.dstModule.toHtmlEscaped(),
            c.dstPort.toHtmlEscaped(),
            c.dataType.toHtmlEscaped());
}

/**
 * Open a bordered table with the given column titles.
 */
static QString htmlDataTableStart(const QStringList &columns)
{
    auto html = QStringLiteral(
        "<table cellspacing=\"0\" cellpadding=\"3\" border=\"1\" style=\"border-collapse:collapse\"><tr>");
    for (const auto &column : columns)
        html += QStringLiteral("<th align=\"left\">%1</th>").arg(column);
    return html + QStringLiteral("</tr>");
}

QString RunStatistics::toHtml() const
{
    const auto colWarn = SyColorWarning.name();
    const auto colDanger = SyColorDanger.name();
    const auto colOk = SyColorSuccess.name();

    QString html;
    html.reserve(16 * 1024);

    // summary
    html += QStringLiteral("<h3>Run Summary</h3><table cellspacing=\"0\" cellpadding=\"3\">");
    const auto addRow = [&](const QString &key, const QString &value) {
        html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(key.toHtmlEscaped(), value);
    };
    addRow(
        QStringLiteral("Result"),
        failed ? QStringLiteral("<span style=\"color:%1\"><b>Failed</b></span> – %2")
                     .arg(colDanger, failReason.toHtmlEscaped())
               : QStringLiteral("<span style=\"color:%1\"><b>Success</b></span>").arg(colOk));
    addRow(QStringLiteral("Started"), started.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")).toHtmlEscaped());
    // the load percentages relate CPU time to the window it was measured over, which is
    // a bit longer than the recording itself (it includes starting and stopping modules)
    const double loadWindowSec = usageWindowSec > 0 ? usageWindowSec : durationSec;
    if (durationSec <= 0)
        addRow(QStringLiteral("Duration"), QStringLiteral("– (run never started)"));
    else if (usageWindowSec > durationSec + 0.05)
        addRow(
            QStringLiteral("Duration"),
            QStringLiteral("%1 (CPU usage measured over %2)").arg(fmtSec(durationSec), fmtSec(usageWindowSec)));
    else
        addRow(QStringLiteral("Duration"), fmtSec(durationSec));
    addRow(QStringLiteral("Run ID"), runId.toHtmlEscaped());
    addRow(
        QStringLiteral("Storage"),
        ephemeral ? QStringLiteral("ephemeral (data discarded)") : exportDir.toHtmlEscaped());
    addRow(QStringLiteral("Data written"), fmtBytes(bytesWritten));
    addRow(
        QStringLiteral("Threads"),
        QStringLiteral("%1 total, %2 with elevated priority (budget: %3), on %4 CPU cores")
            .arg(threadsTotal)
            .arg(threadsElevated)
            .arg(priorityBudget)
            .arg(cpuCoreCount));
    if (process)
        addRow(
            QStringLiteral("Process CPU"),
            QStringLiteral("%1 user, %2 system (%3 of one core)")
                .arg(fmtSec(process->userTimeSec), fmtSec(process->systemTimeSec), fmtLoad(*process, loadWindowSec)));
    if (mainThread)
        addRow(
            QStringLiteral("Main thread CPU"),
            QStringLiteral("%1 user, %2 system (%3)")
                .arg(
                    fmtSec(mainThread->userTimeSec),
                    fmtSec(mainThread->systemTimeSec),
                    fmtLoad(*mainThread, loadWindowSec)));
    if (peakRssKiB >= 0)
        addRow(
            QStringLiteral("Peak memory (RSS)"),
            QStringLiteral("%1 MiB (process lifetime)").arg(static_cast<double>(peakRssKiB) / 1024.0, 0, 'f', 1));
    html += QStringLiteral("</table>");

    // modules
    html += QStringLiteral("<h3>Modules</h3>")
            + htmlDataTableStart(
                {QStringLiteral("Module"),
                 QStringLiteral("Driver"),
                 QStringLiteral("Priority"),
                 QStringLiteral("Load"),
                 QStringLiteral("CPU user"),
                 QStringLiteral("CPU sys"),
                 QStringLiteral("Ctx. switches (vol/invol)"),
                 QStringLiteral("State")});
    for (const auto &m : modules) {
        auto prio = fmtPriority(m.realtimeRequested, m.niceness, m.priorityApplied);
        if (!m.cpuAffinity.empty()) {
            QStringList cores;
            for (const auto c : m.cpuAffinity)
                cores << QString::number(c);
            prio += QStringLiteral(", cores %1").arg(cores.join(QLatin1Char(',')));
        }

        QString driver = driverKindToString(m.driver);
        if (m.outOfProcess)
            driver = QStringLiteral("out-of-process");
        else if (!m.eventThreadKey.isEmpty())
            driver += QStringLiteral(" (%1)").arg(m.eventThreadKey.toHtmlEscaped());

        QString state = QString::fromStdString(toString(m.finalState));
        if (m.finalState == ModuleState::ERROR)
            state = QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>").arg(colDanger, state.toHtmlEscaped());
        if (!m.errorMessage.isEmpty())
            state += QStringLiteral("<br/><small>%1</small>").arg(m.errorMessage.toHtmlEscaped());

        const auto &usage = m.outOfProcess ? m.worker : m.thread;
        html += QStringLiteral("<tr><td><b>%1</b><br/><small>%2</small></td><td>%3</td><td>%4</td>%5<td>%6</td></tr>")
                    .arg(
                        m.name.toHtmlEscaped(),
                        m.id.toHtmlEscaped(),
                        driver,
                        prio,
                        fmtUsageCells(usage, loadWindowSec),
                        state);
    }
    html += QStringLiteral("</table>");
    if (!modules.isEmpty())
        html += QStringLiteral(
            "<p><small>Load is CPU time relative to the time CPU usage was measured over, in percent of a single core. "
            "Out-of-process modules show the usage of their worker process. Modules sharing an "
            "event thread are accounted for under that thread below. We cannot track additional threads "
            "spawned by the modules (e.g. for video encoders), so accounting may be inaccurate for those.</small></p>");

    // event threads
    if (!eventThreads.isEmpty()) {
        html += QStringLiteral("<h3>Event Threads</h3>")
                + htmlDataTableStart(
                    {QStringLiteral("Thread"),
                     QStringLiteral("Modules"),
                     QStringLiteral("Priority"),
                     QStringLiteral("Load"),
                     QStringLiteral("CPU user"),
                     QStringLiteral("CPU sys"),
                     QStringLiteral("Ctx. switches (vol/invol)")});
        for (const auto &e : eventThreads) {
            html += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td>%4</tr>")
                        .arg(
                            e.key.toHtmlEscaped(),
                            e.moduleNames.join(QStringLiteral(", ")).toHtmlEscaped(),
                            fmtPriority(e.realtimeRequested, e.niceness, e.priorityApplied),
                            fmtUsageCells(e.thread, loadWindowSec));
        }
        html += QStringLiteral("</table>");
    }

    // connections
    html += QStringLiteral("<h3>Connections</h3>");
    if (connections.isEmpty()) {
        html += QStringLiteral("<p><i>No connections between modules were active.</i></p>");
    } else {
        html += htmlDataTableStart(
            {QStringLiteral("From"),
             QStringLiteral("To"),
             QStringLiteral("Type"),
             QStringLiteral("Items"),
             QStringLiteral("Rate"),
             QStringLiteral("Peak backlog"),
             QStringLiteral("Left at stop"),
             QStringLiteral("Max. heat")});
        for (const auto &c : connections) {
            QString heat = heatToString(c.maxHeat);
            if (c.maxHeat == ConnectionHeatLevel::HIGH || c.maxHeat == ConnectionHeatLevel::MEDIUM)
                heat = QStringLiteral("<span style=\"color:%1\"><b>%2</b></span>").arg(colDanger, heat);
            else if (c.maxHeat == ConnectionHeatLevel::LOW)
                heat = QStringLiteral("<span style=\"color:%1\">%2</span>").arg(colWarn, heat);

            if (c.directIpc) {
                html += QStringLiteral("<tr>") + fmtConnectionCells(c)
                        + QStringLiteral(
                            "<td colspan=\"5\"><i>direct worker-to-worker IPC, not observed by "
                            "the engine</i></td></tr>");
                continue;
            }
            const QString rate = durationSec > 0 ? QStringLiteral("%1 /s").arg(
                                                       static_cast<double>(c.itemsReceived) / durationSec,
                                                       0,
                                                       'f',
                                                       1)
                                                 : QStringLiteral("–");
            html += QStringLiteral("<tr>") + fmtConnectionCells(c)
                    + QStringLiteral(
                          "<td align=\"right\">%1</td><td align=\"right\">%2</td>"
                          "<td align=\"right\">%3</td><td align=\"right\">%4</td><td>%5</td></tr>")
                          .arg(c.itemsReceived)
                          .arg(rate)
                          .arg(c.peakPending)
                          .arg(c.pendingAtStop)
                          .arg(heat);
        }
        html += QStringLiteral("</table>");
        html += QStringLiteral(
            "<p><small>Peak backlog is the largest queue length the resource monitor observed "
            "(sampled every few seconds).</small></p>");
    }

    // system
    if (!system.isEmpty()) {
        html += QStringLiteral("<h3>System</h3><table cellspacing=\"0\" cellpadding=\"3\">");
        for (auto it = system.constBegin(); it != system.constEnd(); ++it)
            addRow(it.key(), it.value().toHtmlEscaped());
        html += QStringLiteral("</table>");
    }

    return html;
}
