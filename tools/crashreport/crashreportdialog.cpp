/*
 * Copyright (C) 2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "crashreportdialog.h"
#include "ui_crashreportdialog.h"

#include <QtConcurrent/QtConcurrent>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>

#include "debugcollect.h"

CrashReportDialog::CrashReportDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CrashReportDialog)
{
    ui->setupUi(this);
    ui->pageStack->setCurrentIndex(0);
    setWindowTitle("Syntalos Crash Info Collector");

    // set icon
    ui->doneIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("checkmark")).pixmap(64, 64));

    // check if all the tools we need are there
    m_allToolsFound = true;
    if (QStandardPaths::findExecutable("coredumpctl").isEmpty()) {
        ui->coredumpctlLabel->setText("Coredumpctl is missing!");
        ui->coredumpctlIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-error")).pixmap(20, 20));
        m_allToolsFound = false;
    } else {
        ui->coredumpctlLabel->setText("Coredumpctl found.");
        ui->coredumpctlIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-checked")).pixmap(20, 20));
    }
    if (QStandardPaths::findExecutable("gdb").isEmpty()) {
        ui->gdbLabel->setText("GDB is missing!");
        ui->gdbIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-error")).pixmap(20, 20));
        m_allToolsFound = false;
    } else {
        ui->gdbLabel->setText("GDB found.");
        ui->gdbIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-checked")).pixmap(20, 20));
    }
    ui->extraInfoWidget->setVisible(false);

    if (m_allToolsFound)
        ui->warnLabel->setVisible(false);
    else
        ui->warnLabel->setText("Some important tools were not found!\nThe generated report may be incomplete.");
}

CrashReportDialog::~CrashReportDialog()
{
    delete ui;
}

void CrashReportDialog::on_closeButton_clicked()
{
    QApplication::quit();
}

extern QString generateReport()
{
    QString report;
    JournalCollector journal;

    qDebug().noquote() << "Finding Syntalos-related journal entries...";
    journal.findJournalEntries("syntalos");

    qDebug().noquote() << "Generating report.";
    report += QStringLiteral("# Syntalos Crash Report (generated: %1)\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    report += "### Log Messages\n";
    if (journal.messageEntries().isEmpty()) {
        report += "- None found!\n";
    } else {
        const auto msgEntries = journal.messageEntries();
        for (int i = 0; i < msgEntries.length(); i++) {
            const auto entry = msgEntries[i];
            report += QStringLiteral("[%1] %2 :: %3 - %4\n%5\n").arg(entry.bootID,
                                                                     QString::number(entry.priority),
                                                                     entry.time.toString(Qt::ISODate),
                                                                     entry.unit,
                                                                     entry.message);
            if (i >= 50)
                break;
        }
        if (msgEntries.length() > 50)
            report += QStringLiteral("- List truncated, included only the last 50 of %1 entries.\n").arg(msgEntries.length());
    }

    const auto coredumpEntries = journal.coredumpEntries();
    report += "\n### Latest Crash Backtrace\n";
    if (coredumpEntries.length() < 1) {
        report += "- No recent crashes found!\n";
        return report;
    }
    const auto coredumpEntry = coredumpEntries.first();
    report += journal.generateBacktrace(coredumpEntry);

    return report;
}

void CrashReportDialog::on_nextButton_clicked()
{
    const auto currentPage = ui->pageStack->currentWidget();
    if (currentPage == ui->pageIntro) {
        // Switch from intro page to processing
        ui->pageStack->setCurrentWidget(ui->pageProcessing);
        ui->closeButton->setEnabled(false);
        ui->nextButton->setEnabled(false);

        ui->progressBar->setRange(0, 0);
        // TODO: Make use of QPromise when we can switch to Qt6 to display
        // a proper progress bar.
        QFuture<QString> future = QtConcurrent::run(generateReport);
        while (!future.isFinished())
            QApplication::processEvents();

        qDebug().noquote() << "Received report data.";
        m_lastMdReport = future.result();

        ui->pageStack->setCurrentWidget(ui->pageResult);
        ui->nextButton->setText("Save Report");
        ui->nextButton->setIcon(QIcon::fromTheme("document-save-as"));
        ui->nextButton->setEnabled(true);
        ui->closeButton->setEnabled(true);

        ui->doneInfoLabel->setText(QStringLiteral("<html><b>All done!</b><br/>"
                                                  "You can now save the generated report to disk for sharing. Click on <i>Save Report</i> to save it."));
        if (!m_allToolsFound)
            ui->doneInfoLabel->setText(ui->doneInfoLabel->text() + QStringLiteral("<br/>"
                                                                                  "Please keep in mind that this report may be <b>incomplete</b> "
                                                                                  "due to missing analysis tools on this system."));

        return;
    }

    if (currentPage == ui->pageResult) {
        // Switch from result page to saving data and exiting

        QString fileName;
        QString fnameSuggestion = QStringLiteral("SyntalosCrashReport_%1.md").arg(QDateTime::currentDateTime().toString("yyddMM"));
        fileName = QFileDialog::getSaveFileName(this,
                                                "Select Report Filename",
                                                QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/" + fnameSuggestion,
                                                "Text Files (*.md)");

        if (fileName.isEmpty())
            return;

        // save result
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Unable to save file", QStringLiteral("Failed to save file: %1").arg(file.errorString()));
            return;
        }

        QTextStream out(&file);
        out << m_lastMdReport << "\n";

        qDebug().noquote() << "Crash info saved:" << fileName;
        QApplication::quit();
    }
}
