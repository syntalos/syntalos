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

#include "tablemodule.h"

#include <QTimer>
#include <QTime>
#include <QDir>
#include <QFile>
#include <QSvgRenderer>
#include <QPainter>

#include "utils/style.h"
#include "recordedtable.h"

SYNTALOS_MODULE(TableModule)

static QIcon getTableModuleIcon();

class TableModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<TableRow>> m_rowsIn;

    std::shared_ptr<StreamSubscription<TableRow>> m_rowSub;

    RecordedTable *m_recTable;
    QString m_imgWinTitle;
    QTimer *m_evTimer;

public:
    explicit TableModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_rowsIn = registerInputPort<TableRow>(QStringLiteral("rows"), QStringLiteral("Rows"));

        m_recTable = new RecordedTable(nullptr, getTableModuleIcon());
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

    bool prepare(const TestSubject &) override
    {
        m_rowSub.reset();
        if (m_rowsIn->hasSubscription())
            m_rowSub = m_rowsIn->subscription();

        return true;
    }

    void start() override
    {
        if (m_rowSub.get() != nullptr) {
            const auto mdata = m_rowSub->metadata();

            auto dstore = getOrCreateDefaultDataset(name(), mdata);

            // get our file basename
            auto fname = dataBasenameFromSubMetadata(mdata, QStringLiteral("table"));
            fname = QStringLiteral("%1.csv").arg(fname);

            // this turns it into an absolute path we can open for data storage
            fname = dstore->setDataFile(fname);
            if (!m_recTable->open(fname)) {
                raiseError(QStringLiteral("Unable to open file %1").arg(fname));
                return;
            }

            // remove any old data from the table display
            m_recTable->reset();

            const auto header = mdata.value("table_header").toStringList();
            m_recTable->setHeader(header);

            auto imgWinTitle = m_rowSub->metadataValue(CommonMetadataKey::SrcModName).toString();
            if (imgWinTitle.isEmpty())
                imgWinTitle = "Canvas";
            const auto portTitle = m_rowSub->metadataValue(CommonMetadataKey::SrcModPortTitle).toString();
            if (!portTitle.isEmpty())
                imgWinTitle = QStringLiteral("%1 - %2").arg(imgWinTitle).arg(portTitle);
            m_recTable->widget()->setWindowTitle(imgWinTitle);

            m_evTimer->start();
        }
    }

    void stop() override
    {
        m_evTimer->stop();
        m_recTable->close();
    }

    void addRow()
    {
        auto maybeRow = m_rowSub->peekNext();
        if (!maybeRow.has_value())
            return;

        const auto row = maybeRow.value();
        m_recTable->addRows(row);
    }
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

QIcon TableModuleInfo::icon() const
{
    return getTableModuleIcon();
}

AbstractModule *TableModuleInfo::createModule(QObject *parent)
{
    return new TableModule(parent);
}

static QIcon getTableModuleIcon()
{
    const auto tableIconResStr = QStringLiteral(":/module/table");
    bool isDark = currentThemeIsDark();
    if (!isDark)
        return QIcon(tableIconResStr);

    // convert our bright-mode icon into something that's visible easier
    // on a dark background
    QFile f(QStringLiteral(":/module/table"));
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        qWarning().noquote() << "Failed to table module icon: " << f.errorString();
        return QIcon(tableIconResStr);
    }

    QTextStream in(&f);
    auto data = in.readAll();
    QSvgRenderer renderer(data.replace(QStringLiteral("#232629"), QStringLiteral("#eff0f1")).toLocal8Bit());
    QPixmap pix(96, 96);
    pix.fill(QColor(0, 0, 0, 0));
    QPainter painter(&pix);
    renderer.render(&painter, pix.rect());

    return QIcon(pix);
}

#include "tablemodule.moc"
