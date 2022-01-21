/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <QWidget>
#include "moduleapi.h"

class QListWidgetItem;
class QTimer;
class TracePlot;
class PlotChannelData;

namespace Ui {
class TraceDisplay;
}

namespace QtCharts {
class QXYSeries;
}

class TraceDisplay final : public QWidget
{
    Q_OBJECT

public:
    explicit TraceDisplay(QWidget *parent = nullptr);
    ~TraceDisplay();

    void addIntPort(std::shared_ptr<StreamInputPort<IntSignalBlock> > port);
    void addFloatPort(std::shared_ptr<StreamInputPort<FloatSignalBlock> > port);
    void updatePortChannels();

    void updatePlotData(bool adjustView = true);

    void plotAdjustView();
    void plotMoveTo(int position);

    void resetPlotConfig();

private slots:
    void repaintPlot();

    void on_portListWidget_currentItemChanged(QListWidgetItem *item, QListWidgetItem *);
    void on_chanListWidget_currentItemChanged(QListWidgetItem *, QListWidgetItem *);
    void on_multiplierDoubleSpinBox_valueChanged(double arg1);
    void on_plotApplyButton_clicked();
    void on_yShiftDoubleSpinBox_valueChanged(double arg1);
    void on_prevPlotButton_toggled(bool checked);

    void on_chanDisplayCheckBox_clicked(bool checked);
    void on_plotRefreshSpinBox_valueChanged(int arg1);

private:
    Ui::TraceDisplay *ui;
    TracePlot *m_plot;
    int m_maxXVal;
    QTimer *m_timer;
    QList<QPair<std::shared_ptr<VarStreamInputPort>, QList<PlotChannelData*>>> m_portsChannels;

    QList<QPair<std::shared_ptr<StreamSubscription<FloatSignalBlock>>, QList<PlotChannelData*>>> m_activeFSubChans;
    QList<QPair<std::shared_ptr<StreamSubscription<IntSignalBlock>>, QList<PlotChannelData*>>> m_activeISubChans;

    void addChannel(int streamIndex, int chanIndex);
    PlotChannelData *selectedPlotChannelData();
};
