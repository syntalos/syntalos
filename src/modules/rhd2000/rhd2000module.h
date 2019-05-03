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
    ~Rhd2000Module();

    QString id() const;
    QString displayName() const;
    QString description() const;
    QPixmap pixmap() const;
    bool singleton() const;

    bool initialize(ModuleManager *manager);

    bool prepare(const QString& storageRootDir, const QString& subjectId);

    bool runCycle();

    void stop();

    void finalize();

    void showDisplayUi();
    void hideDisplayUi();

    void showSettingsUi();
    void hideSettingsUi();

    QList<QAction *> actions();

    QByteArray serializeSettings();
    bool loadSettings(const QByteArray& data);

    void setPlotProxy(TracePlotProxy *proxy);

    void setStatusMessage(const QString& message);

private:
    IntanUI *m_intanUi;
    QList<QAction *> m_actions;
};

#endif // RHD2000MODULE_H
