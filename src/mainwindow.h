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
namespace KTextEditor {
class View;
}

class QVBoxLayout;
class IntanUI;
class QLabel;
class WavePlot;
class MazeScript;
class QTableWidget;
class StatusWidget;
class MazeVideo;
class VideoViewWidget;
class QSpinBox;
class QCheckBox;
class QDoubleSpinBox;
class QMdiSubWindow;
class QListWidgetItem;
class TracePlotProxy;
class ChannelDetails;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setStatusText(const QString& msg);

    void changeExperimentId(const QString &text);
    void changeTestSubject(const TestSubject& subject);

private slots:
    void runActionTriggered();
    void stopActionTriggered();

    void intanRunActionTriggered();

    void openDataExportDirectory();

    void firmataError(const QString& message);
    void scriptEvalError(const QString& message);

    void onMazeEvent(const QStringList& data);
    void onEventHeadersSet(const QStringList& headers);

    void videoError(const QString &message);

    void saveSettingsActionTriggered();
    void loadSettingsActionTriggered();

    void aboutActionTriggered();

    void changeExperimentFeatures(const ExperimentFeatures& features);
    void applyExperimentFeatureChanges();

    void on_portListWidget_itemActivated(QListWidgetItem *item);
    void on_chanListWidget_itemActivated(QListWidgetItem *item);
    void on_multiplierDoubleSpinBox_valueChanged(double arg1);
    void on_plotApplyButton_clicked();
    void on_yShiftDoubleSpinBox_valueChanged(double arg1);
    void on_prevPlotButton_toggled(bool checked);
    void on_chanDisplayCheckBox_clicked(bool checked);
    void on_plotRefreshSpinBox_valueChanged(int arg1);
    void on_cbIOFeature_toggled(bool checked);
    void on_cbEphysFeature_toggled(bool checked);
    void on_cbTrackingFeature_toggled(bool checked);
    void on_cbVideoFeature_toggled(bool checked);

protected:
    void closeEvent(QCloseEvent *event);

private:
    void updateWindowTitle(const QString& fileName);

    void setRunPossible(bool enabled);
    void setStopPossible(bool enabled);

    void setDataExportBaseDir(const QString& dir);
    void updateDataExportDir();

    bool makeDirectory(const QString& dir);

    ChannelDetails *selectedPlotChannelDetails();

    ExperimentFeatures m_features;

    Ui::MainWindow *ui;

    QLabel *m_statusBarLabel;
    QLabel *m_exportDirLabel;
    QLabel *m_exportDirInfoLabel;

    IntanUI *m_intanUI;
    QMdiSubWindow *m_intanTraceWin;

    QString m_experimentId;
    QString m_currentDate;

    TestSubjectListModel *m_subjectList;
    TestSubject m_currentSubject;

    QString m_dataExportBaseDir;
    QString m_dataExportDir;
    bool m_exportDirValid;

    MazeScript *m_msintf;

    bool m_failed;
    bool m_running;

    QTableWidget *m_mazeEventTable;
    QMdiSubWindow *m_mazeEventTableWin;

    KTextEditor::View *m_mscriptView;

    StatusWidget *m_statusWidget;

    MazeVideo *m_videoTracker;
    VideoViewWidget *m_rawVideoWidget;
    QMdiSubWindow *m_rawVideoWidgetWin;

    VideoViewWidget *m_trackVideoWidget;
    VideoViewWidget *m_trackInfoWidget;
    QMdiSubWindow *m_trackVideoWidgetWin;
    QMdiSubWindow *m_trackInfoWidgetWin;

    QSpinBox *m_fpsEdit;
    QDoubleSpinBox *m_exposureEdit;
    QSpinBox *m_eresWidthEdit;
    QSpinBox *m_eresHeightEdit;
    QCheckBox *m_gainCB;
    QLabel *m_ueyeConfFileLbl;
    QCheckBox *m_saveTarCB;
    QCheckBox *m_camFlashMode;

    QDialog *m_aboutDialog;

    TracePlotProxy *m_traceProxy;
};

#endif // MAINWINDOW_H
