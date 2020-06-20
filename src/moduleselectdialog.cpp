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

#include "moduleselectdialog.h"
#include "ui_moduleselectdialog.h"

#include <QStandardItemModel>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QtMath>
#include <QDebug>

#include "globalconfig.h"
#include "moduleapi.h"


class HtmlDelegate : public QStyledItemDelegate
{
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex& index) const override
    {
        auto options = option;
        initStyleOption(&options, index);

        QSize iconSize = options.icon.actualSize(options.rect.size());
        QTextDocument doc;
        doc.setHtml(options.text);
        doc.setTextWidth(options.rect.width() - iconSize.width());

        auto height = qRound(doc.size().height());
        if (height < iconSize.height())
            height = iconSize.height();

        // set a fixed height for now
        height = 80;
        return QSize(qCeil(doc.size().width()), height);
    }
};

void HtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto options = option;
    initStyleOption(&options, index);

    painter->save();

    QSize iconSize = options.icon.actualSize(options.rect.size());
    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width() - iconSize.width());

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    // shift text right to make icon visible
    painter->translate(options.rect.left() + iconSize.width(), options.rect.top());
    QRect clip(0, 0, options.rect.width() + iconSize.width(), options.rect.height());

    painter->setClipRect(clip);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = clip;
    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

ModuleSelectDialog::ModuleSelectDialog(QList<QSharedPointer<ModuleInfo> > infos, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ModuleSelectDialog)
{
    ui->setupUi(this);
    this->setWindowModality(Qt::ApplicationModal);
    this->setWindowTitle(QStringLiteral("Select a module"));

    m_model = new QStandardItemModel(this);
    ui->listView->setModel(m_model);
    ui->listView->setIconSize(QSize(48, 48));

    auto htmlDelegate = new HtmlDelegate;
    ui->listView->setItemDelegate(htmlDelegate);

    m_selectedEntryId.clear();
    setModuleInfo(infos);
}

ModuleSelectDialog::~ModuleSelectDialog()
{
    delete ui;
}

void ModuleSelectDialog::setModuleInfo(QList<QSharedPointer<ModuleInfo>> infos)
{
    Syntalos::GlobalConfig gconf;
    const auto showDevModules = gconf.showDevelModules();

    struct {
        bool operator()(const QSharedPointer<ModuleInfo> &mi1, const QSharedPointer<ModuleInfo> &mi2) const
        {
            return mi1->id() < mi2->id();
        }
    } moduleInfoLess;

    m_model->clear();

    QList<QSharedPointer<ModuleInfo>> sortedInfos = infos;
    std::sort(sortedInfos.begin(), sortedInfos.end(), moduleInfoLess);

    for (auto &info : sortedInfos) {
        // hide developer modules, unless the user explicitly chose to show them
        if (info->devel() && !showDevModules)
            continue;

        auto item = new QStandardItem(QIcon(info->pixmap()),
                                      QStringLiteral("<b>%1</b><br/><span>%2</span>").arg(info->name()).arg(info->description()));
        item->setData(info->id());
        m_model->appendRow(item);

        if (info->singleton() && info->count() > 0)
            item->setEnabled(false);
    }
}

QString ModuleSelectDialog::selectedEntryId() const
{
    return m_selectedEntryId;
}

void ModuleSelectDialog::on_listView_activated(const QModelIndex &index)
{
    m_selectedEntryId = m_model->itemFromIndex(index)->data().toString();
}

void ModuleSelectDialog::on_listView_clicked(const QModelIndex &index)
{
    m_selectedEntryId = m_model->itemFromIndex(index)->data().toString();
}
