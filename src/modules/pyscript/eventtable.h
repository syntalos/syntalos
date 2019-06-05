/*
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef EVENTTABLE_H
#define EVENTTABLE_H

#include <QObject>

class QTableWidget;
class QFile;

class EventTable : public QObject
{
    Q_OBJECT
public:
    explicit EventTable(const QString &dirPath, const QString& name, QObject *parent = nullptr);
    ~EventTable();

    QString name() const;
    bool open();

    void show();
    void hide();

    void setHeader(const QStringList &headers);
    void addEvent(const QStringList &data);

    const QRect &geometry() const;
    void setGeometry(const QRect& rect);

signals:

public slots:

private:
    QTableWidget *m_tableWidget;
    QFile *m_eventFile;
    QString m_eventFileName;
    QString m_name;
    bool m_haveEvents;
};

#endif // EVENTTABLE_H
