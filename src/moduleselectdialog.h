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

#ifndef MODULESELECTDIALOG_H
#define MODULESELECTDIALOG_H

#include <QDialog>

#include "moduleapi.h"

class QStandardItemModel;
class QStandardItem;

namespace Ui
{
class ModuleSelectDialog;
}

class ModuleSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModuleSelectDialog(const QList<QSharedPointer<ModuleInfo>> &infos, QWidget *parent = nullptr);
    ~ModuleSelectDialog();

    void setModuleInfo(const QList<QSharedPointer<ModuleInfo>> &infos);
    QString selectedEntryId() const;

private slots:
    void on_modListView_doubleClicked(const QModelIndex &index);
    void on_filterEdit_editingFinished();
    void on_filterEdit_textChanged(const QString &arg1);

private:
    Ui::ModuleSelectDialog *ui;

    void setModuleViewModel(QStandardItemModel *model);
    QStandardItem *newCatModelItem(int catId, const QString &name, const QIcon &icon);
    QStandardItem *newCatModelItem(Syntalos::ModuleCategory cat, const QString &name, const QIcon &icon);
    void setCategoryFromIndex(const QModelIndex &index);
    void setModuleIdFromIndex(const QModelIndex &index);
    void filterByTerm(const QString &filterTerm);

    bool m_showDevModules;
    bool m_termFilterPending;
    QStandardItemModel *m_catModel;
    QStandardItemModel *m_modModel;
    QStandardItemModel *m_filterModel;
    QHash<QString, QSharedPointer<ModuleInfo>> m_modInfoLib;
    QHash<Syntalos::ModuleCategory, QList<QString>> m_modCats;
    QString m_selectedEntryId;
};

#endif // MODULESELECTDIALOG_H
