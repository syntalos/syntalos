/*
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDBusAbstractAdaptor>
#include <QDBusVariant>

#include "../equeueshared.h"
#include "queuemodel.h"

Q_DECLARE_LOGGING_CATEGORY(logEncodeMgr)

class QThreadPool;

class TaskManager : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", EQUEUE_DBUS_MANAGERINTF)
    Q_PROPERTY(int parallelCount READ parallelCount WRITE setParallelCount)
public:
    explicit TaskManager(QueueModel *queue, QObject *parent = nullptr);

    int parallelCount() const;

    bool tasksAvailable();
    bool allTasksCompleted();

    bool isRunning();

public slots:
    void setParallelCount(int count);
    bool enqueueVideo(
        const QString &projectId,
        const QString &videoFname,
        const QHash<QString, QVariant> &codecProps,
        const QHash<QString, QVariant> &mdata);

    bool processVideos();

signals:
    void newTasksAvailable();
    void encodingStarted();
    void encodingFinished();
    void parallelCountChanged(int count);

private slots:
    void checkThreadPoolRunning();

private:
    void obtainSleepShutdownIdleInhibitor();
    void releaseSleepShutdownIdleInhibitor();

private:
    QueueModel *m_queue;
    QThreadPool *m_threadPool;
    QSet<QString> m_scheduledDSPaths;
    QTimer *m_checkTimer;
    int m_idleInhibitFd;
};
