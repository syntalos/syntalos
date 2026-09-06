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

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "fabric/logging.h"

namespace Syntalos
{
/**
 * @brief Metrics recorded for a single (non-ephemeral) run of a project.
 */
struct ProjectRunMetrics {
    QDateTime finished;
    qint64 bytesWritten = 0;
    double durationSec = 0.0;
    bool success = false;

    /**
     * @brief Average write rate of this run in bytes per second (0 if unknown).
     */
    double writeRateBps() const;
};

/**
 * @brief Per-project cache of metrics from previous runs.
 *
 * Describes information Syntalos remembers about projects it has run before
 * (e.g. how much data a run of this project produced), so that it can make
 * better guesses in the future, for example about whether the disk
 * has enough free space for another run.
 */
class ProjectMetrics
{
public:
    static constexpr int MaxProjects = 20;
    static constexpr int MaxRunsPerProject = 5;

    /**
     * @param storageFile JSON file to store the metrics in, or empty to use the
     *                    default location in the user's cache directory.
     */
    explicit ProjectMetrics(const QString &storageFile = QString());

    ProjectMetrics(const ProjectMetrics &) = delete;
    ProjectMetrics &operator=(const ProjectMetrics &) = delete;

    /**
     * @brief Set the project file that all following queries and records refer to.
     *
     * An empty file name (unsaved project) disables recording and yields no history.
     */
    void setProjectFile(const QString &fname);
    QString projectFile() const;

    bool hasRunHistory() const;

    /**
     * @brief Recorded runs of the current project, most recent first.
     */
    QList<ProjectRunMetrics> runHistory() const;

    /**
     * @brief The largest amount of data any recorded run of this project produced.
     */
    qint64 maxRunBytes() const;

    /**
     * @brief Average write rate of the most recent run that actually produced data, in bytes/sec.
     */
    double lastRunWriteRateBps() const;

    /**
     * @brief Record metrics of a finished run for the current project.
     *
     * Older runs beyond MaxRunsPerProject are dropped, and if more than MaxProjects
     * are known, the least recently used ones are forgotten.
     */
    void recordRun(const ProjectRunMetrics &run);

private:
    void load();
    bool save() const;
    void pruneProjects();

private:
    QuillLogger *m_log;
    QString m_storageFile;
    QJsonObject m_projects;
    QString m_projectFile;
};

} // namespace Syntalos
