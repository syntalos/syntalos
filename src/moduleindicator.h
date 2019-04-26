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

namespace Ui {
class ModuleIndicator;
}

class ModuleIndicator : public QFrame
{
    Q_OBJECT

public:
    explicit ModuleIndicator(AbstractModule *module, QWidget *parent = nullptr);
    ~ModuleIndicator();

private slots:
    void on_showButton_clicked();
    void on_configButton_clicked();

private:
    class MIData;
    Ui::ModuleIndicator *ui;
    QSharedDataPointer<MIData> d;

    void receiveStateChange(ModuleState state);
    void receiveErrorMessage(const QString& message);
};

#endif // MODULEINDICATORWIDGET_H
