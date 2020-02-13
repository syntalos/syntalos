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

#include "tablemodule.h"

#include <QTimer>
#include <QTime>
#include <QDir>

#include "recordedtable.h"

class TableModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort<TableRow>> m_rowsIn;

    std::shared_ptr<StreamSubscription<TableRow>> m_rowSub;

    RecordedTable *m_recTable;
    QString m_imgWinTitle;
    QTimer *m_evTimer;

    QString m_storageRootDir;

public:
    explicit TableModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_rowsIn = registerInputPort<TableRow>(QStringLiteral("rows"), QStringLiteral("Rows"));

        m_recTable = new RecordedTable;
        addDisplayWindow(m_recTable->widget(), false); // we register this to get automatic layout save/restore
        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &TableModule::addRow);
    }

    ~TableModule() override
    {
        delete m_recTable;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    void showDisplayUi() override
    {
        m_recTable->show();
    }

    void hideDisplayUi() override
    {
        m_recTable->hide();
    }

    bool prepare(const QString &storageRootDir, const TestSubject &) override
    {
        m_rowSub.reset();
        if (m_rowsIn->hasSubscription())
            m_rowSub = m_rowsIn->subscription();
        m_storageRootDir = storageRootDir;

        return true;
    }

    void start() override
    {
        if (m_rowSub.get() != nullptr) {
            auto dataName = m_rowSub->metadata().value("dataName").toString();
            if (!dataName.contains('/')) {
                const auto srcModName = m_rowSub->metadata().value("srcModName").toString();
                if (!srcModName.isEmpty())
                    dataName = QStringLiteral("tables/%1.csv").arg(QString(srcModName).replace(' ', '_'));
                else
                    dataName = QStringLiteral("tables/unknown.csv");
            }
            auto const fname = QStringLiteral("%1/%2").arg(m_storageRootDir).arg(dataName);
            QFileInfo fi(fname);
            if (!makeDirectory(fi.absolutePath()))
                return;
            if (!m_recTable->open(fname)) {
                raiseError(QStringLiteral("Unable to open file %1").arg(fname));
                return;
            }

            const auto header = m_rowSub->metadata().value("tableHeader").toStringList();
            m_recTable->setHeader(header);

            m_evTimer->start();
        }
    }

    void stop() override
    {
        m_evTimer->stop();
    }

    void addRow()
    {
        auto maybeRow = m_rowSub->peekNext();
        if (!maybeRow.has_value())
            return;

        const auto row = maybeRow.value();
        m_recTable->addRows(row);
    }

private:

};

QString TableModuleInfo::id() const
{
    return QStringLiteral("table");
}

QString TableModuleInfo::name() const
{
    return QStringLiteral("Table");
}

QString TableModuleInfo::description() const
{
    return QStringLiteral("Display & record data that can be represented as a table.");
}

QPixmap TableModuleInfo::pixmap() const
{
    return QPixmap(":/module/table");
}

AbstractModule *TableModuleInfo::createModule(QObject *parent)
{
    return new TableModule(parent);
}
