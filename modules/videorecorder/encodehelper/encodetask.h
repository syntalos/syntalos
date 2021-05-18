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

#pragma once

#include <QObject>
#include <QRunnable>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logEncodeTask)

class QueueItem;

class EncodeTask : public QRunnable
{
public:
    EncodeTask(QueueItem *item, bool updateAttrs);

    void run() override;

private:
    bool prepareSourceFiles();

private:
    QueueItem *m_item;
    bool m_updateAttrsData;
    QString m_datasetRoot;
    QString m_srcFname;
    QString m_destFname;

    bool m_writeTsync;
    QString m_tsyncSrcFname;
    QString m_tsyncDestFname;
};
