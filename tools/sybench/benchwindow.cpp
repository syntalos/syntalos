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

#include "benchwindow.h"
#include "ui_benchwindow.h"

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

#include "health.h"
#include "report.h"
#include "score.h"
#include "utils/misc.h"

namespace SyBench
{

BenchWindow::BenchWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::BenchWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon::fromTheme(QStringLiteral("speedometer")));
    ui->startButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    ui->stopButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-stop")));
    ui->saveButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    ui->backButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    ui->resultsButton->setIcon(QIcon::fromTheme(QStringLiteral("go-next")));
    ui->stepsButton->setIcon(QIcon::fromTheme(QStringLiteral("go-previous")));
    ui->newButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));

    ui->summaryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->healthTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->healthTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->stepsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->stepsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_workDir = QDir::temp().filePath(QStringLiteral("syntalos-bench-%1").arg(QCoreApplication::applicationPid()));
    setSyntalosBinary(SyntalosRunner::findSyntalosBinary());
    populateDimensions();

    connect(ui->startButton, &QPushButton::clicked, this, &BenchWindow::startBenchmark);
    connect(ui->stopButton, &QPushButton::clicked, this, &BenchWindow::stopBenchmark);
    connect(ui->saveButton, &QPushButton::clicked, this, &BenchWindow::saveReport);
    connect(ui->backButton, &QPushButton::clicked, this, &BenchWindow::backToStart);
    connect(ui->resultsButton, &QPushButton::clicked, this, &BenchWindow::showResultsPage);
    connect(ui->stepsButton, &QPushButton::clicked, this, &BenchWindow::showRunPage);
    connect(ui->newButton, &QPushButton::clicked, this, &BenchWindow::backToStart);
    connect(ui->dataDirButton, &QPushButton::clicked, this, &BenchWindow::chooseDataDir);
    ui->dataDirEdit->setText(defaultDataDir());

    m_elapsedTimer.setInterval(1000);
    connect(&m_elapsedTimer, &QTimer::timeout, this, &BenchWindow::updateElapsed);
}

BenchWindow::~BenchWindow()
{
    delete ui;
}

void BenchWindow::setSyntalosBinary(const QString &path)
{
    m_syntalosBinary = path;
    if (path.isEmpty()) {
        ui->syntalosLabel->setText(
            QStringLiteral("<b>Syntalos executable not found.</b> Install Syntalos or pass --syntalos-bin."));
        ui->startButton->setEnabled(false);
    } else {
        ui->syntalosLabel->setText(QStringLiteral("Syntalos: %1").arg(path));
        ui->startButton->setEnabled(true);
    }
}

void BenchWindow::setWorkDir(const QString &dir)
{
    m_workDir = dir;
}

void BenchWindow::setDataDir(const QString &dir)
{
    ui->dataDirEdit->setText(dir);
}

void BenchWindow::chooseDataDir()
{
    const auto dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Data Directory"),
        ui->dataDirEdit->text());
    if (!dir.isEmpty())
        ui->dataDirEdit->setText(dir);
}

void BenchWindow::populateDimensions()
{
    ui->dimensionTree->clear();
    for (const auto &dim : createAllDimensions()) {
        auto *item = new QTreeWidgetItem(ui->dimensionTree);
        item->setText(0, dim->title());
        item->setToolTip(0, dim->description());
        item->setData(0, Qt::UserRole, dim->id());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
        item->setCheckState(0, Qt::Checked);
        for (const auto &p : dim->profiles()) {
            auto *child = new QTreeWidgetItem(item);
            child->setText(0, p.title);
            child->setData(0, Qt::UserRole, p.id);
            child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
            child->setCheckState(0, Qt::Checked);
        }
        item->setExpanded(true);
    }
}

SessionConfig BenchWindow::collectConfig() const
{
    SessionConfig cfg;
    cfg.quick = ui->quickCheck->isChecked();
    cfg.workDir = m_workDir;
    cfg.dataDir = ui->dataDirEdit->text().trimmed();
    cfg.syntalosBinary = m_syntalosBinary;
    for (int i = 0; i < ui->dimensionTree->topLevelItemCount(); ++i) {
        const auto *item = ui->dimensionTree->topLevelItem(i);
        const auto dimId = item->data(0, Qt::UserRole).toString();
        for (int j = 0; j < item->childCount(); ++j) {
            const auto *child = item->child(j);
            if (child->checkState(0) == Qt::Checked)
                cfg.selections.append({dimId, child->data(0, Qt::UserRole).toString()});
        }
    }
    return cfg;
}

bool BenchWindow::isRunning() const
{
    return m_thread != nullptr;
}

void BenchWindow::setRunningState(bool running)
{
    ui->stopButton->setEnabled(running);
    ui->stopButton->setVisible(running);
    ui->resultsButton->setVisible(!running);
    ui->resultsButton->setEnabled(!running && m_finished);
    ui->backButton->setEnabled(!running);
    ui->saveButton->setEnabled(!m_ladders.isEmpty());
}

void BenchWindow::startBenchmark()
{
    if (isRunning())
        return;
    m_config = collectConfig();
    if (m_config.selections.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Nothing selected"),
            QStringLiteral("Please select at least one benchmark dimension."));
        return;
    }

    m_ladders.clear();
    m_finished = false;
    ui->summaryTable->setRowCount(0);
    ui->stepsTable->setRowCount(0);
    ui->logView->clear();
    ui->scoreLabel->clear();
    ui->scoreDetailLabel->clear();
    ui->progressBar->setValue(0);
    ui->phaseLabel->setText(QStringLiteral("Starting..."));
    ui->pages->setCurrentWidget(ui->runPage);

    m_thread = new QThread(this);
    // set the OS thread name
    m_thread->setObjectName(QStringLiteral("session"));

    m_session = new BenchSession(m_config);
    m_cpuCores = m_session->cpuCores();
    m_stepsPerLadder = m_session->estimatedStepsPerLadder();
    ui->progressBar->setRange(0, m_config.selections.size() * m_stepsPerLadder);
    m_session->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_session, &BenchSession::run);
    connect(m_session, &BenchSession::progressMessage, this, &BenchWindow::appendLog);
    connect(m_session, &BenchSession::phaseChanged, this, &BenchWindow::onPhaseChanged);
    connect(m_session, &BenchSession::ladderStarted, this, &BenchWindow::onLadderStarted);
    connect(m_session, &BenchSession::stepFinished, this, &BenchWindow::onStepFinished);
    connect(m_session, &BenchSession::ladderFinished, this, &BenchWindow::onLadderFinished);
    connect(m_session, &BenchSession::finished, this, &BenchWindow::onSessionFinished);

    setRunningState(true);
    m_elapsed.start();
    updateElapsed();
    m_elapsedTimer.start();
    m_thread->start();
}

void BenchWindow::stopBenchmark()
{
    if (m_session == nullptr)
        return;
    ui->stopButton->setEnabled(false);
    ui->phaseLabel->setText(QStringLiteral("Stopping after the current run..."));
    // only touches atomics, safe to call from this thread
    m_session->cancel();
}

void BenchWindow::onSessionFinished(bool cancelled)
{
    m_elapsedTimer.stop();
    updateElapsed();
    ui->progressBar->setValue(cancelled ? ui->progressBar->value() : ui->progressBar->maximum());
    ui->phaseLabel->setText(cancelled ? QStringLiteral("Benchmark cancelled.") : QStringLiteral("Benchmark finished."));

    m_thread->quit();
    m_thread->wait();
    m_session->deleteLater();
    m_session = nullptr;
    m_thread->deleteLater();
    m_thread = nullptr;
    m_finished = true;
    setRunningState(false);
    showHealth();
    showScore();
    showResultsPage();
}

void BenchWindow::showResultsPage()
{
    ui->pages->setCurrentWidget(ui->resultsPage);
}

void BenchWindow::showRunPage()
{
    ui->pages->setCurrentWidget(ui->runPage);
}

void BenchWindow::showScore()
{
    const auto score = computeScore(m_ladders, m_config);
    ui->scoreLabel->setText(scoreHeadline(score));
    if (!score.valid()) {
        ui->scoreDetailLabel->setText(QStringLiteral("No scored dimension completed."));
        return;
    }
    QStringList parts;
    for (const auto &d : score.dimensions)
        parts.append(QStringLiteral("%1 %2").arg(d.title).arg(d.score));
    ui->scoreDetailLabel->setText(parts.join(QStringLiteral("  ·  ")));
}

void BenchWindow::showHealth()
{
    ui->healthTable->setRowCount(0);
    for (const auto &item : collectHealthItems()) {
        const int row = ui->healthTable->rowCount();
        ui->healthTable->insertRow(row);
        ui->healthTable->setItem(row, 0, new QTableWidgetItem(item.name));
        ui->healthTable->setItem(row, 1, new QTableWidgetItem(item.value));
        auto *status = new QTableWidgetItem(healthStatusString(item.status));
        // same colours as the System Info dialog in Syntalos
        if (item.status == Syntalos::SysInfoCheckResult::ISSUE)
            status->setForeground(QColor(218, 68, 83));
        else if (item.status == Syntalos::SysInfoCheckResult::SUSPICIOUS)
            status->setForeground(QColor(244, 119, 80));
        ui->healthTable->setItem(row, 2, status);
    }
}

void BenchWindow::saveReport()
{
    if (m_ladders.isEmpty())
        return;
    const auto suggested = QDir::home().filePath(
        QStringLiteral("syntalos-benchmark-%1.json")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmm"))));
    const auto fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Benchmark Report"),
        suggested,
        QStringLiteral("JSON report (*.json)"));
    if (fileName.isEmpty())
        return;
    if (const auto res = SyBench::saveReport(fileName, m_ladders, m_config, m_cpuCores); !res)
        QMessageBox::critical(this, QStringLiteral("Unable to save report"), res.error());
}

void BenchWindow::backToStart()
{
    if (isRunning())
        return;
    ui->pages->setCurrentWidget(ui->startPage);
}

void BenchWindow::updateElapsed()
{
    const auto secs = m_elapsed.isValid() ? m_elapsed.elapsed() / 1000 : 0;
    ui->elapsedLabel->setText(QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QChar('0')));
}

void BenchWindow::onPhaseChanged(const QString &text)
{
    ui->phaseLabel->setText(text);
}

void BenchWindow::onLadderStarted(int index, int, const LadderRecord &)
{
    m_ladderIndex = index;
    m_ladderSteps = 0;
    ui->progressBar->setValue(index * m_stepsPerLadder);
}

void BenchWindow::onStepFinished(const StepRecord &step)
{
    m_ladderSteps++;
    ui->progressBar->setValue(m_ladderIndex * m_stepsPerLadder + std::min(m_ladderSteps, m_stepsPerLadder - 1));

    const int row = ui->stepsTable->rowCount();
    ui->stepsTable->insertRow(row);
    const auto setCell = [&](int col, const QString &text) {
        auto *item = new QTableWidgetItem(text);
        ui->stepsTable->setItem(row, col, item);
        return item;
    };
    setCell(0, step.dimensionTitle);
    setCell(1, step.profileTitle);
    setCell(2, QStringLiteral("%1 %2").arg(step.level).arg(step.levelUnit));
    auto *resItem = setCell(
        3,
        step.verdict.passed          ? QStringLiteral("Pass")
        : step.verdict.sourceLimited ? QStringLiteral("Inconclusive")
                                     : QStringLiteral("Fail"));
    resItem->setIcon(
        QIcon::fromTheme(step.verdict.passed ? QStringLiteral("emblem-checked") : QStringLiteral("emblem-error")));
    setCell(4, step.verdict.summary);
    setCell(
        5,
        step.result.success ? QStringLiteral("%1 cores").arg(step.result.processLoad(), 0, 'f', 1)
                            : QStringLiteral("-"));
    setCell(6, Syntalos::formatByteSize(step.result.peakRssKiB() * 1024));
    ui->stepsTable->scrollToBottom();
}

void BenchWindow::onLadderFinished(const LadderRecord &ladder)
{
    m_ladders.append(ladder);
    const int row = ui->summaryTable->rowCount();
    ui->summaryTable->insertRow(row);
    ui->summaryTable->setItem(row, 0, new QTableWidgetItem(ladder.dimensionTitle));
    ui->summaryTable->setItem(row, 1, new QTableWidgetItem(ladder.profileTitle));
    auto sustained = QStringLiteral("%1 %2").arg(ladder.outcome.sustained).arg(ladder.levelUnit);
    if (ladder.outcome.cancelled)
        sustained += QStringLiteral(" (incomplete)");
    else if (ladder.outcome.inconclusive)
        sustained += QStringLiteral(" (source limit reached)");
    else if (ladder.outcome.reachedMax)
        sustained += QStringLiteral(" (or more)");
    auto *item = new QTableWidgetItem(sustained);
    auto font = item->font();
    font.setBold(true);
    item->setFont(font);
    ui->summaryTable->setItem(row, 2, item);
}

void BenchWindow::appendLog(const QString &msg)
{
    ui->logView->appendPlainText(msg);
}

void BenchWindow::closeEvent(QCloseEvent *event)
{
    if (isRunning()) {
        const auto reply = QMessageBox::question(
            this,
            QStringLiteral("Benchmark running"),
            QStringLiteral("A benchmark is still running. Stop it and quit?"));
        if (reply != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_session->cancel();
        m_thread->quit();
        m_thread->wait();
    }
    event->accept();
}

} // namespace SyBench
