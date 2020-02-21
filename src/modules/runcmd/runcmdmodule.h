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

#ifndef RUNCMDMODULE_H
#define RUNCMDMODULE_H

#include <QObject>
#include <chrono>
#include "moduleapi.h"

class RunCmdModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QPixmap pixmap() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class RunCmdModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit RunCmdModule(QObject *parent = nullptr);
    ~RunCmdModule() override;

    bool prepare(const TestSubject& testSubject) override;
    void stop() override;

private:

};

#endif // RUNCMDMODULE_H
