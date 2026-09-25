/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QObject>

class QSocketNotifier;

namespace Syntalos
{

/**
 * @brief Delivers SIGINT and SIGTERM as a Qt signal on the thread that created the watcher.
 *
 * Syntalos must not simply die on these signals: a running experiment would be cut off
 * without finalizing its data, and shared-memory resources of the IPC layer would be left behind.
 * Only one instance may exist per process.
 */
class TermSignalWatcher : public QObject
{
    Q_OBJECT
public:
    explicit TermSignalWatcher(QObject *parent = nullptr);
    ~TermSignalWatcher() override;

    /**
     * @brief Reinstall our handlers if something replaced them.
     * @return true if the handlers were still in place.
     */
    bool ensureHandlersInstalled();

signals:
    void terminationRequested(int signum);

private:
    QSocketNotifier *m_notifier = nullptr;
};

} // namespace Syntalos
