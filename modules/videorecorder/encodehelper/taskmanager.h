/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

public:
    /**
     * @brief Start encoding all waiting videos without checking for free disk space.
     */
    void startEncoding();

    /**
     * @brief Estimate whether enough disk space is available to encode all waiting videos.
     * @return An empty string if there is enough space, or a human-readable description of
     *         the shortage otherwise.
     */
    QString checkDiskSpaceForPendingTasks() const;

signals:
    void newTasksAvailable();
    void encodingStarted();
    void encodingFinished();
    void parallelCountChanged(int count);

    /**
     * @brief Emitted when encoding was requested, but the disk holding the videos is too full.
     *
     * The queue is not started in this case. The user should be asked whether they
     * want to proceed anyway, in which case startEncoding() should be called.
     */
    void lowDiskSpaceConfirmationNeeded(const QString &message);

private slots:
    void checkThreadPoolRunning();

private:
    void obtainSleepShutdownIdleInhibitor();
    void releaseSleepShutdownIdleInhibitor();

private:
    QuillLogger *m_log;
    QueueModel *m_queue;
    QThreadPool *m_threadPool;
    QSet<QString> m_scheduledDSPaths;
    QTimer *m_checkTimer;
    int m_idleInhibitFd;
};
