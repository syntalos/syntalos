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

#ifndef SPECTROGRAMDIALOG_H
#define SPECTROGRAMDIALOG_H

#include <QDialog>
#include "systemstate.h"
#include "spectrogramplot.h"

class QLabel;
class QComboBox;
class QSlider;
class QSpinBox;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QCheckBox;
class WaveformFifo;


class SpectrogramDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SpectrogramDialog(SystemState* state_, QWidget *parent = nullptr);
    ~SpectrogramDialog();

    void updateForRun();
    void updateForLoad();
    void updateForStop();
    void updateForChangeHeadstages();
    void updateSpectrogram(WaveformFifo *waveformFifo, int numSamples);
    void activate();

private slots:
    void updateFromState();
    void changeCurrentChannel(const QString& nativeChannelName)
        { state->spectrogramChannel->setValue(nativeChannelName); }
    void toggleLock() { updateFromState(); }
    void setToSelected();
    void changeDisplayMode(int index);
    void setNumFftPoints(int index)
        { state->fftSizeSpectrogram->setIndex(index); }
    void setFMin(int fMin);
    void setFMax(int fMax);
    void setFMarker(int fMarker)
        { state->fMarkerSpectrogram->setValue(fMarker); }
    void toggleShowFMarker(bool enabled);
    void setNumHarmonics(int num)
        { state->numHarmonicsSpectrogram->setValue(num); }
    void setTimeScale(int index)
        { state->tScaleSpectrogram->setIndex(index); }
    void setDigitalDisplay(int index)
        { state->digitalDisplaySpectrogram->setIndex(index); }
    void configSave();
    void saveData();

private:
    SystemState* state;

    QLabel *channelName;

    QCheckBox *lockScopeCheckbox;
    QPushButton *setToSelectedButton;

    QButtonGroup *displayModeButtonGroup;
    QRadioButton *spectrogramRadioButton;
    QRadioButton *spectrumRadioButton;

    QSpinBox *fMinSpinBox;
    QSpinBox *fMaxSpinBox;
    QSpinBox *fMarkerSpinBox;
    QLabel *fMarkerLabel;
    QCheckBox *fMarkerShowCheckBox;
    QSpinBox *fMarkerHarmonicsSpinBox;
    QLabel *fMarkerHarmonicsLabel1;
    QLabel *fMarkerHarmonicsLabel2;

    QLabel *deltaTimeLabel;
    QLabel *deltaFreqLabel;

    QSlider *nFftSlider;

    QLabel *timeScaleLabel;
    QComboBox *timeScaleComboBox;

    QComboBox *digitalDisplayComboBox;

    QPushButton *configSaveButton;
    QPushButton *saveButton;

    static const int FSpanMin = 10;

    SpectrogramPlot* specPlot;

    void updateDeltaTimeFreqLabels();
};


class SpectrogramSaveConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SpectrogramSaveConfigDialog(SystemState* state_, QWidget *parent = nullptr);

    QCheckBox *csvFileCheckBox;
    QCheckBox *matFileCheckBox;
    QCheckBox *pngFileCheckBox;

private:
    SystemState* state;
    QDialogButtonBox* buttonBox;
};

#endif // SPECTROGRAMDIALOG_H
