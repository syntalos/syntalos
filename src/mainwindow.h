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
#include "barrier.h"

namespace Ui {
class MainWindow;
}

class QVBoxLayout;
class QLabel;
class QTableWidget;
class StatusWidget;
class MazeVideo;
class QSpinBox;
class QCheckBox;
class QDoubleSpinBox;
class QMdiSubWindow;
class QListWidgetItem;
class TracePlotProxy;
class ModuleManager;
class AbstractModule;
class ModuleIndicator;


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

    void firmataError(const QString& message);
    void scriptEvalError(const QString& message);

    void onMazeEvent(const QStringList& data);
    void onEventHeadersSet(const QStringList& headers);

    void videoError(const QString &message);

    void saveSettingsActionTriggered();
    void loadSettingsActionTriggered();

    void aboutActionTriggered();
    void on_tbAddModule_clicked();

    void moduleAdded(AbstractModule *mod);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void updateWindowTitle(const QString& fileName);

    void setRunPossible(bool enabled);
    void setStopPossible(bool enabled);

    void setDataExportBaseDir(const QString& dir);
    void updateDataExportDir();

    bool makeDirectory(const QString& dir);

    ModuleManager *m_modManager;
    ExperimentFeatures m_features;

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

    QTableWidget *m_mazeEventTable;
    QMdiSubWindow *m_mazeEventTableWin;

    StatusWidget *m_statusWidget;

    QSpinBox *m_fpsEdit;
    QDoubleSpinBox *m_exposureEdit;
    QSpinBox *m_eresWidthEdit;
    QSpinBox *m_eresHeightEdit;
    QCheckBox *m_gainCB;
    QLabel *m_ueyeConfFileLbl;
    QCheckBox *m_camFlashMode;

    QDialog *m_aboutDialog;
};

#endif // MAINWINDOW_H
