/*
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "taskmanager.h"

#include <QDebug>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QMessageBox>
#include <QThreadPool>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

#include "encodetask.h"

Q_LOGGING_CATEGORY(logEncodeMgr, "encoder.manager")

TaskManager::TaskManager(QueueModel *queue, QObject *parent)
    : QDBusAbstractAdaptor(parent),
      m_queue(queue),
      m_threadPool(new QThreadPool(this)),
      m_checkTimer(new QTimer(this)),
      m_idleInhibitFd(-1)
{
    auto maxThreads = QThread::idealThreadCount() - 2;
    if (maxThreads < 2)
        maxThreads = 2;
    setProperty("parallelCount", maxThreads);

    m_checkTimer->setInterval(1500);
    connect(m_checkTimer, &QTimer::timeout, this, &TaskManager::checkThreadPoolRunning);
    m_checkTimer->stop();
}

int TaskManager::parallelCount() const
{
    return m_threadPool->maxThreadCount();
}

void TaskManager::setParallelCount(int count)
{
    m_threadPool->setMaxThreadCount((count >= 1)? count : 1);
    emit parallelCountChanged(m_threadPool->maxThreadCount());
}

bool TaskManager::tasksAvailable()
{
    for (auto &item : m_queue->queueItems())
        if (item->status() == QueueItem::WAITING)
            return true;
    return false;
}

bool TaskManager::allTasksCompleted()
{
    for (auto &item : m_queue->queueItems())
        if ((item->status() != QueueItem::FAILED) &&
            (item->status() != QueueItem::FINISHED))
            return false;
    return true;
}

bool TaskManager::isRunning()
{
    return m_threadPool->activeThreadCount() > 0;
}

void TaskManager::checkThreadPoolRunning()
{
    if (!isRunning()) {
        m_checkTimer->stop();
        m_scheduledDSPaths.clear();
        emit encodingFinished();
        releaseSleepShutdownIdleInhibitor();
    }
}

bool TaskManager::enqueueVideo(const QString &projectId, const QString &videoFname,
                               const QHash<QString, QVariant> &codecProps,
                               const QHash<QString, QVariant> &mdata)
{
    CodecProperties cprops(codecProps);

    auto item = new QueueItem(projectId, videoFname, m_queue);
    item->setCodecProps(cprops);
    item->setMdata(mdata);

    m_queue->append(item);

    // we prohibit shutdown even if we just have stuff queued - all data should be processed
    // before the user can shutdown the system
    obtainSleepShutdownIdleInhibitor();

    // notify about the new task
    emit newTasksAvailable();
    return true;
}

bool TaskManager::processVideos()
{
    QSet<QueueItem*> rmItems;

    for (auto &item : m_queue->queueItems()) {
        if (item->status() == QueueItem::WAITING) {
            // start encoding new items
            item->setStatus(QueueItem::SCHEDULED);

            // allow codecs to use some multithreading (especially FFV1 benefits a lot from this)
            // we are certainly overcommitting the CPU here, but in reality this seems to work extremely
            // well for resource utilization and performance balance.
            int codecThreadCount = QThread::idealThreadCount() - m_threadPool->maxThreadCount() - 2;
            codecThreadCount = (codecThreadCount <= 1)? 2 : codecThreadCount;

            // we only set the "update attribute metadata" flag for the first
            // video in a dataset the we encounter. Otherwise we have multiple parallel
            // writers trying to write to the same file, which causes ugly race conditions
            QFileInfo fi(item->fname());
            const auto datasetRoot = fi.dir().path();

            auto task = new EncodeTask(item,
                                       !m_scheduledDSPaths.contains(datasetRoot),
                                       codecThreadCount);
            m_scheduledDSPaths.insert(datasetRoot);

            m_threadPool->start(task);
        } else if (item->status() == QueueItem::FINISHED) {
            // remove successfuly completed entries
            rmItems.insert(item);
        }
    }

    // FIXME: Queue cleanup doesn't work properly yet
    //m_queue->remove(rmItems);

    m_checkTimer->start();
    emit encodingStarted();
    return true;
}

void TaskManager::obtainSleepShutdownIdleInhibitor()
{
    if (m_idleInhibitFd >= 0)
        return;
    QDBusInterface iface(QStringLiteral("org.freedesktop.login1"),
                         QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"),
                         QDBusConnection::systemBus());
    if (!iface.isValid()) {
        qCDebug(logEncodeMgr).noquote() << "Unable to connect to logind DBus interface";
        m_idleInhibitFd = -1;
    }

    QDBusReply<QDBusUnixFileDescriptor> reply;
    reply = iface.call(QStringLiteral("Inhibit"),
                       QStringLiteral("sleep:shutdown:idle"),
                       QCoreApplication::applicationName(),
                       QStringLiteral("Encoding video datasets"),
                       QStringLiteral("block"));
    if (!reply.isValid()) {
        qCDebug(logEncodeMgr).noquote() << "Unable to request sleep/shutdown/idle inhibitor from logind.";
        m_idleInhibitFd = -1;
    }

    m_idleInhibitFd = ::dup(reply.value().fileDescriptor());
}

void TaskManager::releaseSleepShutdownIdleInhibitor()
{
    if (m_idleInhibitFd != -1)
        ::close(m_idleInhibitFd);
}
