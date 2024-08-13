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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "moduleselectdialog.h"
#include "ui_moduleselectdialog.h"

#include <QAbstractTextDocumentLayout>
#include <QDebug>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QtMath>
#include <QTimer>

#include "globalconfig.h"
#include "moduleapi.h"

class HtmlDelegate : public QStyledItemDelegate
{
protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto options = option;
        initStyleOption(&options, index);

        QSize iconSize = options.icon.actualSize(options.rect.size());
        QTextDocument doc;
        doc.setHtml(options.text);
        doc.setTextWidth(options.rect.width() - iconSize.width());

        doc.adjustSize();
        auto height = qRound(doc.size().height());
        if (height > 76)
            height = 76;

        return {qCeil(doc.size().width()), height};
    }
};

void HtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto options = option;
    initStyleOption(&options, index);

    painter->save();

    const auto iconWidthSpace = option.decorationSize.width() + 16;
    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width() - iconWidthSpace);

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    // shift text right to make icon visible
    painter->translate(options.rect.left() + iconWidthSpace, options.rect.top());
    QRect clip(0, 0, options.rect.width() + iconWidthSpace, options.rect.height());

    painter->setClipRect(clip);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.clip = clip;
    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

ModuleSelectDialog::ModuleSelectDialog(const QList<QSharedPointer<ModuleInfo>> &infos, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ModuleSelectDialog)
{
    ui->setupUi(this);
    this->setWindowModality(Qt::ApplicationModal);
    this->setWindowTitle(QStringLiteral("Select a module"));

    // load settings
    Syntalos::GlobalConfig gconf;
    m_showDevModules = gconf.showDevelModules();

    // categories
    m_catModel = new QStandardItemModel(this);
    ui->categoryListView->setModel(m_catModel);
    ui->categoryListView->setIconSize(QSize(32, 32));

    m_catModel->appendRow(newCatModelItem(-1, "All", QIcon(":/categories/all")));
    m_catModel->appendRow(newCatModelItem(ModuleCategory::DEVICES, "Device Support", QIcon(":/categories/devices")));
    m_catModel->appendRow(
        newCatModelItem(ModuleCategory::GENERATORS, "Data Generators", QIcon(":/categories/generators")));
    m_catModel->appendRow(
        newCatModelItem(ModuleCategory::SCRIPTING, "Custom Scripting", QIcon(":/categories/scripting")));
    m_catModel->appendRow(newCatModelItem(ModuleCategory::DISPLAY, "Display", QIcon(":/categories/display")));
    m_catModel->appendRow(newCatModelItem(ModuleCategory::WRITERS, "Data Writers", QIcon(":/categories/writers")));
    m_catModel->appendRow(
        newCatModelItem(ModuleCategory::PROCESSING, "Live Data Processing", QIcon(":/categories/processing")));
    m_catModel->appendRow(
        newCatModelItem(ModuleCategory::EXAMPLES, "Module Templates", QIcon(":/categories/examples")));
    if (m_showDevModules)
        m_catModel->appendRow(
            newCatModelItem(ModuleCategory::SYNTALOS_DEV, "Development & Tests", QIcon(":/categories/development")));
    ui->categoryListView->setCurrentIndex(m_catModel->index(0, 0));

    // modules
    m_modModel = new QStandardItemModel(this);
    m_filterModel = new QStandardItemModel(this);
    setModuleViewModel(m_modModel);
    ui->modListView->setIconSize(QSize(48, 48));

    auto htmlDelegate = new HtmlDelegate;
    ui->modListView->setItemDelegate(htmlDelegate);

    m_selectedEntryId.clear();
    setModuleInfo(infos);

    // setup right sidebar
    ui->modIconLabel->setPixmap(QIcon::fromTheme("question").pixmap(96));
    ui->modNameLabel->setText("");
    ui->modDescLabel->setText("");
    ui->detailsWidget->setVisible(false);

    // setup filter
    ui->filterEdit->setClearButtonEnabled(true);
    ui->filterEdit->addAction(QIcon::fromTheme("search"), QLineEdit::LeadingPosition);
    ui->filterEdit->setPlaceholderText("Filter...");

    // connections
    connect(
        ui->categoryListView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex &index, const QModelIndex &) {
            setCategoryFromIndex(index);
        });

    // focus
    ui->filterEdit->setFocus();
}

ModuleSelectDialog::~ModuleSelectDialog()
{
    delete ui;
}

void ModuleSelectDialog::setModuleViewModel(QStandardItemModel *model)
{
    if (ui->modListView->model() == model)
        return;

    ui->modListView->setModel(model);
    connect(
        ui->modListView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [this](const QModelIndex &index, const QModelIndex &) {
            setModuleIdFromIndex(index);
        });
}

QStandardItem *ModuleSelectDialog::newCatModelItem(int catId, const QString &name, const QIcon &icon)
{
    auto item = new QStandardItem(icon, name);
    auto font = item->font();
    font.setBold(true);
    font.setPointSize(11);
    item->setFont(font);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    item->setData((uint32_t)catId);
    return item;
}

QStandardItem *ModuleSelectDialog::newCatModelItem(ModuleCategory cat, const QString &name, const QIcon &icon)
{
    return newCatModelItem(static_cast<int>(cat), name, icon);
}

void ModuleSelectDialog::setModuleInfo(const QList<QSharedPointer<ModuleInfo>> &infos)
{
    struct {
        bool operator()(const QSharedPointer<ModuleInfo> &mi1, const QSharedPointer<ModuleInfo> &mi2) const
        {
            return mi1->id() < mi2->id();
        }
    } moduleInfoLess;

    m_modModel->clear();

    QList<QSharedPointer<ModuleInfo>> sortedInfos = infos;
    std::sort(sortedInfos.begin(), sortedInfos.end(), moduleInfoLess);

    for (auto &info : sortedInfos) {
        // hide developer modules, unless the user explicitly chose to show them
        if (info->categories().testFlag(ModuleCategory::SYNTALOS_DEV) && !m_showDevModules)
            continue;

        m_modInfoLib[info->id()] = info;

        auto item = new QStandardItem(
            info->icon(), QStringLiteral("<b>%1</b><br/><span>%2</span>").arg(info->name(), info->summary()));
        item->setTextAlignment(Qt::AlignLeft);
        item->setData(info->id());
        m_modModel->appendRow(item);

        if (info->singleton() && info->count() > 0)
            item->setEnabled(false);
    }
}

QString ModuleSelectDialog::selectedEntryId() const
{
    return m_selectedEntryId;
}

void ModuleSelectDialog::on_modListView_doubleClicked(const QModelIndex &index)
{
    setModuleIdFromIndex(index);
    this->done(QDialog::Accepted);
}

void ModuleSelectDialog::setCategoryFromIndex(const QModelIndex &index)
{
    auto catId = m_catModel->itemFromIndex(index)->data().toInt();
    m_filterModel->clear();

    if (catId < 0) {
        // show all entries
        setModuleViewModel(m_modModel);
        return;
    }

    // show filtered list
    for (int i = 0; i < m_modModel->rowCount(); ++i) {
        auto item = m_modModel->item(i);
        auto info = m_modInfoLib.value(item->data().toString());
        if (info->categories().testFlag(static_cast<ModuleCategory>(catId))) {
            m_filterModel->appendRow(item->clone());
        }
    }

    setModuleViewModel(m_filterModel);
}

void ModuleSelectDialog::setModuleIdFromIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    auto model = qobject_cast<QStandardItemModel *>(ui->modListView->model());
    m_selectedEntryId = model->itemFromIndex(index)->data().toString();
    auto info = m_modInfoLib.value(m_selectedEntryId);
    if (!info)
        return;
    ui->modIconLabel->setPixmap(info->icon().pixmap(96));
    ui->modNameLabel->setText(info->name());
    ui->modDescLabel->setText(info->description());

    ui->detailsWidget->setVisible(true);
    ui->licenseInfoLabel->setVisible(false);
    ui->licenseLabel->setVisible(false);
    ui->authorsLabel->setVisible(false);
    ui->authorsInfoLabel->setVisible(false);
    if (!info->authors().isEmpty()) {
        ui->authorsLabel->setVisible(true);
        ui->authorsInfoLabel->setVisible(true);
        ui->authorsLabel->setText(info->authors());
    }
    if (!info->license().isEmpty()) {
        ui->licenseLabel->setVisible(true);
        ui->licenseInfoLabel->setVisible(true);
        ui->licenseLabel->setText(info->license().replace("\n", "<br/>") + QStringLiteral("<br/>"));
    }
}

void ModuleSelectDialog::filterByTerm(const QString &filterTerm)
{
    m_filterModel->clear();

    if (filterTerm.isEmpty()) {
        // show all modules
        setModuleViewModel(m_modModel);
        ui->categoryListView->setEnabled(true);
        m_termFilterPending = false;
        return;
    }

    // disable category filter
    ui->categoryListView->setEnabled(false);
    ui->categoryListView->setCurrentIndex(m_catModel->index(0, 0));

    // show filtered list
    for (int i = 0; i < m_modModel->rowCount(); ++i) {
        auto item = m_modModel->item(i);
        auto info = m_modInfoLib.value(item->data().toString());
        if (info->name().contains(filterTerm, Qt::CaseInsensitive)
            || info->description().contains(filterTerm, Qt::CaseInsensitive)) {
            m_filterModel->appendRow(item->clone());
        }
    }

    setModuleViewModel(m_filterModel);
    m_termFilterPending = false;
}

void ModuleSelectDialog::on_filterEdit_editingFinished()
{
    if (!m_termFilterPending) {
        m_termFilterPending = true;
        QTimer::singleShot(200, this, [&]() {
            filterByTerm(ui->filterEdit->text());
        });
    }
}

void ModuleSelectDialog::on_filterEdit_textChanged(const QString &)
{
    if (!m_termFilterPending) {
        m_termFilterPending = true;
        QTimer::singleShot(200, this, [&]() {
            filterByTerm(ui->filterEdit->text());
        });
    }
}

void ModuleSelectDialog::on_filterEdit_textEdited(const QString &arg1)
{
    on_filterEdit_textChanged(arg1);
}

void ModuleSelectDialog::on_filterEdit_returnPressed()
{
    on_filterEdit_editingFinished();
}
