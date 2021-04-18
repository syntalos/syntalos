//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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

#ifndef SPIKESORTINGDIALOG_H
#define SPIKESORTINGDIALOG_H

#include <QDialog>

#include "systemstate.h"
#include "xmlinterface.h"
#include "spikeplot.h"

class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;
class QCheckBox;
class QTabWidget;
class WaveformFifo;
class ControllerInterface;

class SpikeSortingDialog : public QDialog
{
    Q_OBJECT
public:
    SpikeSortingDialog(SystemState* state_, ControllerInterface* controllerInterface_, QWidget *parent = nullptr);
    ~SpikeSortingDialog();

    void updateForRun();
    void updateForLoad();
    void updateForStop();
    void updateForChangeHeadstages();

    void updateSpikeScope(WaveformFifo *waveformFifo, int numSamples);
    void activate();

private slots:
    void updateFromState();
    void setVoltageThreshold(int threshold)
        { state->signalSources->channelByName(state->spikeScopeChannel->getValue())->setSpikeThreshold(threshold); }
    void setVoltageScale(int index)
        { state->yScaleSpikeScope->setIndex(index); }
    void setTimeScale(int index)
        { state->tScaleSpikeScope->setIndex(index); }
    void setNumSpikesDisplayed(int index)
        { state->numSpikesDisplayed->setIndex(index); }
    void loadSpikeSortingParameters();
    void saveSpikeSortingParameters();
    void clearScope() { spikePlot->clearSpikes(); spikePlot->setFocus(); }
    void takeSnapshot() { spikePlot->takeSnapshot(); spikePlot->clearSpikes(); spikePlot->setFocus(); }
    void clearSnapshot() { spikePlot->clearSnapshot(); spikePlot->setFocus(); }
    void changeCurrentChannel(const QString& nativeChannelName)
        { state->spikeScopeChannel->setValue(nativeChannelName); }
    void toggleLock() { updateFromState(); }
    void setToSelected();
    void toggleSuppressionEnabled(bool enabled);
    void toggleArtifactsShown(bool enabled)
        { state->artifactsShown->setValue(enabled); }
    void setSuppressionThreshold()
        { state->suppressionThreshold->setValue(suppressionThresholdSpinBox->value()); }

private:   
    SystemState* state;
    ControllerInterface* controllerInterface;

    XMLInterface *spikeSettingsInterface;

    QPushButton *loadSpikeSortingParametersButton;
    QPushButton *saveSpikeSortingParametersButton;

    QLabel *channelName;

    QCheckBox *lockScopeCheckbox;
    QPushButton *setToSelectedButton;

    QSpinBox *thresholdSpinBox;

    QComboBox *voltageScaleComboBox;

    QComboBox *timeScaleComboBox;

    QComboBox *showSpikesComboBox;

    QPushButton *clearScopeButton;
    QPushButton *takeSnapshotButton;
    QPushButton *clearSnapshotButton;

    QCheckBox *suppressionCheckBox;
    QCheckBox *artifactsShownCheckBox;
    QLabel *suppressionThresholdLabel1;
    QSpinBox *suppressionThresholdSpinBox;

    QTabWidget *unitTabWidget;
    QVector<QWidget*> unitTabs;
    QVector<QCheckBox*> hoopEnabledCheckBoxes;
    QVector<QLabel*> tALabels;
    QVector<QLabel*> vALabels;
    QVector<QLabel*> tBLabels;
    QVector<QLabel*> vBLabels;

    SpikePlot *spikePlot;
};

#endif // SPIKESORTINGDIALOG_H
