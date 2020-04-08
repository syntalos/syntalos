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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "utils.h"
#include "testsubjectlistmodel.h"
#include "moduleapi.h"

class QLabel;
class ModuleManager;
class ModuleIndicator;
class QSvgWidget;
namespace Syntalos {
class Engine;
class ModuleInfo;
class AbstractModule;
class TimingsDialog;
}

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setStatusText(const QString& msg);

    void changeExperimentId(const QString &text);
    void changeTestSubject(const TestSubject& subject);

private slots:
    void runActionTriggered();
    void stopActionTriggered();

    void openDataExportDirectory();

    void saveSettingsActionTriggered();
    void loadSettingsActionTriggered();

    void aboutActionTriggered();

    void onModuleCreated(ModuleInfo *info, AbstractModule *mod);
    void moduleErrorReceived(AbstractModule *mod, const QString& message);
    void onEnginePreRunStart();

    void statusMessageChanged(const QString &message);

    void on_actionSubjectsLoad_triggered();
    void on_actionSubjectsSave_triggered();
    void on_actionTimings_triggered();

    void showBusyIndicatorProcessing();
    void showBusyIndicatorRunning();
    void hideBusyIndicator();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateWindowTitle(const QString& fileName);
    void updateExportDirDisplay();

    void setRunPossible(bool enabled);
    void setStopPossible(bool enabled);

    void setDataExportBaseDir(const QString& dir);

    bool saveConfiguration(const QString& fileName);
    bool loadConfiguration(const QString& fileName);

    Ui::MainWindow *ui;

    Engine *m_engine;

    QLabel *m_statusBarLabel;
    QSvgWidget *m_busyIndicator;

    TestSubjectListModel *m_subjectList;

    TimingsDialog *m_timingsDialog;
};

#endif // MAINWINDOW_H
