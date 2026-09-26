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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <expected>

#include "engine.h"
#include "entitylistmodels.h"
#include "exitcodes.h"
#include "moduleapi.h"
#include "networkcontroller.h"
#include "utils/misc.h"

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
class QLabel;
class ModuleManager;
class ModuleIndicator;
class QSvgWidget;
class QSettings;

namespace Syntalos
{
class ModuleInfo;
class AbstractModule;
class TimingsDialog;
class LogViewDialog;
class GlobalConfig;
class IntervalRunDialog;
class SoundCuePlayer;
class TermSignalWatcher;
} // namespace Syntalos
#endif // DOXYGEN_SHOULD_SKIP_THIS

namespace Ui
{
class MainWindow;
}

/**
 * @brief Main application window
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setStatusText(const QString &msg);

    /**
     * @brief Load a project file synchronously, for automation.
     *
     * This will load the project file, apply all settings, and prepare the engine for a run.
     */
    auto loadProject(const QString &fname) -> std::expected<void, QString>;

    /**
     * @brief Load a project file as the user would, reporting errors to them.
     */
    void openProjectFile(const QString &fileName);

    /**
     * @brief Change the data export base directory of the loaded project, for automation.
     *
     * The directory is created if it does not exist yet. Loading another project
     * replaces this setting with the one stored in that project.
     */
    auto setExportDirectory(const QString &dir) -> std::expected<void, QString>;

    /**
     * @brief Start a run of the currently loaded project, for automation.
     *
     * This blocks until the run has finished. If @p maxDurationSec is greater than zero,
     * the run is stopped automatically after that many seconds.
     */
    auto startRun(bool ephemeral, int maxDurationSec) -> std::expected<void, QString>;

    /**
     * @brief Stop the current run, if there is one.
     */
    void stopRun();

    /**
     * @brief Stop any active run and quit the application.
     */
    void requestQuit();

    bool isProjectLoadInProgress() const;

    Engine *engine() const;

    /**
     * @brief Schedule running a project file autonomously.
     *
     * This function is mainly used for automation, and can be used to trigger a project
     * to be loaded, and then optionally launched for a period of time before quitting
     * the application, without any user interaction.
     *
     * @param projectFname       The project file to load and run.
     * @param ephemeral          If true, the run will be ephemeral, meaning that no data is stored.
     * @param runDurationSec     If > 0, stop the run automatically after this
     *                           many seconds and quit.
     */
    void scheduleProjectAutorun(const QString &projectFname, bool ephemeral, int runDurationSec = 0);

    void setNetPortOverrides(int controlPort, int feedbackPort);

    /**
     * If non-empty, overrides the export base directory stored in
     * the project file that is loaded next.
     */
    void setExportDirOverride(const QString &dir);

    /**
     * If non-empty, statistics of every completed run are written as JSON to this file.
     */
    void setRunStatisticsOutputFile(const QString &path);

private slots:
    void runActionTriggered();
    void runActionTriggered(const Uuid &recordIdOverride);
    void temporaryRunActionTriggered();
    void stopActionTriggered();

    void onActionNetControllerToggled(bool checked);
    void onActionNetListenerToggled(bool checked);

    void onNetPrepareReceived(const Uuid &runId);
    void onNetStopReceived(const Uuid &runId);

    void openDataExportDirectory();
    void showExperimenterSelector(const QString &message);

    void projectNewActionTriggered();
    void projectSaveAsActionTriggered();
    void projectSaveActionTriggered();
    void projectOpenActionTriggered();
    void updateRecentProjectsMenu();
    void openRecentProject(const QString &fileName);

    void on_actionProjectDetails_toggled(bool arg1);
    void globalConfigActionTriggered();
    void aboutActionTriggered();

    void onModuleCreated(ModuleInfo *info, AbstractModule *mod);
    void moduleErrorReceived(AbstractModule *mod, const QString &message);
    void onEnginePreRunPrepare();
    void onEngineRunStarted();
    void onEngineStopped();
    void onEngineRunFinishedCue();
    void onEngineModuleFailed(AbstractModule *mod, const QString &message, bool runStopping);
    void onEngineResourceWarningUpdate(Engine::SystemResource kind, bool resolved, const QString &message);
    void onEngineConnectionHeatChanged(VarStreamInputPort *iport, ConnectionHeatLevel hlevel);
    void onElapsedTimeUpdate();
    void onTerminationRequested(int signum);

    void statusMessageChanged(const QString &message);

    void showBusyIndicatorProcessing();
    void showBusyIndicatorRunning();
    void showBusyIndicatorWaiting();
    void hideBusyIndicator();

    void on_actionEditComment_triggered();
    void on_actionSubjectsLoad_triggered();
    void on_actionSubjectsSave_triggered();
    void on_actionTimings_triggered();
    void on_actionSystemInfo_triggered();
    void on_actionBenchmark_triggered();
    void on_actionUsbDevices_triggered();
    void on_actionShowLog_triggered();
    void on_actionIntervalRunConfig_triggered();
    void on_actionOnlineDocs_triggered();
    void on_actionReportIssue_triggered();
    void on_actionHelpDiscuss_triggered();
    void on_actionOpenCrashCollector_triggered();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void applySelectedAppStyle(bool updateIcons = true);
    void updateIconStyles();
    void shutdown(int errorCode = 0);
    void closeOnTerminationRequest();
    void setCurrentProjectFile(const QString &fileName);
    void setExportDirSafe(const QString &dir);
    void updateExportDirDisplay();
    void updateIntervalRunMessage();
    [[nodiscard]] QByteArray loadBusyAnimation(const QString &name) const;

    void changeExperimenter(const EDLAuthor &person);
    void changeTestSubject(const TestSubject &subject);
    void changeExperimentId(const QString &text);

    QList<ExportPathComponent> exportDirLayoutFromUi() const;
    void changeExportDirLayout(const QList<ExportPathComponent> &layout);

    void setRunPossible(bool enabled);
    void updateManualRunPossible();
    void setRunUiControlStates(bool engineRunning, bool stopPossible);
    void setConfigModifyAllowed(bool allowed);
    void setExperimenterSelectVisible(bool visible);

    bool saveConfiguration(const QString &fileName);
    auto loadConfiguration(const QString &fileName) -> std::expected<void, QString>;

    NetworkControlConfig buildNetControlConfig() const;
    void applyNetControllerConfig();

    Ui::MainWindow *ui;
    QuillLogger *m_log;

    Syntalos::GlobalConfig *m_gconf;
    Syntalos::SoundCuePlayer *m_soundCues;
    Syntalos::TermSignalWatcher *m_termSignalWatcher;
    int m_terminationSignal{0};
    QTimer *m_rtElapsedTimer;
    Engine *m_engine;
    QString m_currentProjectFname;
    bool m_isIntervalRun;
    bool m_configLoadInProgress;
    bool m_whatsNewChecked;
    seconds_t m_runMaxDuration;

    int m_netCtlPortOverride{-1};
    int m_netFbPortOverride{-1};
    QString m_exportDirOverride;

    QLabel *m_statusBarLabel;
    QSvgWidget *m_busyIndicator;

    TestSubjectListModel *m_subjectList;
    ExperimenterListModel *m_experimenterList;

    TimingsDialog *m_timingsDialog;
    LogViewDialog *m_logViewDialog;
    IntervalRunDialog *m_intervalRunDialog;
};

#endif // MAINWINDOW_H
