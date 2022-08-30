//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.1.0
//
//  Copyright (c) 2020-2022 Intan Technologies
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

class ColorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWidget(QWidget *parent = nullptr);

    void setEnabled(bool enabled) { chooseColorToolButton->setEnabled(enabled); }
    void setColor(QColor color);
    void setBlank() { chooseColorToolButton->setIcon(blankIcon); }
    void setCheckerboard() { chooseColorToolButton->setIcon(checkerboardIcon); }

signals:
    void clicked();

private:
    QToolButton *chooseColorToolButton;

    QIcon checkerboardIcon;
    QIcon blankIcon;
};


class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanel(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser* parser_,
                          ControlWindow *parent = nullptr);
    ~ControlPanel();

    void updateForRun();
    void updateForLoad();
    void updateForStop();

    void updateSlidersEnabled(YScaleUsed yScaleUsed);
    YScaleUsed slidersEnabled() const;

    void setCurrentTabName(QString tabName);
    QString currentTabName() const;

public slots:
    void updateFromState();

private slots:
    void promptColorChange();
    void clipWaveforms(int checked);
    void changeTimeScale(int index);
    void changeWideScale(int index);
    void changeLowScale(int index);
    void changeHighScale(int index);
    void changeAuxScale(int index);
    void changeAnaScale(int index);
    void changeDCSScale(int index);
    void openStimParametersDialog();
    void enableChannelsSlot();

private:
    SystemState* state;
    CommandParser* parser;
    ControlWindow* controlWindow;
    ControllerInterface* controllerInterface;

    FilterDisplaySelector *filterDisplaySelector;

    QToolButton *hideControlPanelButton;
    QLabel* topLabel;
    QTabWidget *tabWidget;
    ControlPanelBandwidthTab *bandwidthTab;
    ControlPanelImpedanceTab *impedanceTab;
    ControlPanelAudioAnalogTab *audioAnalogTab;
    ControlPanelConfigureTab *configureTab;
    ControlPanelTriggerTab *triggerTab;

    QList<Channel*> selectedSignals;

    StimParamDialog* stimParamDialog;
    AnOutDialog* anOutDialog;
    DigOutDialog* digOutDialog;

    QLabel *selectionNameLabel;
    QLabel *selectionImpedanceLabel;
    QLabel *selectionReferenceLabel;
    QLabel *selectionStimTriggerLabel;

    ColorWidget *colorAttribute;
    QCheckBox *enableCheckBox;
    QPushButton *renameButton;
    QPushButton *setRefButton;
    QPushButton *setStimButton;

    QSlider *wideSlider;
    QSlider *lowSlider;
    QSlider *highSlider;
    QSlider *variableSlider;
    QSlider *analogSlider;

    QLabel *wideLabel;
    QLabel *lowLabel;
    QLabel *highLabel;
    QLabel *variableLabel;
    QLabel *analogLabel;

    QCheckBox* clipWaveformsCheckBox;
    QComboBox *timeScaleComboBox;

    QHBoxLayout* createSelectionLayout();
    QHBoxLayout* createSelectionToolsLayout();
    QHBoxLayout* createDisplayLayout();

    void updateSelectionName();
    void updateElectrodeImpedance();
    void updateReferenceChannel();
    void updateStimTrigger();
    void updateColor();
    void updateEnableCheckBox();
    void updateYScales();
    void updateStimParamDialogButton();
};

#endif // CONTROLPANEL_H
