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

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>

#include "benchsession.h"

namespace Ui
{
class BenchWindow;
}

class QThread;

namespace SyBench
{

class BenchWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit BenchWindow(QWidget *parent = nullptr);
    ~BenchWindow() override;

    void setSyntalosBinary(const QString &path);
    void setWorkDir(const QString &dir);
    void setDataDir(const QString &dir);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startBenchmark();
    void stopBenchmark();
    void saveReport();
    void backToStart();
    void showResultsPage();
    void showRunPage();
    void chooseDataDir();
    void updateElapsed();

    void onPhaseChanged(const QString &text);
    void onLadderStarted(int index, int total, const SyBench::LadderRecord &ladder);
    void onStepFinished(const SyBench::StepRecord &step);
    void onLadderFinished(const SyBench::LadderRecord &ladder);
    void onSessionFinished(bool cancelled);
    void appendLog(const QString &msg);

private:
    Ui::BenchWindow *ui;
    QString m_syntalosBinary;
    QString m_workDir;

    QThread *m_thread = nullptr;
    BenchSession *m_session = nullptr;
    SessionConfig m_config;
    QList<LadderRecord> m_ladders;
    QElapsedTimer m_elapsed;
    QTimer m_elapsedTimer;
    int m_cpuCores = 0;
    bool m_finished = false;
    int m_stepsPerLadder = 1;
    int m_ladderIndex = 0;
    int m_ladderSteps = 0;

    void populateDimensions();
    SessionConfig collectConfig() const;
    void setRunningState(bool running);
    void showHealth();
    void showScore();
    bool isRunning() const;
};

} // namespace SyBench
