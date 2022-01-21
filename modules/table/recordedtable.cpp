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

#include "recordedtable.h"

#include <QDebug>
#include <QFile>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>

RecordedTable::RecordedTable(QObject *parent)
    : QObject(parent),
      m_name(QString())
{
    m_tableWidget = new QTableWidget;
    m_tableWidget->setWindowTitle(QStringLiteral("Table"));
    m_tableWidget->setWindowIcon(QIcon(":/module/table"));
    m_tableWidget->horizontalHeader()->hide();

    m_eventFile = new QFile;
    m_haveEvents = false;
}

RecordedTable::~RecordedTable()
{
    delete m_tableWidget;
    delete m_eventFile;
}

QString RecordedTable::name() const
{
    return m_name;
}

void RecordedTable::setName(const QString &name)
{
    m_name = name;
    m_tableWidget->setWindowTitle(m_name);
}

bool RecordedTable::open(const QString& fileName)
{
    m_eventFileName = fileName;
    close();
    m_eventFile->setFileName(m_eventFileName);
    return m_eventFile->open(QFile::WriteOnly | QFile::Truncate);
}

void RecordedTable::close()
{
    if (m_eventFile->isOpen())
        m_eventFile->close();
}

void RecordedTable::show()
{
    m_tableWidget->show();
}

void RecordedTable::hide()
{
    m_tableWidget->hide();
}

void RecordedTable::reset()
{
    m_tableWidget->setRowCount(0);
    m_haveEvents = false;
}

void RecordedTable::setHeader(const QStringList &headers)
{
    if (m_haveEvents) {
        QMessageBox::warning(m_tableWidget,
                             QStringLiteral("Warning"),
                             QStringLiteral("Can not change table headers after already receiving events."));
        return;
    }

    m_tableWidget->horizontalHeader()->show();
    m_tableWidget->setColumnCount(headers.count());
    m_tableWidget->setHorizontalHeaderLabels(headers);

    // write headers
    if (!m_eventFile->isOpen())
        return;
    QTextStream tsout(m_eventFile);
    auto csvHdr = headers;
    tsout << csvHdr.replaceInStrings(QStringLiteral(";"), QStringLiteral("；"))
                    .join(";") << "\n";
}

void RecordedTable::addRows(const QStringList &data)
{
    m_haveEvents = true;

    // write to file if file is opened
    if (!m_eventFile->isOpen())
            return;
    QTextStream tsout(m_eventFile);

    // since our tables are semicolon-separated, we replace the "regular" semicolon
    // with a unicode fullwith semicolon (U+FF1B). That way, users of the table module
    // can use pretty much any character they want and a machine-readable CSV table will be generated.
    auto csvRows = data;
    tsout << csvRows.replaceInStrings(QStringLiteral(";"), QStringLiteral("；"))
                    .join(";") << "\n";

    auto columnCount = m_tableWidget->columnCount();
    if (columnCount < data.count()) {
        // create necessary amount of columns
        if (columnCount == 0) {
            m_tableWidget->setColumnCount(data.count());
        } else {
            for (auto i = columnCount; i < data.count(); i++)
                m_tableWidget->insertColumn(i);
        }
    }

    auto lastRowId = m_tableWidget->rowCount();
    m_tableWidget->setRowCount(lastRowId + 1);

    for (auto i = 0; i < data.count(); i++) {
        auto item = new QTableWidgetItem(data.at(i));
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        m_tableWidget->setItem(lastRowId, i, item);
    }

    // scroll to the last item
    m_tableWidget->scrollToBottom();
}

const QRect &RecordedTable::geometry() const
{
    return m_tableWidget->geometry();
}

void RecordedTable::setGeometry(const QRect &rect)
{
    m_tableWidget->setGeometry(rect);
}

QWidget *RecordedTable::widget() const
{
    return m_tableWidget;
}
