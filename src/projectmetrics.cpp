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

#include "projectmetrics.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>

#include "globalconfig.h"

namespace Syntalos
{
static const int STORAGE_FORMAT_VERSION = 1;

static const QString KEY_VERSION = QStringLiteral("version");
static const QString KEY_PROJECTS = QStringLiteral("projects");
static const QString KEY_LAST_USED = QStringLiteral("last_used");
static const QString KEY_RUNS = QStringLiteral("runs");
static const QString KEY_FINISHED = QStringLiteral("finished");
static const QString KEY_BYTES_WRITTEN = QStringLiteral("bytes_written");
static const QString KEY_DURATION_SEC = QStringLiteral("duration_sec");
static const QString KEY_SUCCESS = QStringLiteral("success");

double ProjectRunMetrics::writeRateBps() const
{
    if (durationSec <= 0.0 || bytesWritten <= 0)
        return 0.0;
    return static_cast<double>(bytesWritten) / durationSec;
}

ProjectMetrics::ProjectMetrics(const QString &storageFile)
    : m_log(getLogger("project-metrics")),
      m_storageFile(storageFile)
{
    if (m_storageFile.isEmpty()) {
        Syntalos::GlobalConfig gconf;
        m_storageFile = QDir(gconf.userCacheDir()).filePath(QStringLiteral("project-metrics.json"));
    }
    load();
}

void ProjectMetrics::load()
{
    m_projects = QJsonObject();

    QFile f(m_storageFile);
    if (!f.exists())
        return;
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "Unable to read project metrics from" << m_storageFile << ":" << f.errorString();
        return;
    }

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << "Ignoring malformed project metrics file" << m_storageFile << ":"
                             << parseError.errorString();
        return;
    }

    const auto root = doc.object();
    if (root.value(KEY_VERSION).toInt(0) > STORAGE_FORMAT_VERSION) {
        qWarning().noquote() << "Ignoring project metrics file" << m_storageFile
                             << "written by a newer Syntalos version.";
        return;
    }
    m_projects = root.value(KEY_PROJECTS).toObject();
}

bool ProjectMetrics::save() const
{
    QDir().mkpath(QFileInfo(m_storageFile).absolutePath());

    QJsonObject root;
    root.insert(KEY_VERSION, STORAGE_FORMAT_VERSION);
    root.insert(KEY_PROJECTS, m_projects);

    QSaveFile f(m_storageFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning().noquote() << "Unable to write project metrics to" << m_storageFile << ":" << f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        qWarning().noquote() << "Unable to write project metrics to" << m_storageFile << ":" << f.errorString();
        return false;
    }
    return true;
}

void ProjectMetrics::setProjectFile(const QString &fname)
{
    m_projectFile.clear();
    if (fname.isEmpty())
        return;

    // Use the canonical path when possible, so the same project is recognized
    // regardless of how it was opened.
    const QFileInfo fi(fname);
    m_projectFile = fi.exists() ? fi.canonicalFilePath() : fi.absoluteFilePath();
}

QString ProjectMetrics::projectFile() const
{
    return m_projectFile;
}

bool ProjectMetrics::hasRunHistory() const
{
    return !runHistory().isEmpty();
}

QList<ProjectRunMetrics> ProjectMetrics::runHistory() const
{
    QList<ProjectRunMetrics> runs;
    if (m_projectFile.isEmpty())
        return runs;

    const auto runsArr = m_projects.value(m_projectFile).toObject().value(KEY_RUNS).toArray();
    for (const auto &v : runsArr) {
        const auto obj = v.toObject();
        ProjectRunMetrics run;
        run.finished = QDateTime::fromString(obj.value(KEY_FINISHED).toString(), Qt::ISODateWithMs);
        run.bytesWritten = static_cast<qint64>(obj.value(KEY_BYTES_WRITTEN).toDouble(0));
        run.durationSec = obj.value(KEY_DURATION_SEC).toDouble(0);
        run.success = obj.value(KEY_SUCCESS).toBool(false);
        runs.append(run);
    }

    // stored oldest-first, but we report most recent first
    std::reverse(runs.begin(), runs.end());
    return runs;
}

qint64 ProjectMetrics::maxRunBytes() const
{
    qint64 maxBytes = 0;
    for (const auto &run : runHistory())
        maxBytes = std::max(maxBytes, run.bytesWritten);
    return maxBytes;
}

double ProjectMetrics::lastRunWriteRateBps() const
{
    for (const auto &run : runHistory()) {
        const auto rate = run.writeRateBps();
        if (rate > 0.0)
            return rate;
    }
    return 0.0;
}

void ProjectMetrics::recordRun(const ProjectRunMetrics &run)
{
    if (m_projectFile.isEmpty())
        return;

    auto runs = runHistory();
    runs.prepend(run);
    while (runs.size() > MaxRunsPerProject)
        runs.removeLast();
    std::reverse(runs.begin(), runs.end()); // store oldest-first

    QJsonArray runsArr;
    for (const auto &r : runs) {
        QJsonObject obj;
        obj.insert(KEY_FINISHED, r.finished.toString(Qt::ISODateWithMs));
        obj.insert(KEY_BYTES_WRITTEN, static_cast<double>(r.bytesWritten));
        obj.insert(KEY_DURATION_SEC, r.durationSec);
        obj.insert(KEY_SUCCESS, r.success);
        runsArr.append(obj);
    }

    QJsonObject project;
    project.insert(KEY_LAST_USED, QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    project.insert(KEY_RUNS, runsArr);
    m_projects.insert(m_projectFile, project);

    pruneProjects();
    save();
}

void ProjectMetrics::pruneProjects()
{
    if (m_projects.size() <= MaxProjects)
        return;

    QList<std::pair<QDateTime, QString>> byAge;
    for (auto it = m_projects.constBegin(); it != m_projects.constEnd(); ++it) {
        const auto lastUsed = QDateTime::fromString(
            it.value().toObject().value(KEY_LAST_USED).toString(),
            Qt::ISODateWithMs);
        byAge.append({lastUsed, it.key()});
    }
    std::sort(byAge.begin(), byAge.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });
    for (qsizetype i = 0; i < byAge.size() - MaxProjects; ++i)
        m_projects.remove(byAge[i].second);
}

} // namespace Syntalos
