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

#pragma once

#include <QIcon>
#include <QObject>

class QTableWidget;
class QFile;

class RecordedTable : public QObject
{
    Q_OBJECT
public:
    explicit RecordedTable(QObject *parent = nullptr, const QIcon &winIcon = QIcon());
    ~RecordedTable();

    QString name() const;
    void setName(const QString &name);

    bool open(const QString &fileName);
    void close();

    void show();
    void hide();

    void reset();
    void setHeader(const QStringList &headers);
    void addRows(const QStringList &data);

    const QRect &geometry() const;
    void setGeometry(const QRect &rect);

    QWidget *widget() const;

signals:

public slots:

private:
    QTableWidget *m_tableWidget;
    QFile *m_eventFile;
    QString m_eventFileName;
    QString m_name;
    bool m_haveEvents;
};
