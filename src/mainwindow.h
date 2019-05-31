/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

class QLabel;
class ModuleManager;
class AbstractModule;
class ModuleIndicator;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void on_tbAddModule_clicked();

    void moduleAdded(AbstractModule *mod);
    void receivedModuleError(AbstractModule *mod, const QString& message);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void updateWindowTitle(const QString& fileName);

    void setRunPossible(bool enabled);
    void setStopPossible(bool enabled);

    void setDataExportBaseDir(const QString& dir);
    void updateDataExportDir();

    bool saveConfiguration(const QString& fileName);
    bool loadConfiguration(const QString& fileName);

    bool makeDirectory(const QString& dir);

    ModuleManager *m_modManager;

    Ui::MainWindow *ui;

    QLabel *m_statusBarLabel;
    QLabel *m_exportDirLabel;
    QLabel *m_exportDirInfoLabel;

    QString m_experimentId;
    QString m_currentDate;

    TestSubjectListModel *m_subjectList;
    TestSubject m_currentSubject;

    QString m_dataExportBaseDir;
    QString m_dataExportDir;
    bool m_exportDirValid;

    bool m_failed;
    bool m_running;

    QDialog *m_aboutDialog;
};

#endif // MAINWINDOW_H
