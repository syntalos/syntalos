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

#include <QWidget>

#include "plotcanvas.h"

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

    PlotCanvas *canvas() const;

    void setRunning(bool running);

    bool defaultSettingsVisible();
    void setDefaultSettingsVisible(bool visible);

    int updateFrequency() const;
    void setUpdateFrequency(int hz);

    int bufferSize() const;
    void setBufferSize(int kitems);

    void refreshChannelTable();

private slots:
    void on_settingsDisplayBtn_clicked();
    void on_addPortBtn_clicked();
    void on_removePortBtn_clicked();
    void on_resetLayoutBtn_clicked();
    void on_speedSpinBox_valueChanged(int arg1);
    void on_bufferSizeSpinBox_valueChanged(int arg1);

private:
    Ui::PlotWindow *ui;
    AbstractModule *m_mod;
    PlotCanvas *m_canvas;
    bool m_running;
    bool m_defaultSettingsVisible;

    void setSettingsPanelVisible(bool visible);
    void onShowToggled(int channelIndex, bool checked);
    void onDigitalToggled(int channelIndex, bool checked);
};
