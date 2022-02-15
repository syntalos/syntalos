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

#include "modulelibrary.h"

class QStandardItemModel;

namespace Ui {
class ModuleSelectDialog;
}

class ModuleSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModuleSelectDialog(QList<QSharedPointer<ModuleInfo>> infos, QWidget *parent = nullptr);
    ~ModuleSelectDialog();

    void setModuleInfo(QList<QSharedPointer<ModuleInfo>> infos);
    QString selectedEntryId() const;

private slots:
    void on_listView_doubleClicked(const QModelIndex &index);

private:
    Ui::ModuleSelectDialog *ui;

    void setEntryIdFromIndex(const QModelIndex &index);

    QStandardItemModel *m_model;
    QString m_selectedEntryId;
};

#endif // MODULESELECTDIALOG_H
