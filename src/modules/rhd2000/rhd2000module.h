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

#ifndef RHD2000MODULE_H
#define RHD2000MODULE_H

#include <QObject>
#include <chrono>
#include "abstractmodule.h"

class IntanUI;
class TracePlotProxy;

class Rhd2000Module : public AbstractModule
{
    Q_OBJECT
public:
    explicit Rhd2000Module(QObject *parent = nullptr);
    ~Rhd2000Module() override;

    QString id() const override;
    QString description() const override;
    QString license() const override;
    QPixmap pixmap() const override;
    bool singleton() const override;

    bool initialize(ModuleManager *manager) override;

    bool prepare(const QString& storageRootDir, const TestSubject& testSubject, HRTimer *timer) override;

    void start() override;

    bool runCycle() override;

    void stop() override;

    void finalize() override;

    QList<QAction *> actions() override;

    QByteArray serializeSettings(const QString &confBaseDir) override;
    bool loadSettings(const QString &confBaseDir, const QByteArray& data) override;

    void setPlotProxy(TracePlotProxy *proxy);

    void setStatusMessage(const QString& message);

private:
    IntanUI *m_intanUi;
    QList<QAction *> m_actions;
    QAction *m_runAction;

    void noRecordRunActionTriggered();
};

#endif // RHD2000MODULE_H
