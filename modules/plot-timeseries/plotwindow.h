/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QListWidgetItem>

#include "timeplotwidget.h"

namespace Syntalos
{
class AbstractModule;
}
namespace Ui
{
class PlotWindow;
}

using namespace Syntalos;

class PlotWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit PlotWindow(AbstractModule *mod, QWidget *parent = nullptr);
    ~PlotWindow();

    void updatePortLists();
    void setSignalsForPort(const QString &portId, const QStringList &signalNames);
    TimePlotWidget *plotWidgetForPort(const QString &portId);

    bool signalIsShown(const QString &portId, const QString &signalName);
    PlotSeriesSettings signalPlotSettingsFor(const QString &portId, const QString &signalName);
    QList<PlotSeriesSettings> signalPlotSettingsFor(const QString &portId);
    void setSignalPlotSettings(const QString &portId, const PlotSeriesSettings &pss);

    void setRunning(bool running);

    bool defaultSettingsVisible();
    void setDefaultSettingsVisible(bool visible);

private slots:
    void on_settingsDisplayBtn_clicked();
    void on_portListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_sigListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_portListWidget_clicked(const QModelIndex &index);
    void on_sigListWidget_clicked(const QModelIndex &index);

    void on_addPortBtn_clicked();
    void on_removePortBtn_clicked();

    void on_showSignalCheckBox_toggled(bool checked);
    void on_digitalCheckBox_toggled(bool checked);

private:
    Ui::PlotWindow *ui;
    AbstractModule *m_mod;

    bool m_defaultSettingsVisible;
    QHash<QString, TimePlotWidget *> m_plotWidgets;
    QHash<QString, QMap<QString, PlotSeriesSettings>> m_signalDetails;

    void setSettingsPanelVisible(bool visible);
    bool checkAnyPortSignalsVisible(const QString &portId);
};
