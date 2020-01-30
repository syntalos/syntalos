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

#pragma once

#include <QObject>
#include <semaphore.h>

class SharedMemory
{
public:
    SharedMemory();
    ~SharedMemory();

    void createShmKey();
    void setShmKey(const QString& key);
    QString shmKey() const;

    QString lastError() const;

    size_t size() const;
    void *data();

    bool create(size_t size);
    bool attach();

    void lock();
    void unlock();

    bool isAttached() const;

private:
    void setErrorFromErrno(const QString& hint);
    QString m_shmKey;
    QString m_lastError;

    bool m_attached;
    void *m_data;
    size_t m_dataLen;
    void *m_shmPtr;
    size_t m_shmLen;
    sem_t *m_mutex;
};
