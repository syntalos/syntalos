/*
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "debugcollect.h"

#include "config.h"
#include <QFileInfo>
#include <string.h>
#include <systemd/sd-journal.h>

#include "utils.h"

JournalCollector::JournalCollector()
{
    QFile file(QLatin1String("/proc/sys/kernel/random/boot_id"));
    if (file.open(QIODevice::ReadOnly | QFile::Text)) {
        QTextStream stream(&file);
        m_currentBootId = stream.readAll().trimmed();
        m_currentBootId.remove(QChar::fromLatin1('-'));
    }
}

QString JournalCollector::lastError() const
{
    return m_lastError;
}

static JournalEntry readJournalEntry(sd_journal *journal)
{
    JournalEntry entry;
    const void *data;
    size_t length;
    uint64_t time;
    int res;

    res = sd_journal_get_realtime_usec(journal, &time);
    if (res == 0)
        entry.time.setMSecsSinceEpoch(time / 1000);

    res = sd_journal_get_data(journal, "SYSLOG_IDENTIFIER", &data, &length);
    if (res == 0) {
        entry.unit = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);
    } else {
        res = sd_journal_get_data(journal, "_SYSTEMD_UNIT", &data, &length);
        if (res == 0) {
            entry.unit = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);
        }
    }

    res = sd_journal_get_data(journal, "MESSAGE_ID", &data, &length);
    if (res == 0)
        entry.id = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);

    res = sd_journal_get_data(journal, "MESSAGE", &data, &length);
    if (res == 0)
        entry.message = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);

    res = sd_journal_get_data(journal, "PRIORITY", &data, &length);
    if (res == 0)
        entry.priority = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1).toInt();

    res = sd_journal_get_data(journal, "_BOOT_ID", &data, &length);
    if (res == 0)
        entry.bootID = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);

    res = sd_journal_get_data(journal, "COREDUMP_FILENAME", &data, &length);
    if (res == 0)
        entry.coredumpFname = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);

    res = sd_journal_get_data(journal, "COREDUMP_EXE", &data, &length);
    if (res == 0) {
        entry.coredumpExe = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);

        res = sd_journal_get_data(journal, "COREDUMP_SIGNAL", &data, &length);
        if (res == 0) {
#if HAVE_SIGDESCR_NP
            entry.coredumpSignal = sigdescr_np(
                QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1).toInt());
#else
            entry.coredumpSignal = QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1);
#endif
        } else {
            entry.coredumpSignal = "<unknown signal>";
        }
    }

    return entry;
}

bool JournalCollector::findCoredumpEntries(const QString &exeNameFilter, int limit)
{
    sd_journal *journal;

    m_lastError.clear();
    int res = sd_journal_open(&journal, SD_JOURNAL_CURRENT_USER | SD_JOURNAL_SYSTEM);
    if (res < 0) {
        m_lastError = "Failed to access the journal.";
        return false;
    }

    const auto jFilters = QStringList() << "SYSLOG_IDENTIFIER=systemd-coredump";

    for (const QString &filter : jFilters) {
        res = sd_journal_add_match(journal, filter.toUtf8().constData(), 0);
        if (res < 0) {
            m_lastError = "Failed to add journal match filter.";
            sd_journal_close(journal);
            return false;
        }
    }

    res = sd_journal_seek_tail(journal);
    if (res < 0) {
        m_lastError = "Failed to seek journal tail.";
        sd_journal_close(journal);
        return false;
    }

    // check found entries
    QList<JournalEntry> syCoredumps;
    int count = 0;
    while (true) {
        res = sd_journal_previous(journal);
        if (res < 0) {
            m_lastError = "Failed to access next journal entry.";
            break;
        }
        if (res == 0) {
            // last journal entry reached
            break;
        }

        auto entry = readJournalEntry(journal);
        if (!entry.coredumpExe.isEmpty() && QFileInfo(entry.coredumpExe).baseName().startsWith(exeNameFilter)) {
            syCoredumps.append(entry);
            count++;
            if (limit > 0 && count >= limit)
                break;
        }
    }

    m_coredumpEntries = syCoredumps;
    sd_journal_close(journal);
    return m_lastError.isEmpty();
}

bool JournalCollector::findMessageEntries(const QString &keywordFilter, int limit)
{
    sd_journal *journal;

    m_lastError.clear();
    int res = sd_journal_open(&journal, SD_JOURNAL_CURRENT_USER | SD_JOURNAL_SYSTEM);
    if (res < 0) {
        m_lastError = "Failed to access the journal.";
        return false;
    }

    if (m_currentBootId.isEmpty()) {
        m_lastError = "Boot ID is empty (Likely failed to open /proc/sys/kernel/random/boot_id)";
        return false;
    }

    const auto jFilters = QStringList() << QStringLiteral("_BOOT_ID=%1").arg(m_currentBootId);

    for (const QString &filter : jFilters) {
        res = sd_journal_add_match(journal, filter.toUtf8().constData(), 0);
        if (res < 0) {
            m_lastError = "Failed to add journal match filter.";
            sd_journal_close(journal);
            return false;
        }
    }

    res = sd_journal_seek_tail(journal);
    if (res < 0) {
        m_lastError = "Failed to seek journal tail.";
        sd_journal_close(journal);
        return false;
    }

    // check found entries
    int count = 0;
    QList<JournalEntry> syMessages;
    while (true) {
        res = sd_journal_previous(journal);
        if (res < 0) {
            m_lastError = "Failed to access next journal entry.";
            break;
        }
        if (res == 0) {
            // last journal entry reached
            break;
        }

        auto entry = readJournalEntry(journal);
        if (entry.unit.contains(keywordFilter) || entry.message.contains(keywordFilter, Qt::CaseInsensitive)) {
            syMessages.append(entry);
            count++;
            if (limit > 0 && count >= limit)
                break;
        }
    }

    m_messageEntries = syMessages;
    sd_journal_close(journal);
    return m_lastError.isEmpty();
}

bool JournalCollector::exportCoredumpFile(const JournalEntry &journalEntry, const QString &outFname, QString *details)
{
    const auto cdctlExe = QStandardPaths::findExecutable("coredumpctl");
    if (cdctlExe.isEmpty()) {
        m_lastError = QStringLiteral("Failed to generate a backtrace for '%1': coredumpctl was not found.")
                          .arg(journalEntry.coredumpFname);
        return false;
    }

    // export our core file
    QProcess cdctlProc;
    cdctlProc.setProcessChannelMode(QProcess::MergedChannels);
    cdctlProc.setProgram(cdctlExe);
    cdctlProc.setArguments(
        QStringList() << "dump" << QStringLiteral("MESSAGE_ID=%1").arg(journalEntry.id) << "-o" << outFname);
    cdctlProc.start();
    bool ret = cdctlProc.waitForFinished(60 * 1000);
    if (!ret) {
        m_lastError = QStringLiteral("Failed to generate a backtrace for '%1': coredumctl timed out.")
                          .arg(journalEntry.id);
        return false;
    }
    if (cdctlProc.exitCode() != 0) {
        m_lastError = QStringLiteral("Failed to generate a backtrace for '%1':\n%2")
                          .arg(journalEntry.coredumpFname, QString::fromUtf8(cdctlProc.readAllStandardOutput()));
        return false;
    }
    if (details != nullptr)
        *details = cdctlProc.readAllStandardOutput();

    return true;
}

QString JournalCollector::generateBacktrace(const JournalEntry &journalEntry)
{
    const auto gdbExe = QStandardPaths::findExecutable("gdb");
    if (gdbExe.isEmpty())
        return QStringLiteral("Failed to generate a backtrace for '%1': GDB was not found.")
            .arg(journalEntry.coredumpFname);

    QTemporaryFile tmpCoreFile(QDir::tempPath() + "/syntalos-retrace_XXXXXX");
    if (!tmpCoreFile.open())
        return QStringLiteral("Failed to create temporary file '%1': %2")
            .arg(tmpCoreFile.fileName(), tmpCoreFile.errorString());
    tmpCoreFile.setAutoRemove(true);

    // fetch the coredump file first
    QString cdctlDetails;
    if (!exportCoredumpFile(journalEntry, tmpCoreFile.fileName(), &cdctlDetails))
        return QStringLiteral("Failed to generate a backtrace for '%1': %2.")
            .arg(journalEntry.coredumpFname, m_lastError);

    // create a backtrace with gdb
    QProcess gdbProc;
    gdbProc.setProcessChannelMode(QProcess::MergedChannels);
    gdbProc.setProgram(gdbExe);
    gdbProc.setArguments(
        QStringList() << "-batch"
                      << "-ex"
                      << "thread apply all bt full"
                      << "-c" << tmpCoreFile.fileName() << journalEntry.coredumpExe);
    gdbProc.start();
    bool ret = gdbProc.waitForFinished(60 * 1000);
    if (!ret)
        return QStringLiteral("Failed to generate a backtrace for '%1': GDB timed out.")
            .arg(journalEntry.coredumpFname);
    if (gdbProc.exitCode() != 0)
        return QStringLiteral("Failed to generate a backtrace for '%1':\n%2")
            .arg(journalEntry.coredumpFname, QString::fromUtf8(gdbProc.readAllStandardOutput()));
    return cdctlDetails + "\n------------\n" + QString::fromUtf8(gdbProc.readAllStandardOutput());
}

QString JournalCollector::currentBootId() const
{
    return m_currentBootId;
}

QList<JournalEntry> JournalCollector::coredumpEntries() const
{
    return m_coredumpEntries;
}

QList<JournalEntry> JournalCollector::messageEntries() const
{
    return m_messageEntries;
}

QString generateBacktraceForRunningProcess(const QString &procName, bool *processFound)
{
    if (processFound != nullptr)
        *processFound = true;
    const auto pid = findFirstProcIdByName(procName.toStdString());
    if (pid <= 0) {
        if (processFound != nullptr)
            *processFound = false;
        return QStringLiteral("Error: No running Syntalos process found!");
    }

    const auto gdbExe = QStandardPaths::findExecutable("gdb");
    if (gdbExe.isEmpty())
        return QStringLiteral("Failed to generate a backtrace for '%1': GDB was not found.").arg(procName);

    // create a backtrace with gdb
    QProcess gdbProc;
    gdbProc.setProcessChannelMode(QProcess::MergedChannels);
    gdbProc.setProgram(gdbExe);
    gdbProc.setArguments(
        QStringList() << "-batch"
                      << "-ex" << QStringLiteral("attach %1").arg(pid) << "-ex"
                      << "thread apply all bt full"
                      << "-ex"
                      << "detach");
    gdbProc.start();
    bool ret = gdbProc.waitForFinished(60 * 1000);
    if (!ret)
        return QStringLiteral("Failed to generate a backtrace for '%1': GDB timed out.").arg(procName);
    if (gdbProc.exitCode() != 0)
        return QStringLiteral("Failed to generate a backtrace for '%1':\n%2")
            .arg(procName, QString::fromUtf8(gdbProc.readAllStandardOutput()));
    return QString::fromUtf8(gdbProc.readAllStandardOutput());
}
