/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef ENGINE_H
#define ENGINE_H

#include <memory>
#include <QObject>
#include <QSharedDataPointer>

#include "moduleapi.h"

class ModuleManager;

class Engine : public QObject
{
    Q_OBJECT
public:
    explicit Engine(ModuleManager *modManager, QObject *parent = nullptr);
    ~Engine();

    ModuleManager *modManager() const;

    QString exportBaseDir() const;
    void setExportBaseDir(const QString &dataDir);
    bool exportDirIsTempDir() const;
    bool exportDirIsValid() const;

    TestSubject testSubject() const;
    void setTestSubject(const TestSubject &ts);

    QString experimentId() const;
    void setExperimentId(const QString &id);

    QString exportDir() const;
    bool isRunning() const;
    bool hasFailed() const;

    bool run();
    void stop();

signals:
    void statusMessage(const QString &message);
    void runFailed(AbstractModule *mod, const QString &message);

public slots:

private:
    bool makeDirectory(const QString &dir);
    void refreshExportDirPath();
    void emitStatusMessage(const QString &message);
    QList<AbstractModule*> createModuleExecOrderList();

    class EData;
    Q_DISABLE_COPY(Engine)
    QSharedDataPointer<EData> d;
};

#endif // ENGINE_H
