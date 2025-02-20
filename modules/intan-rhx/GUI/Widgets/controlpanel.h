//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QtWidgets>
#include "abstractpanel.h"
#include "controllerinterface.h"
#include "systemstate.h"
#include "commandparser.h"
#include "filterdisplayselector.h"

class ControlWindow;
class StimParamDialog;
class AnOutDialog;
class DigOutDialog;
class ControlPanelBandwidthTab;
class ControlPanelImpedanceTab;
class ControlPanelAudioAnalogTab;
class ControlPanelConfigureTab;
class ControlPanelTriggerTab;

class ControlPanel : public AbstractPanel
{
    Q_OBJECT
public:
    explicit ControlPanel(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser* parser_,
                          ControlWindow *parent = nullptr);
    void updateForRun() override final;
    void updateForStop() override final;

    void updateSlidersEnabled(YScaleUsed yScaleUsed) override final;
    YScaleUsed slidersEnabled() const override final;

    void setCurrentTabName(QString tabName) override final;
    QString currentTabName() const override final;

public slots:
    void updateFromState() override final;

private slots:

private:
    QHBoxLayout* createSelectionLayout() override final;
    QHBoxLayout* createSelectionToolsLayout() override final;
    QHBoxLayout* createDisplayLayout() override final;

    void updateYScales() override final;

    QToolButton *hideControlPanelButton;
    QLabel* topLabel;
    ControlPanelBandwidthTab *bandwidthTab;
    ControlPanelImpedanceTab *impedanceTab;
    ControlPanelAudioAnalogTab *audioAnalogTab;
    ControlPanelTriggerTab *triggerTab;

    QSlider *lowSlider;
    QSlider *highSlider;
    QSlider *analogSlider;

    QLabel *lowLabel;
    QLabel *highLabel;
    QLabel *analogLabel;

};

#endif // CONTROLPANEL_H
