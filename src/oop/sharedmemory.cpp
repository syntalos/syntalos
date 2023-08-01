/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "sharedmemory.h"

#include <QDebug>
#include <QUuid>
#include <cstring>
#include <fcntl.h>
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

SharedMemory::SharedMemory()
    : m_attached(false),
      m_data(nullptr),
      m_dataLen(0),
      m_shmPtr(nullptr),
      m_shmLen(0)
{
}

SharedMemory::~SharedMemory()
{
    int res;

    if (m_shmPtr != nullptr) {
        int fd;

        qDebug() << "Unlinking shared memory:" << m_shmKey;

        res = sem_destroy(m_mutex);
        if (res == -1) {
            m_lastError = QString::fromStdString(std::strerror(errno));
            qWarning().noquote() << "Semaphore destruction in shared memory failed:" << m_lastError;
            // TODO: Catch error?
        }

        res = munmap(m_shmPtr, m_shmLen);
        if (res == -1) {
            m_lastError = QString::fromStdString(std::strerror(errno));
            qWarning().noquote() << "Shared memory unmap (size:" << m_shmLen << ") failed:" << m_lastError;
            // TODO: Catch error?
        }

        fd = shm_unlink(qPrintable(m_shmKey));
        if (fd == -1) {
            m_lastError = QString::fromStdString(std::strerror(errno));
            qWarning().noquote() << "Shared memory unlink failed:" << m_lastError;
            // TODO: Catch error?
        }
    }
}

static QString getCurrentThreadName()
{
    char threadName[16];
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));
    return QString::fromUtf8(threadName);
}

void SharedMemory::createShmKey()
{
    const auto threadName = getCurrentThreadName();
    const auto idstr = QUuid::createUuid().toString(QUuid::Id128);
    if (threadName.isEmpty())
        setShmKey(idstr);
    else
        setShmKey(QStringLiteral("%1_%2").arg(threadName).arg(idstr));
}

void SharedMemory::setShmKey(const QString &key)
{
    m_shmKey = key;
    if (!key.startsWith('/'))
        m_shmKey = QStringLiteral("/") + m_shmKey.replace('/', '_');
}

QString SharedMemory::shmKey() const
{
    return m_shmKey;
}

QString SharedMemory::lastError() const
{
    return m_lastError;
}

size_t SharedMemory::size() const
{
    return m_dataLen;
}

void *SharedMemory::data()
{
    return m_data;
}

bool SharedMemory::create(size_t size)
{
    int res;
    int fd;

    if (m_data != nullptr) {
        m_lastError = QStringLiteral("Shared memory segment was already created.");
        return false;
    }

    if (m_shmKey.isEmpty())
        createShmKey();

    fd = shm_open(qPrintable(m_shmKey), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        setErrorFromErrno("create/shm_open");
        return false;
    }

    res = ftruncate(fd, static_cast<off_t>(size));
    if (res != 0) {
        setErrorFromErrno("create/ftruncate");
        return false;
    }

    m_shmLen = size + sizeof(sem_t);
    m_shmPtr = mmap(nullptr, m_shmLen, PROT_WRITE, MAP_SHARED, fd, 0);
    if (m_shmPtr == MAP_FAILED) {
        setErrorFromErrno("create/mmap");
        return false;
    }

    m_mutex = static_cast<sem_t *>(m_shmPtr);
    m_data = static_cast<int *>(m_shmPtr) + sizeof(sem_t);
    if (sem_init(m_mutex, 1, 1) < 0) {
        setErrorFromErrno("semaphore initialization");
        return false;
    }

    if (close(fd) != 0) {
        setErrorFromErrno("create/close");
        return false;
    }

    qDebug() << "Created shared memory:" << m_shmKey;
    m_dataLen = size;
    m_attached = true;
    return true;
}

bool SharedMemory::attach()
{
    if (m_data != nullptr) {
        m_lastError = QStringLiteral("Shared memory segment was already attached.");
        return false;
    }

    const char *shmKey = qPrintable(m_shmKey);
    int fd = shm_open(shmKey, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        setErrorFromErrno("attach/shm_open");
        return false;
    }

    struct stat sbuf;
    auto res = fstat(fd, &sbuf);
    if (res == 0) {
        m_shmLen = sbuf.st_size < 0 ? 0 : static_cast<size_t>(sbuf.st_size);
    } else {
        setErrorFromErrno(QString("attach/stat#%1").arg(res));
        return false;
    }

    // we always needs to map this writable, as we may need to lock the semaphore that is in writable memory
    // NOTE: If we want to restrict access to the shared memory region more, we could use named system semaphores
    // instead in future.
    m_shmPtr = mmap(nullptr, m_shmLen, PROT_WRITE, MAP_SHARED, fd, 0);
    if (m_shmPtr == MAP_FAILED) {
        setErrorFromErrno("attach/mmap");
        return false;
    }

    // fetch pointer to semaphore
    m_mutex = static_cast<sem_t *>(m_shmPtr);

    m_dataLen = m_shmLen - sizeof(sem_t);
    m_data = static_cast<int *>(m_shmPtr) + sizeof(sem_t);

    qDebug() << "Attached shared memory:" << m_shmKey;
    m_attached = true;
    return true;
}

void SharedMemory::lock()
{
    while (sem_wait(m_mutex) != 0) {
    }
}

void SharedMemory::unlock()
{
    sem_post(m_mutex);
}

bool SharedMemory::isAttached() const
{
    return m_attached;
}

void SharedMemory::setErrorFromErrno(const QString &hint)
{
    m_lastError = QStringLiteral("%1: %2").arg(hint).arg(QString::fromStdString(std::strerror(errno)));
}
