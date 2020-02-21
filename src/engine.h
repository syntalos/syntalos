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

#include "moduleapi.h"
#include "modulelibrary.h"

class Engine : public QObject
{
    Q_OBJECT
    friend class ModuleManager;
public:
    explicit Engine(QWidget *parentWidget = nullptr);
    ~Engine();

    ModuleLibrary *library() const;

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

    AbstractModule *createModule(const QString &id, const QString &name = QString());
    bool removeModule(AbstractModule *mod);
    void removeAllModules();

    QList<AbstractModule*> activeModules() const;
    AbstractModule *moduleByName(const QString &name) const;

public slots:
    bool run();
    void stop();

signals:
    void moduleCreated(ModuleInfo *info, AbstractModule *mod);
    void modulePreRemove(AbstractModule *mod);

    void statusMessage(const QString &message);
    void runFailed(AbstractModule *mod, const QString &message);
    void moduleError(AbstractModule *mod, const QString& message);

private slots:
    void receiveModuleError(const QString& message);

private:
    class Private;
    Q_DISABLE_COPY(Engine)
    QScopedPointer<Private> d;

    bool makeDirectory(const QString &dir);
    void refreshExportDirPath();
    void emitStatusMessage(const QString &message);
    QList<AbstractModule*> createModuleExecOrderList();    
};

#endif // ENGINE_H
