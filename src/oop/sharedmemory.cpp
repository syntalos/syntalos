/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sharedmemory.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <cstring>
#include <unistd.h>
#include <QUuid>
#include <QDebug>

SharedMemory::SharedMemory()
    : m_attached(false),
      m_data(nullptr),
      m_dataLen(0)
{
}

SharedMemory::~SharedMemory()
{
    int res;

    if (m_data != nullptr) {
        int fd;

        res = munmap(m_data, m_dataLen);
        if (res == -1) {
            m_lastError = QString::fromStdString(std::strerror(errno));
            // TODO: Catch error?
        }

        fd = shm_unlink(qPrintable(m_shmKey));
        if (fd == -1) {
            m_lastError = QString::fromStdString(std::strerror(errno));
            // TODO: Catch error?
        }
    }
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

static QString getCurrentThreadName()
{
    char threadName[16];
    pthread_getname_np(pthread_self(), threadName, sizeof(threadName));
    return QString::fromUtf8(threadName);
}

bool SharedMemory::create(size_t size)
{
    int res;
    int fd;

    if (m_data != nullptr) {
        m_lastError = QStringLiteral("Shared memory segment was already created.");
        return false;
    }

    if (m_shmKey.isEmpty()) {
        const auto threadName = getCurrentThreadName();
        const auto idstr = QUuid::createUuid().toString(QUuid::Id128);
        if (threadName.isEmpty())
            setShmKey(idstr);
        else
            setShmKey(QStringLiteral("%1_%2").arg(threadName).arg(idstr));
    }

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

    m_data = mmap(nullptr, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (m_data == MAP_FAILED) {
        setErrorFromErrno("create/mmap");
        return false;
    }
    m_dataLen = size;

    if (close(fd) != 0) {
        setErrorFromErrno("create/close");
        return false;
    }

    m_attached = true;
    return true;
}

bool SharedMemory::attach(bool writable)
{
    if (m_data != nullptr) {
        m_lastError = QStringLiteral("Shared memory segment was already attached.");
        return false;
    }

    const char *shmKey = qPrintable(m_shmKey);
    int fd = shm_open(shmKey, writable? O_RDWR : O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        setErrorFromErrno("attach/shm_open");
        return false;
    }

    struct stat sbuf;
    auto res = fstat(fd, &sbuf);
    if (res == 0) {
        m_dataLen = sbuf.st_size < 0? 0 : static_cast<size_t>(sbuf.st_size);
    } else {
        setErrorFromErrno(QString("attach/stat#%1").arg(res));
        return false;
    }

    m_data = mmap(nullptr, m_dataLen, writable? PROT_WRITE : PROT_READ, MAP_SHARED, fd, 0);
    if (m_data == MAP_FAILED) {
        setErrorFromErrno("attach/mmap");
        return false;
    }

    m_attached = true;
    return true;
}

bool SharedMemory::isAttached() const
{
    return m_attached;
}

void SharedMemory::setErrorFromErrno(const QString &hint)
{
    m_lastError = QStringLiteral("%1: %2").arg(hint).arg(QString::fromStdString(std::strerror(errno)));
}
