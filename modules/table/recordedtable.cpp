/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QLabel>

RecordedTable::RecordedTable(QObject *parent, const QIcon &winIcon)
    : QObject(parent),
      m_name(QString()),
      m_saveData(true),
      m_displayData(true)
{
    const auto pos = QCursor::pos();
    m_tableBox = new QWidget;
    m_tableBox->setGeometry(pos.x(), pos.y(), 800, 260);
    m_tableBox->setWindowTitle(QStringLiteral("Table"));
    if (winIcon.isNull())
        m_tableBox->setWindowIcon(QIcon(":/module/table"));
    else
        m_tableBox->setWindowIcon(winIcon);

    m_tableWidget = new QTableWidget(m_tableBox);
    m_tableWidget->horizontalHeader()->hide();

    auto layout = new QVBoxLayout(m_tableBox);
    layout->setContentsMargins(2, 2, 2, 2);
    m_tableBox->setLayout(layout);
    layout->addWidget(m_tableWidget);

    m_infoLabel = new QLabel("Ready", m_tableBox);
    m_infoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_infoLabel->setFixedHeight(20);
    layout->addWidget(m_infoLabel);

    m_eventFile = new QFile;
    m_haveEvents = false;
    m_infoLabel->setVisible(false);
}

RecordedTable::~RecordedTable()
{
    delete m_tableBox;
    delete m_eventFile;
}

QString RecordedTable::name() const
{
    return m_name;
}

void RecordedTable::setName(const QString &name)
{
    m_name = name;
    m_tableBox->setWindowTitle(m_name);
}

bool RecordedTable::displayData() const
{
    return m_displayData;
}

bool RecordedTable::saveData() const
{
    return m_saveData;
}

void RecordedTable::setSaveData(bool save)
{
    m_saveData = save;
    updateInfoLabel();
}

void RecordedTable::setDisplayData(bool display)
{
    m_displayData = display;
    updateInfoLabel();
}

bool RecordedTable::open(const QString &fileName)
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
    m_tableBox->show();
}

void RecordedTable::hide()
{
    m_tableBox->hide();
}

void RecordedTable::reset()
{
    m_tableWidget->setRowCount(0);
    m_haveEvents = false;
}

void RecordedTable::setHeader(const QStringList &headers)
{
    if (m_haveEvents) {
        QMessageBox::warning(
            m_tableBox,
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
    tsout << csvHdr.replaceInStrings(QStringLiteral(";"), QStringLiteral("；")).join(";") << "\n";
}

void RecordedTable::addRows(const QStringList &data)
{
    m_haveEvents = true;

    if (m_saveData) {
        // write to file if file is opened
        if (!m_eventFile->isOpen())
            return;
        QTextStream tsout(m_eventFile);

        // since our tables are semicolon-separated, we replace the "regular" semicolon
        // with a unicode fullwith semicolon (U+FF1B). That way, users of the table module
        // can use pretty much any character they want and a machine-readable CSV table will be generated.
        auto csvRows = data;
        tsout << csvRows.replaceInStrings(QStringLiteral(";"), QStringLiteral("；")).join(";") << "\n";
    }

    // exit if we shouldn't display data
    if (!m_displayData)
        return;

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
    return m_tableBox->geometry();
}

void RecordedTable::setGeometry(const QRect &rect)
{
    m_tableBox->setGeometry(rect);
}

QWidget *RecordedTable::widget() const
{
    return m_tableBox;
}

void RecordedTable::updateInfoLabel()
{
    m_infoLabel->setVisible(false);
    if (m_displayData && m_saveData)
        return;
    m_infoLabel->setVisible(true);

    if (!m_displayData && !m_saveData) {
        m_infoLabel->setText("⚠ Not saving table data or updating displayed data!");
        return;
    }

    if (m_displayData && !m_saveData) {
        m_infoLabel->setText("⚠ Not saving displayed table data!");
        return;
    }

    if (!m_displayData && m_saveData) {
        m_infoLabel->setText("Saving data, but not updating displayed content.");
        return;
    }
}
