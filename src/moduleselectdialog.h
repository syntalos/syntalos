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

#ifndef MODULESELECTDIALOG_H
#define MODULESELECTDIALOG_H

#include <QDialog>

#include "modulemanager.h"

class QStandardItemModel;

namespace Ui {
class ModuleSelectDialog;
}

class ModuleSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModuleSelectDialog(QList<QSharedPointer<ModuleInfo> > infos, QWidget *parent = nullptr);
    ~ModuleSelectDialog();

    void setModuleInfo(QList<QSharedPointer<ModuleInfo>> infos);
    QString selectedEntryId() const;

private slots:
    void on_listView_activated(const QModelIndex &index);
    void on_listView_clicked(const QModelIndex &index);

private:
    Ui::ModuleSelectDialog *ui;

    QStandardItemModel *m_model;
    QString m_selectedEntryId;
};

#endif // MODULESELECTDIALOG_H
