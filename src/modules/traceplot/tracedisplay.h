/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef TRACEDISPLAY_H
#define TRACEDISPLAY_H

#include <QWidget>

class TracePlotProxy;
class QListWidgetItem;
class ChannelDetails;

namespace Ui {
class TraceDisplay;
}

class TraceDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit TraceDisplay(TracePlotProxy *proxy, QWidget *parent = nullptr);
    ~TraceDisplay();

private slots:
    void on_portListWidget_itemActivated(QListWidgetItem *item);
    void on_chanListWidget_itemActivated(QListWidgetItem *item);
    void on_multiplierDoubleSpinBox_valueChanged(double arg1);
    void on_plotApplyButton_clicked();
    void on_yShiftDoubleSpinBox_valueChanged(double arg1);
    void on_prevPlotButton_toggled(bool checked);

    void on_chanDisplayCheckBox_clicked(bool checked);
    void on_plotRefreshSpinBox_valueChanged(int arg1);

private:
    Ui::TraceDisplay *ui;
    TracePlotProxy *m_traceProxy;

    void setPlotProxy(TracePlotProxy *proxy);
    ChannelDetails *selectedPlotChannelDetails();
};

#endif // TRACEDISPLAY_H
