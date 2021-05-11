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
#include <QMessageBox>
#include <QThreadPool>

#include "encodetask.h"

TaskManager::TaskManager(QueueModel *queue, QObject *parent)
    : QDBusAbstractAdaptor(parent),
      m_queue(queue),
      m_threadPool(new QThreadPool(this))
{
    auto maxThreads = QThread::idealThreadCount() - 2;
    if (maxThreads < 2)
        maxThreads = 2;
    setProperty("parallelCount", maxThreads);
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

bool TaskManager::enqueueVideo(const QString &projectId, const QString &videoFname,
                               const QHash<QString, QVariant> &codecProps,
                               const QHash<QString, QVariant> &mdata)
{
    CodecProperties cprops(codecProps);

    auto item = new QueueItem(projectId, videoFname, m_queue);
    item->setCodecProps(cprops);
    item->setMdata(mdata);

    m_queue->append(item);
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
            auto task = new EncodeTask(item);
            m_threadPool->start(task);
        } else if (item->status() == QueueItem::FINISHED) {
            // remove successfuly completed entries
            rmItems.insert(item);
        }
    }

    // FIXME: Queue cleanup doesn't work properly yet
    //m_queue->remove(rmItems);

    emit encodingStarted();
    return true;
}
