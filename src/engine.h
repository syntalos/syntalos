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

#include <QLoggingCategory>
#include <QObject>
#include <memory>

#include "moduleapi.h"
#include "modulelibrary.h"
#include "sysinfo.h"

class MLinkModule;

namespace Syntalos
{

Q_DECLARE_LOGGING_CATEGORY(logEngine)

class Engine : public QObject
{
    Q_OBJECT
    friend class ModuleManager;

public:
    /**
     * Order in which directories are placed in the export location
     */
    enum class ExportDirOrder {
        SubjectFirst = 0,
        DateFirst = 1,
    };

    explicit Engine(QWidget *parentWidget = nullptr);
    ~Engine();

    enum SystemResource {
        StorageSpace,
        Memory,
        CpuCores,
        StreamBuffers
    };
    Q_ENUM(SystemResource)

    bool initialize();

    ModuleLibrary *library() const;
    SysInfo *sysInfo() const;

    QString exportBaseDir() const;
    void setExportBaseDir(const QString &dataDir);
    bool exportDirIsTempDir() const;
    bool exportDirIsValid() const;
    void setAlwaysOverrideExportDir(bool alwaysOverride);
    bool alwaysOverrideExportDir() const;

    TestSubject testSubject() const;
    void setTestSubject(const TestSubject &ts);

    QString experimentId() const;
    void setExperimentId(const QString &id);
    bool hasExperimentIdReplaceables() const;

    EDLAuthor experimenter() const;
    void setExperimenter(const EDLAuthor &person);

    bool simpleStorageNames() const;
    void setSimpleStorageNames(bool enabled);

    bool clockTimeInExportDir() const;
    void setClockTimeInExportDir(bool enabled);

    ExportDirOrder exportDirOrder() const;
    void setExportDirOrder(ExportDirOrder order);

    QString exportDir() const;
    bool isRunning() const;
    bool isActive() const;
    bool hasFailed() const;
    milliseconds_t currentRunElapsedTime() const;

    void resetSuccessRunsCounter();
    int successRunsCount();
    void setRunCountExpectedMax(int maxValue);

    AbstractModule *createModule(const QString &id, const QString &name = QString());
    bool removeModule(AbstractModule *mod);
    void removeAllModules();

    QList<AbstractModule *> presentModules() const;
    AbstractModule *moduleByName(const QString &name) const;

    QString lastRunExportDir() const;
    QString readRunComment(const QString &runExportDir = nullptr) const;
    /**
     * @brief Set comment for the next or a last experiment run
     * @param comment The comment to set
     * @param runExportDir The export directory, or empty for next run.
     */
    void setRunComment(const QString &comment, const QString &runExportDir = nullptr);

    bool saveInternalDiagnostics() const;
    void setSaveInternalDiagnostics(bool save);

    void notifyUsbHotplugEvent(UsbHotplugEventKind kind) const;

public slots:
    /**
     * @brief Run the current board, save all data
     */
    bool run();

    /**
     * @brief Run the current board, do not keep any experiment data
     */
    bool runEphemeral();

    /**
     * @brief Stop a running experiment
     */
    void stop();

signals:
    void moduleCreated(ModuleInfo *info, AbstractModule *mod);
    void modulePreRemove(AbstractModule *mod);

    void moduleInitStarted();
    void moduleInitDone();

    void statusMessage(const QString &message);
    void preRunStart();
    void runStarted();
    void runFailed(AbstractModule *mod, const QString &message);
    void runStopped();

    void resourceWarningUpdate(SystemResource kind, bool resolved, const QString &message);
    void connectionHeatChangedAtPort(VarStreamInputPort *iport, ConnectionHeatLevel hlevel);

private slots:
    void receiveModuleError(const QString &message);

    void onSynchronizerDetailsChanged(
        const QString &id,
        const TimeSyncStrategies &strategies,
        const microseconds_t &tolerance);
    void onSynchronizerOffsetChanged(const QString &id, const microseconds_t &currentOffset);
    void onDiskspaceMonitorEvent();
    void onMemoryMonitorEvent();
    void onBufferMonitorEvent();

private:
    class Private;
    Q_DISABLE_COPY(Engine)
    std::unique_ptr<Private> d;

    /**
     * @brief Holds the pre-computed start and stop ordering for one run.
     */
    struct ModuleRunOrder {
        QList<AbstractModule *> start;
        QList<AbstractModule *> stop;
        QList<AbstractModule *> inactive;
    };

    int obtainSleepShutdownIdleInhibitor();
    bool makeDirectory(const QString &dir);

    QHash<AbstractModule *, std::vector<uint>> setupCoreAffinityConfig(const QList<AbstractModule *> &threadedModules);
    void startResourceMonitoring(QList<AbstractModule *> activeModules, const QString &exportDirPath);
    void stopResourceMonitoring();

    bool finalizeExperimentMetadata(
        std::shared_ptr<EDLCollection> storageCollection,
        qint64 finishTimestamp,
        const QList<AbstractModule *> &activeModules);
    void setInactiveModuleInputPortsSuspended(bool suspended);

    void allocateNicenessBudget(const ModuleRunOrder &modOrder, uint maxRtThreads, int defaultThreadNice);
    bool runInternal(const QString &exportDirPath);
    void makeFinalExperimentId();
    void refreshExportDirPath();
    void emitStatusMessage(const QString &message);
    ModuleRunOrder createModuleRunOrder() const;
};

} // namespace Syntalos
