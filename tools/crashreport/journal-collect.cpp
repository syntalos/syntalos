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

#include "journal-collect.h"

#include <QFileInfo>
#include <systemd/sd-journal.h>

JournalCollector::JournalCollector()
{
}

struct JournalEntry {
    QDateTime time;
    QString unit;
    QString message;
    int priority;
    QString bootID;

    QString coredumpFname;
    QString coredumpExe;
    QString coredumpSignal;
};

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
        if (res == 0)
            entry.coredumpSignal = sigdescr_np(QString::fromUtf8((const char *)data, length).section(QChar::fromLatin1('='), 1).toInt());
        else
            entry.coredumpSignal = "<unknown signal>";
    }

    return entry;
}

bool JournalCollector::findLastCoredump()
{
    sd_journal *journal;

    m_lastError.clear();
    int res = sd_journal_open(&journal, SD_JOURNAL_CURRENT_USER | SD_JOURNAL_SYSTEM);
    if (res < 0) {
        m_lastError = "Failed to access the journal.";
        return false;
    }

    const auto jFilters = QStringList() << "CODE_FUNC=submit_coredump";

    for (const QString &filter : jFilters) {
        res = sd_journal_add_match(journal, filter.toUtf8().constData(), 0);
        if (res < 0) {
            m_lastError = "Failed to add journal match filter.";
            sd_journal_close(journal);
            return false;
        }
    }

    res = sd_journal_seek_head(journal);
    if (res < 0) {
        m_lastError = "Failed to seek journal head.";
        sd_journal_close(journal);
        return false;
    }

    // check found entries
    QList<JournalEntry> syCoredumps;
    QList<JournalEntry> syMessages;
    while (true) {
        res = sd_journal_next(journal);
        if (res < 0) {
            m_lastError = "Failed to access next journal entry.";
            break;
        }
        if (res == 0) {
            // last journal entry reached
            break;
        }

        auto entry = readJournalEntry(journal);
        if (!entry.coredumpExe.isEmpty() && QFileInfo(entry.coredumpExe).baseName().startsWith("syntalos"))
            syCoredumps.append(entry);
        else if (entry.message.contains("syntalos") || entry.unit.contains("syntalos"))
            syMessages.append(entry);
    }

    for (const auto& entry : syCoredumps)
        qDebug() << entry.coredumpFname << entry.coredumpSignal;
    for (const auto& entry : syMessages)
        qDebug() << entry.message;

    sd_journal_close(journal);
    return m_lastError.isEmpty();
}
