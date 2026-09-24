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
#include <QFile>
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
        if (!m.moduleStats.isEmpty())
            o.insert(QStringLiteral("module_stats"), QJsonObject::fromVariantHash(m.moduleStats));
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

auto RunStatistics::saveJson(const QString &path) const -> std::expected<void, QString>
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return std::unexpected(f.errorString());
    f.write(toJsonData());
    if (!f.commit())
        return std::unexpected(f.errorString());
    return {};
}

static std::optional<ThreadUsageStats> usageFromJson(const QJsonValue &v)
{
    if (!v.isObject())
        return std::nullopt;
    const auto o = v.toObject();
    ThreadUsageStats u;
    u.tid = o.value(QStringLiteral("tid")).toInteger();
    u.userTimeSec = o.value(QStringLiteral("user_time_sec")).toDouble();
    u.systemTimeSec = o.value(QStringLiteral("system_time_sec")).toDouble();
    u.voluntaryCtxSwitches = o.value(QStringLiteral("voluntary_ctx_switches")).toInteger();
    u.involuntaryCtxSwitches = o.value(QStringLiteral("involuntary_ctx_switches")).toInteger();
    u.minorPageFaults = o.value(QStringLiteral("minor_page_faults")).toInteger();
    u.majorPageFaults = o.value(QStringLiteral("major_page_faults")).toInteger();
    return u;
}

static ModuleDriverKind driverKindFromString(const QString &s)
{
    for (const auto kind :
         {ModuleDriverKind::NONE,
          ModuleDriverKind::THREAD_DEDICATED,
          ModuleDriverKind::EVENTS_DEDICATED,
          ModuleDriverKind::EVENTS_SHARED}) {
        if (driverKindToString(kind) == s)
            return kind;
    }
    return ModuleDriverKind::NONE;
}

static ModuleState moduleStateFromString(const QString &s)
{
    for (int i = static_cast<int>(ModuleState::UNKNOWN); i <= static_cast<int>(ModuleState::ERROR); ++i) {
        const auto state = static_cast<ModuleState>(i);
        if (QString::fromStdString(toString(state)) == s)
            return state;
    }
    return ModuleState::UNKNOWN;
}

static ConnectionHeatLevel heatFromString(const QString &s)
{
    for (const auto heat :
         {ConnectionHeatLevel::NONE,
          ConnectionHeatLevel::LOW,
          ConnectionHeatLevel::MEDIUM,
          ConnectionHeatLevel::HIGH}) {
        if (heatToString(heat) == s)
            return heat;
    }
    return ConnectionHeatLevel::NONE;
}

/**
 * The inverse of insertPriorityApplied().
 */
static std::optional<bool> readPriorityApplied(const QJsonObject &o)
{
    if (o.contains(QStringLiteral("realtime_applied")))
        return o.value(QStringLiteral("realtime_applied")).toBool();
    if (o.contains(QStringLiteral("niceness_applied")))
        return o.value(QStringLiteral("niceness_applied")).toBool();
    return std::nullopt;
}

auto RunStatistics::fromJson(const QJsonObject &root) -> std::expected<RunStatistics, QString>
{
    const int version = root.value(QStringLiteral("format_version")).toInt(-1);
    if (version < 1 || version > FormatVersion)
        return std::unexpected(QStringLiteral("Unsupported run statistics format version %1").arg(version));
    if (!root.value(QStringLiteral("run")).isObject())
        return std::unexpected(QStringLiteral("Run statistics document has no run section"));

    RunStatistics stats;
    const auto run = root.value(QStringLiteral("run")).toObject();
    stats.runId = run.value(QStringLiteral("id")).toString();
    stats.experimentId = run.value(QStringLiteral("experiment_id")).toString();
    stats.started = QDateTime::fromString(run.value(QStringLiteral("started")).toString(), Qt::ISODateWithMs);
    stats.durationSec = run.value(QStringLiteral("duration_sec")).toDouble();
    stats.usageWindowSec = run.value(QStringLiteral("usage_window_sec")).toDouble();
    stats.ephemeral = run.value(QStringLiteral("ephemeral")).toBool();
    stats.failed = !run.value(QStringLiteral("success")).toBool(true);
    stats.failReason = run.value(QStringLiteral("failure_reason")).toString();
    stats.exportDir = run.value(QStringLiteral("export_dir")).toString();
    stats.bytesWritten = run.value(QStringLiteral("bytes_written")).toInteger(-1);
    stats.cpuCoreCount = run.value(QStringLiteral("cpu_cores")).toInt();
    stats.priorityBudget = static_cast<uint>(run.value(QStringLiteral("priority_budget")).toInt());
    stats.threadsTotal = run.value(QStringLiteral("threads_total")).toInt();
    stats.threadsElevated = run.value(QStringLiteral("threads_elevated")).toInt();
    stats.mainThread = usageFromJson(run.value(QStringLiteral("main_thread")));
    stats.process = usageFromJson(run.value(QStringLiteral("process")));
    stats.peakRssKiB = run.value(QStringLiteral("peak_rss_kib")).toInteger(-1);

    for (const auto &mv : root.value(QStringLiteral("modules")).toArray()) {
        const auto o = mv.toObject();
        ModuleRunStats m;
        m.id = o.value(QStringLiteral("id")).toString();
        m.name = o.value(QStringLiteral("name")).toString();
        m.driver = driverKindFromString(o.value(QStringLiteral("driver")).toString());
        m.finalState = moduleStateFromString(o.value(QStringLiteral("final_state")).toString());
        m.outOfProcess = o.value(QStringLiteral("out_of_process")).toBool();
        m.eventThreadKey = o.value(QStringLiteral("event_thread")).toString();
        m.errorMessage = o.value(QStringLiteral("error")).toString();
        m.realtimeRequested = o.value(QStringLiteral("realtime_requested")).toBool();
        m.niceness = o.value(QStringLiteral("niceness")).toInt();
        m.priorityApplied = readPriorityApplied(o);
        for (const auto &c : o.value(QStringLiteral("cpu_affinity")).toArray())
            m.cpuAffinity.push_back(static_cast<uint>(c.toInt()));
        m.thread = usageFromJson(o.value(QStringLiteral("thread")));
        m.workerPid = o.value(QStringLiteral("worker_pid")).toInteger();
        m.worker = usageFromJson(o.value(QStringLiteral("worker")));
        m.moduleStats = o.value(QStringLiteral("module_stats")).toObject().toVariantHash();
        stats.modules.append(m);
    }

    for (const auto &ev : root.value(QStringLiteral("event_threads")).toArray()) {
        const auto o = ev.toObject();
        EventThreadRunStats e;
        e.key = o.value(QStringLiteral("key")).toString();
        for (const auto &n : o.value(QStringLiteral("modules")).toArray())
            e.moduleNames.append(n.toString());
        e.realtimeRequested = o.value(QStringLiteral("realtime_requested")).toBool();
        e.niceness = o.value(QStringLiteral("niceness")).toInt();
        e.priorityApplied = readPriorityApplied(o).value_or(false);
        e.thread = usageFromJson(o.value(QStringLiteral("thread")));
        stats.eventThreads.append(e);
    }

    for (const auto &cv : root.value(QStringLiteral("connections")).toArray()) {
        const auto o = cv.toObject();
        ConnectionRunStats c;
        c.srcModule = o.value(QStringLiteral("src_module")).toString();
        c.srcPort = o.value(QStringLiteral("src_port")).toString();
        c.dstModule = o.value(QStringLiteral("dst_module")).toString();
        c.dstPort = o.value(QStringLiteral("dst_port")).toString();
        c.dataType = o.value(QStringLiteral("data_type")).toString();
        c.directIpc = o.value(QStringLiteral("direct_ipc")).toBool();
        c.itemsReceived = o.value(QStringLiteral("items_received")).toInteger();
        c.peakPending = o.value(QStringLiteral("peak_pending")).toInteger();
        c.pendingAtStop = o.value(QStringLiteral("pending_at_stop")).toInteger();
        c.maxHeat = heatFromString(o.value(QStringLiteral("max_heat")).toString());
        stats.connections.append(c);
    }

    const auto sys = root.value(QStringLiteral("system")).toObject();
    for (auto it = sys.constBegin(); it != sys.constEnd(); ++it)
        stats.system.insert(it.key(), it.value().toString());

    return stats;
}

auto RunStatistics::loadJson(const QString &path) -> std::expected<RunStatistics, QString>
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::unexpected(QStringLiteral("Unable to read %1: %2").arg(path, f.errorString()));
    QJsonParseError perr;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (doc.isNull())
        return std::unexpected(QStringLiteral("Unable to parse %1: %2").arg(path, perr.errorString()));
    if (!doc.isObject())
        return std::unexpected(QStringLiteral("%1 does not contain a JSON object").arg(path));
    return fromJson(doc.object());
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

/**
 * Format a value reported by a module, grouped values are shown as "key: value" list.
 */
static QString fmtModuleStatValue(const QVariant &value)
{
    if (value.typeId() == QMetaType::QVariantHash || value.typeId() == QMetaType::QVariantMap) {
        const auto map = value.toMap();
        QStringList parts;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            parts << QStringLiteral("%1:&nbsp;%2").arg(it.key().toHtmlEscaped(), fmtModuleStatValue(it.value()));
        return parts.join(QStringLiteral(", "));
    }
    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float)
        return QString::number(value.toDouble(), 'f', 2);
    return value.toString().toHtmlEscaped();
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

    // measurements reported by the modules themselves
    QString modStatsHtml;
    for (const auto &m : modules) {
        auto keys = m.moduleStats.keys();
        std::sort(keys.begin(), keys.end());
        for (const auto &key : keys)
            modStatsHtml += QStringLiteral("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                                .arg(
                                    m.name.toHtmlEscaped(),
                                    key.toHtmlEscaped(),
                                    fmtModuleStatValue(m.moduleStats[key]));
    }
    if (!modStatsHtml.isEmpty())
        html += QStringLiteral("<h3>Module Statistics</h3>")
                + htmlDataTableStart({QStringLiteral("Module"), QStringLiteral("Statistic"), QStringLiteral("Value")})
                + modStatsHtml + QStringLiteral("</table>");

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
