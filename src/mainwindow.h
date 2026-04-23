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

#include "engine.h"
#include "entitylistmodels.h"
#include "moduleapi.h"
#include "utils/misc.h"

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
class QLabel;
class ModuleManager;
class ModuleIndicator;
class QSvgWidget;
class QSettings;
class FlowGraphEdge;
namespace Syntalos
{
class ModuleInfo;
class AbstractModule;
class TimingsDialog;
class GlobalConfig;
class IntervalRunDialog;
} // namespace Syntalos
#endif // DOXYGEN_SHOULD_SKIP_THIS

namespace Ui
{
class MainWindow;
}

// Exit codes for the application
constexpr int SY_EXIT_SUCCESS = 0;
constexpr int SY_EXIT_FAILURE = 1;
constexpr int SY_EXIT_LOAD_ERROR = 2;
constexpr int SY_EXIT_PERMISSION_ERROR = 3;
constexpr int SY_EXIT_NOT_FOUND = 4;
constexpr int SY_EXIT_RUN_FAILED = 5;

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
     * @brief Load a project file and apply its configuration.
     *
     * This will load the project file, apply all settings, and prepare the engine for a run,
     * but it will not start the run.
     *
     * @param fname The project file to load.
     */
    void loadProjectFilename(const QString &fname);

    /**
     * @brief Schedule running a project file autonomously.
     *
     * This function is mainly used for automation, and can be used to trigger a project
     * to be loaded, and then optionally launched for a period of time before quitting
     * the application, without any user interaction.
     *
     * @param projectFname       The project file to load and run.
     * @param overrideExportDir  If non-empty, overrides the export base
     *                           directory stored in the project file.
     * @param ephemeral          If true, the run will be ephemeral, meaning that
     * @param noninteractive     If true, try to reduce GUI interactions.
     * @param runDurationSec     If > 0, stop the run automatically after this
     *                           many seconds and quit.  Implies @p autoRun.
     */
    void scheduleProjectAutorun(
        const QString &projectFname,
        const QString &overrideExportDir,
        bool ephemeral,
        bool noninteractive,
        int runDurationSec = 0);

private slots:
    void runActionTriggered();
    void temporaryRunActionTriggered();
    void stopActionTriggered();

    void openDataExportDirectory();
    void showExperimenterSelector(const QString &message);

    void projectNewActionTriggered();
    void projectSaveAsActionTriggered();
    void projectSaveActionTriggered();
    void projectOpenActionTriggered();

    void on_actionProjectDetails_toggled(bool arg1);
    void globalConfigActionTriggered();
    void aboutActionTriggered();

    void onModuleCreated(ModuleInfo *info, AbstractModule *mod);
    void moduleErrorReceived(AbstractModule *mod, const QString &message);
    void onEnginePreRunStart();
    void onEngineRunStarted();
    void onEngineStopped();
    void onEngineResourceWarningUpdate(Engine::SystemResource kind, bool resolved, const QString &message);
    void onEngineConnectionHeatChanged(VarStreamInputPort *iport, ConnectionHeatLevel hlevel);
    void onElapsedTimeUpdate();

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
    void on_actionUsbDevices_triggered();
    void on_actionModuleLoadInfo_triggered();
    void on_actionIntervalRunConfig_triggered();
    void on_actionOnlineDocs_triggered();
    void on_actionReportIssue_triggered();
    void on_actionHelpDiscuss_triggered();
    void on_actionOpenCrashCollector_triggered();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void applySelectedAppStyle(bool updateIcons = true);
    void updateIconStyles();
    void shutdown(int errorCode = 0);
    void setCurrentProjectFile(const QString &fileName);
    void setDataExportBaseDir(const QString &dir);
    void updateExportDirDisplay();
    void updateIntervalRunMessage();
    [[nodiscard]] QByteArray loadBusyAnimation(const QString &name) const;

    void changeExperimenter(const EDLAuthor &person);
    void changeTestSubject(const TestSubject &subject);
    void changeExperimentId(const QString &text);

    QList<ExportPathComponent> exportDirLayoutFromUi() const;
    void changeExportDirLayout(const QList<ExportPathComponent> &layout);

    void setRunPossible(bool enabled);
    void setRunUiControlStates(bool engineRunning, bool stopPossible);
    void setConfigModifyAllowed(bool allowed);
    void setExperimenterSelectVisible(bool visible);

    bool saveConfiguration(const QString &fileName);
    bool loadConfiguration(const QString &fileName);

    Ui::MainWindow *ui;
    QuillLogger *m_log;

    Syntalos::GlobalConfig *m_gconf;
    QTimer *m_rtElapsedTimer;
    Engine *m_engine;
    QString m_currentProjectFname;
    bool m_isIntervalRun;
    bool m_configLoadInProgress;
    seconds_t m_runMaxDuration;

    QLabel *m_statusBarLabel;
    QSvgWidget *m_busyIndicator;

    TestSubjectListModel *m_subjectList;
    ExperimenterListModel *m_experimenterList;

    TimingsDialog *m_timingsDialog;
    IntervalRunDialog *m_intervalRunDialog;

    QHash<VarStreamInputPort *, FlowGraphEdge *> m_portGraphEdgeCache;
};

#endif // MAINWINDOW_H
