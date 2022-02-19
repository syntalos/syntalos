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

#pragma once

#include <QtCore>

struct JournalEntry {
    QDateTime time;
    QString id;
    QString unit;
    QString message;
    int priority;
    QString bootID;

    QString coredumpFname;
    QString coredumpExe;
    QString coredumpSignal;
};

class JournalCollector
{
public:
    JournalCollector();

    bool findJournalEntries(const QString &exeNameFilter);
    bool exportCoredumpFile(const JournalEntry &journalEntry, const QString &outFname, QString *details = nullptr);
    QString generateBacktrace(const JournalEntry &journalEntry);

    QList<JournalEntry> coredumpEntries() const;
    QList<JournalEntry> messageEntries() const;

private:
    QString m_lastError;
    QList<JournalEntry> m_coredumpEntries;
    QList<JournalEntry> m_messageEntries;
};

QString generateCoredumpBacktrace(const JournalEntry &journalEntry);

QString generateBacktraceForRunningProcess(const QString &procName, bool *processFound = nullptr);
