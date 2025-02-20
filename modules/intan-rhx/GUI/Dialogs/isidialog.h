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

#ifndef ISIDIALOG_H
#define ISIDIALOG_H

#include <QDialog>
#include "systemstate.h"
#include "isiplot.h"

class QLabel;
class QComboBox;
class QPushButton;
class QCheckBox;
class WaveformFifo;

class ISIDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ISIDialog(SystemState* state_, QWidget *parent = nullptr);
    ~ISIDialog();

    void updateForRun();
    void updateForLoad();
    void updateForStop();
    void updateForChangeHeadstages();

    void updateISI(WaveformFifo *waveformFifo, int numSamples);
    void activate();

private slots:
    void updateFromState();
    void changeCurrentChannel(const QString& nativeChannelName)
        { state->isiChannel->setValue(nativeChannelName); }
    void toggleLock() { updateFromState(); }
    void setToSelected();
    void setTimeSpan(int index)
        { state->tSpanISI->setIndex(index); }
    void setBinSize(int index)
        { state->binSizeISI->setIndex(index); }
    void changeYAxisMode(int index)
        { state->yAxisLogISI->setValue((bool) index); }
    void clearISI() { isiPlot->resetISI(); isiPlot->setFocus(); }
    void configSave();
    void saveData();

private:
    SystemState* state;

    QLabel *channelName;

    QCheckBox *lockScopeCheckbox;
    QPushButton *setToSelectedButton;

    QComboBox *timeSpanComboBox;
    QComboBox *binSizeComboBox;

    QButtonGroup *yAxisButtonGroup;
    QRadioButton *yAxisLinearRadioButton;
    QRadioButton *yAxisLogRadioButton;

    QPushButton *clearISIPushButton;

    QPushButton *configSaveButton;
    QPushButton *saveButton;

    ISIPlot* isiPlot;

    void updateTitle();
};


class ISISaveConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ISISaveConfigDialog(SystemState* state_, QWidget *parent = nullptr);

    QCheckBox *csvFileCheckBox;
    QCheckBox *matFileCheckBox;
    QCheckBox *pngFileCheckBox;

private:
    SystemState* state;
    QDialogButtonBox* buttonBox;
};

#endif // ISIDIALOG_H
