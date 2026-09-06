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

#include "taskmanager.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QThreadPool>
#include <QTimer>
#include <unistd.h>

#include <algorithm>

#include "encodetask.h"
#include "utils/misc.h"
#include "utils/resourceinfo.h"

TaskManager::TaskManager(QueueModel *queue, QObject *parent)
    : QDBusAbstractAdaptor(parent),
      m_queue(queue),
      m_threadPool(new QThreadPool(this)),
      m_checkTimer(new QTimer(this)),
      m_idleInhibitFd(-1)
{
    m_log = getLogger("encoder.manager");

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
    m_threadPool->setMaxThreadCount((count >= 1) ? count : 1);
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
        if ((item->status() != QueueItem::FAILED) && (item->status() != QueueItem::FINISHED))
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

bool TaskManager::enqueueVideo(
    const QString &projectId,
    const QString &videoFname,
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

QString TaskManager::checkDiskSpaceForPendingTasks() const
{
    // Each running task writes its encoded output next to the source file, and the source
    // is only deleted once the task has completed. So while N tasks run in parallel, we may
    // temporarily need the size of the N largest encoded outputs as free space.
    // The source is already losslessly compressed (FFVHuff), so a stronger lossless target codec
    // may end up about as large as the source in the worst case (noisy data), while lossy
    // codecs produce only a fraction of it. The lossy factor is deliberately generous,
    // so even unusually high bitrate settings are covered.
    const int parallelCount = std::max(m_threadPool->maxThreadCount(), 1);
    const auto estimateOutputSize = [](qint64 sourceSize, const CodecProperties &cprops) -> qint64 {
        const double factor = cprops.isLossless() ? 0.9 : 0.5;
        return static_cast<qint64>(sourceSize * factor);
    };

    // collect source sizes per filesystem (the queue may hold videos of multiple runs)
    struct FsSpace {
        Syntalos::DiskSpaceInfo disk;
        QList<qint64> sizes;
    };
    QHash<quint64, FsSpace> spaceByFs;
    for (const auto &item : m_queue->queueItems()) {
        if (item->status() != QueueItem::WAITING)
            continue;
        const QFileInfo fi(item->fname());
        if (!fi.exists())
            continue;

        const auto disk = Syntalos::diskSpaceInfo(fi.absolutePath());
        if (!disk.valid) {
            LOG_WARNING(m_log, "Unable to determine free disk space for '{}'", item->fname());
            continue;
        }
        auto &fsSpace = spaceByFs[disk.deviceId];
        fsSpace.disk = disk;
        fsSpace.sizes.append(estimateOutputSize(fi.size(), item->codecProps()));
    }

    QStringList problems;
    for (auto &fsSpace : spaceByFs) {
        auto &sizes = fsSpace.sizes;
        std::sort(sizes.begin(), sizes.end(), std::greater<>());

        qint64 required = 0;
        for (int i = 0; i < std::min(static_cast<int>(sizes.size()), parallelCount); ++i)
            required += sizes[i];

        const auto available = fsSpace.disk.bytesAvailable;
        LOG_INFO(
            m_log,
            "Disk space on '{}': {} MB available, estimated {} MB needed for encoding",
            fsSpace.disk.mountPoint,
            available / 1000 / 1000,
            required / 1000 / 1000);
        if (available < required) {
            problems.append(QStringLiteral(
                                "The disk mounted at '%1' has only %2 free, but encoding the queued videos "
                                "may need up to %3 of temporary space.")
                                .arg(
                                    fsSpace.disk.mountPoint,
                                    Syntalos::formatByteSize(available),
                                    Syntalos::formatByteSize(required)));
        }
    }

    return problems.join(QStringLiteral("\n"));
}

bool TaskManager::processVideos()
{
    const auto spaceProblem = checkDiskSpaceForPendingTasks();
    if (!spaceProblem.isEmpty()) {
        LOG_WARNING(m_log, "Not starting encoding queue due to low disk space: {}", spaceProblem);
        emit lowDiskSpaceConfirmationNeeded(spaceProblem);
        return true;
    }

    startEncoding();
    return true;
}

void TaskManager::startEncoding()
{
    QSet<QueueItem *> rmItems;

    for (auto &item : m_queue->queueItems()) {
        if (item->status() == QueueItem::WAITING) {
            // start encoding new items
            item->setStatus(QueueItem::SCHEDULED);

            // allow codecs to use some multithreading (especially FFV1 benefits a lot from this)
            // we are certainly overcommitting the CPU here, but in reality this seems to work extremely
            // well for resource utilization and performance balance.
            int codecThreadCount = QThread::idealThreadCount() - m_threadPool->maxThreadCount() - 2;
            codecThreadCount = (codecThreadCount <= 1) ? 2 : codecThreadCount;

            // we only set the "update attribute metadata" flag for the first
            // video in a dataset the we encounter. Otherwise we have multiple parallel
            // writers trying to write to the same file, which causes ugly race conditions
            QFileInfo fi(item->fname());
            const auto datasetRoot = fi.absoluteDir().canonicalPath();

            auto task = new EncodeTask(item, !m_scheduledDSPaths.contains(datasetRoot), codecThreadCount);
            m_scheduledDSPaths.insert(datasetRoot);

            m_threadPool->start(task);
        } else if (item->status() == QueueItem::FINISHED) {
            // remove successfuly completed entries
            rmItems.insert(item);
        }
    }

    // FIXME: Queue cleanup doesn't work properly yet
    // m_queue->remove(rmItems);

    m_checkTimer->start();
    emit encodingStarted();
}

void TaskManager::obtainSleepShutdownIdleInhibitor()
{
    if (m_idleInhibitFd >= 0)
        return;
    QDBusInterface iface(
        QStringLiteral("org.freedesktop.login1"),
        QStringLiteral("/org/freedesktop/login1"),
        QStringLiteral("org.freedesktop.login1.Manager"),
        QDBusConnection::systemBus());
    if (!iface.isValid()) {
        LOG_INFO(m_log, "Unable to connect to logind DBus interface");
        m_idleInhibitFd = -1;
        return;
    }

    QDBusReply<QDBusUnixFileDescriptor> reply;
    reply = iface.call(
        QStringLiteral("Inhibit"),
        QStringLiteral("sleep:shutdown:idle"),
        QCoreApplication::applicationName(),
        QStringLiteral("Encoding video datasets"),
        QStringLiteral("block"));
    if (!reply.isValid()) {
        LOG_INFO(m_log, "Unable to request sleep/shutdown/idle inhibitor from logind.");
        m_idleInhibitFd = -1;
        return;
    }

    m_idleInhibitFd = ::dup(reply.value().fileDescriptor());
}

void TaskManager::releaseSleepShutdownIdleInhibitor()
{
    if (m_idleInhibitFd != -1) {
        ::close(m_idleInhibitFd);
        m_idleInhibitFd = -1;
    }
}
