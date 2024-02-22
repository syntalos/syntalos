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

#include <QAbstractTableModel>
#include <QMutex>
#include <QObject>
#include <QStyledItemDelegate>

#include "../videowriter.h"

class QueueItem : public QObject
{
    Q_OBJECT
public:
    enum QueueStatus {
        WAITING = 0,
        SCHEDULED,
        RUNNING,
        FINISHED,
        FAILED
    };

    QueueItem(const QString &projectId, const QString &fname, QObject *parent);

    QString projectId() const;
    QString videoId() const;
    QString fname() const;

    CodecProperties codecProps();
    void setCodecProps(const CodecProperties &props);

    QVariantHash mdata() const;
    void setMdata(const QVariantHash &mdata);

    QueueStatus status() const;
    void setStatus(QueueStatus status);

    int progress() const;
    void setProgress(int progress);

    QString errorMessage();
    void setError(const QString &text);

signals:
    void dataChanged();

private:
    QMutex m_mutex;

    QString m_projectId;
    QString m_videoId;
    QueueStatus m_status;
    int m_progress;
    QString m_fname;
    QString m_errorMsg;
    QVariantHash m_mdata;
    CodecProperties m_codecProps;
};

class QueueModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    QueueModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &) const override;
    int columnCount(const QModelIndex &) const override;

    QVariant data(const QModelIndex &index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void refresh();
    void append(QueueItem *queueItem);
    void remove(QSet<QueueItem *> rmItems);
    QList<QueueItem *> queueItems();
    QueueItem *itemByIndex(const QModelIndex &index);

private slots:
    void itemDataChanged();

private:
    QList<QueueItem *> m_data;
};

class ProgressBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    ProgressBarDelegate(QObject *parent = nullptr);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class HtmlDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit HtmlDelegate(QObject *parent = nullptr);

    QString anchorAt(QString html, const QPoint &point) const;

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
