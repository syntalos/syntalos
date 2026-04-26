/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QEventLoop>
#include <QObject>
#include <QProcessEnvironment>

#include "moduleapi.h"
#include "streamexporter.h"

namespace Syntalos
{

struct ErrorEvent;
struct StateChangeEvent;

/**
 * @brief Worker mode for out-of-process modules
 */
enum class ModuleWorkerMode {
    PERSISTENT, /// Worker is started once, and runs while the module is present on the board
    TRANSIENT   /// Worker run only for the duration of the experiment, and terminated afterwards
};

/**
 * @brief Master link for out-of-process modules
 */
class MLinkModule : public AbstractModule
{
    Q_OBJECT
public:
    enum OutChannelType {
        ChannelAll,
        ChannelStdout,
        ChannelStderr
    };
    Q_ENUM(OutChannelType)

    explicit MLinkModule(QObject *parent = nullptr);
    ~MLinkModule() override;
    bool initialize() override;

    ModuleDriverKind driver() const override;
    bool usesDirectModuleLink() const override;
    virtual ModuleFeatures features() const override;

    QString moduleBinary() const;
    void setModuleBinary(const QString &binaryPath);
    void setModuleBinaryArgs(const QStringList &args);
    void setModuleBinaryWorkDir(const QString &wdir);

    QProcessEnvironment moduleBinaryEnv() const;
    void setModuleBinaryEnv(const QProcessEnvironment &env);

    ModuleWorkerMode workerMode() const;
    void setWorkerMode(ModuleWorkerMode mode);

    bool outputCaptured() const;
    void setOutputCaptured(bool capture);

    void setPythonVirtualEnv(const QString &venvDir);
    void setScript(const QString &script, const QString &wdir = QString());
    bool setScriptFromFile(const QString &fname, const QString &wdir = QString());

    void serializeSettings(const QString &confBaseDir, QVariantHash &settings, QByteArray &extraData) override;
    bool loadSettings(const QString &confBaseDir, const QVariantHash &settings, const QByteArray &extraData) override;

    virtual void showDisplayUi() override;
    virtual void showSettingsUi() override;

    void terminateProcess();
    bool runProcess();
    bool isProcessRunning() const;

    bool loadCurrentScript(bool resetPorts = false);
    bool sendPortInformation();

    QString readProcessOutput(OutChannelType channel = ChannelAll);

    void markIncomingForExport(StreamExporter *exporter);
    bool prepare(const TestSubject &subject) override;
    void start() override;
    void stop() override;
    void runThread(OptionalWaitCondition *startWaitCondition) override;

signals:
    void processOutputReceived(OutChannelType channel, const QString &text);

protected:
    bool testIpcApiVersion(bool emitErrors = true);

    /**
     * Process all pending IPC control messages from the worker.
     *
     * Drains error, state-change, port-change and settings-change channels.
     * Subclasses may call this after operations that are expected to trigger
     * port registrations (e.g. after loading a script in Python modules).
     */
    void handleIncomingControl();

private:
    void resetConnection();
    bool sendSettings();

    bool registerOutPortForwarders();
    void disconnectOutPortForwarders();

private:
    class Private;
    Q_DISABLE_COPY(MLinkModule)
    std::unique_ptr<Private> d;
    friend class Private;
};

} // namespace Syntalos
