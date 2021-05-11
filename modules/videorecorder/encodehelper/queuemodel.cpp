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

#include "queuemodel.h"

#include <QApplication>
#include <QFileInfo>

QueueItem::QueueItem(const QString &projectId, const QString &fname, QObject *parent)
    : QObject(parent),
      m_projectId(projectId),
      m_status(WAITING),
      m_progress(0),
      m_fname(fname)
{
    QFileInfo fi(fname);
    m_videoId = fi.baseName();
}

QString QueueItem::projectId() const
{
    return m_projectId;
}

QString QueueItem::videoId() const
{
    return m_videoId;
}

QString QueueItem::fname() const
{
    return m_fname;
}

CodecProperties QueueItem::codecProps()
{
    return m_codecProps;
}

void QueueItem::setCodecProps(const CodecProperties &props)
{
    m_codecProps = props;
}

QVariantHash QueueItem::mdata() const
{
    return m_mdata;
}

void QueueItem::setMdata(const QVariantHash &mdata)
{
    m_mdata = mdata;
}

QueueItem::QueueStatus QueueItem::status() const
{
    return m_status;
}

void QueueItem::setStatus(QueueItem::QueueStatus status)
{
    m_status = status;
    emit dataChanged();
}

int QueueItem::progress() const
{
    return m_progress;
}

void QueueItem::setProgress(int progress)
{
    if (m_progress == progress)
        return;
    m_progress = progress;
    emit dataChanged();
}

QString QueueItem::errorMessage() const
{
    return m_errorMsg;
}

void QueueItem::setError(const QString &text)
{
    m_errorMsg = text;
    m_status = FAILED;
    emit dataChanged();
}

QueueModel::QueueModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int QueueModel::rowCount(const QModelIndex &) const
{
    return m_data.count();
}

int QueueModel::columnCount(const QModelIndex &) const
{
    return 4;
}

QVariant QueueModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();

    const QueueItem *queueItem = m_data[index.row()];
    switch (index.column()) {
        case 0: return queueItem->projectId();
        case 1: return queueItem->videoId();
        case 2: {
            std::string str;
            switch (queueItem->status()){
            case QueueItem::WAITING :
                str = "Waiting";
                break;
            case QueueItem::SCHEDULED :
                str = "Scheduled";
                break;
            case QueueItem::RUNNING:
                str = "In Progress";
                break;
            case QueueItem::FINISHED:
                str = "Finished";
                break;
            case QueueItem::FAILED:
                str = "Failed";
                break;

            default:
                str = "Unkown";
            }
            return QString::fromStdString(str);
        }
        case 3: return queueItem->progress();
        default: return QVariant();
    };
}

QVariant QueueModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0: return QStringLiteral("Project");
    case 1: return QStringLiteral("Video");
    case 2: return QStringLiteral("Status");
    case 3: return QStringLiteral("Progress");
    default: return QVariant();
    }
}

void QueueModel::refresh()
{
    emit dataChanged(index(0, 0), index(m_data.count() - 1, 5));
}

void QueueModel::append(QueueItem *queueItem)
{
   beginInsertRows(QModelIndex(), m_data.count(), m_data.count());
   m_data.append(queueItem);
   endInsertRows();
   connect(queueItem, &QueueItem::dataChanged, this, &QueueModel::itemDataChanged);
}

void QueueModel::remove(QSet<QueueItem *> rmItems)
{
    beginRemoveRows(QModelIndex(), 0, m_data.count());

    QMutableListIterator<QueueItem*> i(m_data);
    while (i.hasNext()) {
        auto item = i.next();
        if (rmItems.contains(item))
            i.remove();
        disconnect(item, &QueueItem::dataChanged, this, &QueueModel::itemDataChanged);
    }
    endRemoveRows();
    refresh();
}

QList<QueueItem*> QueueModel::queueItems()
{
    return m_data;
}

void QueueModel::itemDataChanged()
{
    //auto item = qobject_cast<QueueItem*>(sender());
    emit dataChanged(index(0, 0), index(m_data.count() - 1, 5));
    //QModelIndex startOfRow = this->index(row, 0);
    //QModelIndex endOfRow   = this->index(row, Column::MaxColumns);
    //emit QAbstractItemModel::dataChanged(startOfRow, endOfRow);
}

ProgressBarDelegate::ProgressBarDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void ProgressBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int progress = index.data().toInt();

    QStyleOptionProgressBar progressBarOption;
    progressBarOption.rect = option.rect;
    progressBarOption.minimum = 0;
    progressBarOption.maximum = 100;
    progressBarOption.progress = progress;
    progressBarOption.text = QString::number(progress) + QStringLiteral(" %");
    progressBarOption.textVisible = true;

    QApplication::style()->drawControl(QStyle::CE_ProgressBar,
        &progressBarOption, painter);
}
