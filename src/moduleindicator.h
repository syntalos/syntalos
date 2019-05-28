/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef MODULEINDICATORWIDGET_H
#define MODULEINDICATORWIDGET_H

#include <QFrame>
#include <QSharedDataPointer>
#include "abstractmodule.h"

class ModuleManager;

namespace Ui {
class ModuleIndicator;
}

class ModuleIndicator : public QFrame
{
    Q_OBJECT

public:
    explicit ModuleIndicator(AbstractModule *module, ModuleManager *manager = nullptr, QWidget *parent = nullptr);
    ~ModuleIndicator();

    AbstractModule *module() const;

private slots:
    void on_removeButton_clicked();
    void on_showButton_clicked();
    void on_configButton_clicked();
    void on_modulePreRemove(AbstractModule *mod);
    void on_editNameActionTriggered();

private:
    class MIData;
    Ui::ModuleIndicator *ui;
    QSharedDataPointer<MIData> d;

    void receiveActionsUpdated();
    void receiveStateChange(ModuleState state);
    void receiveErrorMessage(const QString& message);
    void receiveMessage(const QString& message);
};

#endif // MODULEINDICATORWIDGET_H
