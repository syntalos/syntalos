/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <expected>
#include <memory>
#include <stop_token>

#include "dimension.h"
#include "ladder.h"
#include "logging.h"
#include "runner.h"

namespace SyBench
{

/**
 * @brief What a benchmark session should run
 */
struct SessionConfig {
    QList<QPair<QString, QString>> selections; /// (dimension id, profile id) pairs, in run order
    bool quick = false;
    int stepSeconds = 0;    /// 0 = default for the mode (20 s, 10 s in quick mode)
    int startLevel = 0;     /// 0 = let the dimension decide
    int maxLevel = 0;       /// 0 = let the dimension decide (developer option, e.g. for single-step runs)
    bool warmup = true;     /// do a discarded run first
    QString workDir;        /// where projects and statistics files are written
    QString dataDir;        /// where dimensions that record data write to (a real disk, not tmpfs)
    QString syntalosBinary; /// empty = auto-detect

    /**
     * @brief Duration of each run, with the mode's default applied.
     */
    [[nodiscard]] int effectiveStepSeconds() const;
};

/**
 * @brief Default location for recorded benchmark data (in the user's cache directory).
 */
QString defaultDataDir();

/**
 * @brief Which dimension profile a record belongs to
 */
struct ProfileRef {
    QString dimensionId;
    QString dimensionTitle;
    QString profileId;
    QString profileTitle;
    QString levelUnit;

    static ProfileRef of(const Dimension &dim, const QString &profileId);
};

/**
 * @brief One measured step of a ladder
 */
struct StepRecord : ProfileRef {
    int level = 0;
    StepVerdict verdict;
    StepResult result;
    QString statsFile;
};

/**
 * @brief The outcome of one dimension/profile ladder
 */
struct LadderRecord : ProfileRef {
    LadderOutcome outcome;
    QList<StepRecord> steps;
};

/**
 * @brief Runs the selected ladders one after another.
 *
 * Meant to live in its own thread: run() blocks until everything is done or cancelled,
 * progress is reported through signals.
 */
class BenchSession : public QObject
{
    Q_OBJECT
public:
    explicit BenchSession(const SessionConfig &config, QObject *parent = nullptr);
    ~BenchSession() override;

    int estimatedStepsPerLadder() const;

    /**
     * @brief Run a single step of a dimension (used by the self test).
     */
    auto runSingleStep(const Dimension &dim, const QString &profileId, int level, int durationSec)
        -> std::expected<StepRecord, QString>;

public slots:
    void run();
    void cancel();

signals:
    /**
     * @brief Human-readable progress, e.g. for a log pane in the GUI.
     */
    void progressMessage(const QString &msg);
    void phaseChanged(const QString &text);
    void ladderStarted(int index, int total, const SyBench::LadderRecord &ladder);
    void stepStarted(const SyBench::LadderRecord &ladder, int level);
    void stepFinished(const SyBench::StepRecord &step);
    void ladderFinished(const SyBench::LadderRecord &ladder);
    /**
     * @brief The session ended.
     * @param error Why the session was aborted early, empty if it ran to completion or was cancelled.
     */
    void finished(bool cancelled, const QString &error);

private:
    SessionConfig m_config;
    SyntalosRunner m_runner;
    std::vector<std::unique_ptr<Dimension>> m_dimensions;
    quill::Logger *m_log;
    int m_cpuCores;
    std::stop_source m_stop;

    const Dimension *dimension(const QString &id) const;
    void progress(const QString &msg);
    int bisections() const;
    LadderConfig ladderConfig(const Dimension &dim, const QString &profileId) const;
    void abort(const QString &error);

    /**
     * @brief Run one step. The error branch is taken when no run can work at all.
     */
    auto runStep(const Dimension &dim, const QString &profileId, int level, int durationSec)
        -> std::expected<StepRecord, QString>;
};

} // namespace SyBench

Q_DECLARE_METATYPE(SyBench::StepRecord)
Q_DECLARE_METATYPE(SyBench::LadderRecord)
