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

#pragma once

#include <QObject>
#include <QEventLoop>

#include "moduleapi.h"

/**
 * @brief Base for out-of-process module implementations
 */
class OOPModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit OOPModule(QObject *parent = nullptr);
    ~OOPModule();

    ModuleFeatures features() const override;

    bool prepare(const QString &storageRootDir, const TestSubject &testSubject) override;

    bool oopPrepare(QEventLoop *loop);
    void oopStart(QEventLoop *);
    void oopRunEvent(QEventLoop *loop);
    void oopFinalize(QEventLoop *loop);

signals:
    void processStdoutReceived(const QString &text);

protected:
    void loadPythonScript(const QString &script, const QString &env = QString());
    void loadPythonFile(const QString &fname, const QString &env = QString());

    QString workerBinary() const;
    void setWorkerBinary(const QString &binPath);
    void setWorkerBinaryPyWorker();

    bool captureStdout() const;
    void setCaptureStdout(bool capture);

private:
    class Private;
    Q_DISABLE_COPY(OOPModule)
    QScopedPointer<Private> d;
};
