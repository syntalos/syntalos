/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QObject>
#include "optionalwaitcondition.h"
#include "moduleapi.h"

namespace Syntalos {

/**
 * @brief Manages a thread which is running evented modules
 *
 * This class can be created in one thread and opaquely manages a second
 * thread which is running a dedicated event loop to execute the tasks
 * by the registered modules at their desired interfvals.
 *
 * Running module tasks at particular intervals or have them possibly wait
 * on other events it is tricky using the standard QEventLoop, since
 * Syntalos does not use QThread in many occasions and tries to never move
 * objects between threads explicitly, ruling out the use of the Qt event system.
 *
 * Therefore, the thread managed by this class runs a GMainLoop-based event
 * loop to have much tighter control on what is executed when and why, and
 * to take care of Syntalos-specific quirks.
 */
class ModuleEventThread : public QObject
{
    Q_OBJECT
public:
    explicit ModuleEventThread(const QString &threadName = QString(), QObject *parent = nullptr);
    ~ModuleEventThread();

    bool isRunning() const;
    bool isFailed() const;
    QString threadName() const;

    void setFailed(bool failed);

    void run(QList<AbstractModule *> mods, OptionalWaitCondition *waitCondition);
    void stop();

signals:
    void failed();

private:
    class Private;
    Q_DISABLE_COPY(ModuleEventThread)
    QScopedPointer<Private> d;

    void shutdownThread();
    void moduleEventThreadFunc(QList<AbstractModule *> mods,
                               OptionalWaitCondition *waitCondition);
};

} // end of namespace
