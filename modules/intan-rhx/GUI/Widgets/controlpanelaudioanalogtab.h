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

#ifndef CONTROLPANELAUDIOANALOGTAB_H
#define CONTROLPANELAUDIOANALOGTAB_H

#include <QtWidgets>
#include "controllerinterface.h"
#include "systemstate.h"

class AnalogOutConfigDialog;

class ControlPanelAudioAnalogTab : public QWidget
{
    Q_OBJECT
public:
    explicit ControlPanelAudioAnalogTab(ControllerInterface* controllerInterface_, SystemState* state_, QWidget *parent = nullptr);
    ~ControlPanelAudioAnalogTab();

    void updateFromState();

private slots:
    void changeAudioFilter(int filterIndex) { state->audioFilter->setIndex(filterIndex); }
    void enableAudio(bool enabled);
    void changeVolume(int volume);
    void changeThreshold(int threshold);
    void changeDacGain(int index);
    void changeDacNoiseSuppress(int index);
    void openDacConfigDialog();
    void dac1LockToSelected(bool enable);
    void enableDacHighpassFilter(bool enable);
    void setDacHighpassFilterFrequency();
    void dac1SetChannel();
    void dac2SetChannel();
    void dac1Disable();
    void dac2Disable();

private:
    SystemState* state;
    ControllerInterface* controllerInterface;

    AnalogOutConfigDialog* analogOutConfigDialog;

    QCheckBox *audioEnabledCheckBox;
    QComboBox *audioFilterComboBox;
    QLabel *audioChannelLabel;

    QLabel *audioVolumeLabel;
    QSlider *audioVolumeSlider;
    QLabel *audioVolumeValueLabel;

    QLabel *audioThresholdLabel;
    QSlider *audioThresholdSlider;
    QLabel *audioThresholdValueLabel;

    QSlider *dacGainSlider;
    QSlider *dacNoiseSuppressSlider;

    QLabel *dacGainLabel;
    QLabel *dacNoiseSuppressLabel;
    QCheckBox *dacHighpassFilterCheckBox;
    QCheckBox *dacLockToSelectedCheckBox;
    QLineEdit *dacHighpassFilterLineEdit;

    QLabel *dac1Label;
    QLabel *dac2Label;

    QPushButton *dac1SetButton;
    QPushButton *dac2SetButton;
    QPushButton *dac1DisableButton;
    QPushButton *dac2DisableButton;
    QPushButton *openDacConfigButton;

    int gainIndexOld;
    int noiseSuppressIndexOld;
    bool analogOutHighpassFilterEnabledOld;
    double analogOutHighpassFilterFrequencyOld;

    QString analogOut1ChannelOld;
    QString analogOut2ChannelOld;
    QString analogOut3ChannelOld;
    QString analogOut4ChannelOld;
    QString analogOut5ChannelOld;
    QString analogOut6ChannelOld;
    QString analogOut7ChannelOld;
    QString analogOut8ChannelOld;
    QString analogRefChannelOld;

    int analogOut1ThresholdOld;
    int analogOut2ThresholdOld;
    int analogOut3ThresholdOld;
    int analogOut4ThresholdOld;
    int analogOut5ThresholdOld;
    int analogOut6ThresholdOld;
    int analogOut7ThresholdOld;
    int analogOut8ThresholdOld;

    bool analogOut1ThresholdEnabledOld;
    bool analogOut2ThresholdEnabledOld;
    bool analogOut3ThresholdEnabledOld;
    bool analogOut4ThresholdEnabledOld;
    bool analogOut5ThresholdEnabledOld;
    bool analogOut6ThresholdEnabledOld;
    bool analogOut7ThresholdEnabledOld;
    bool analogOut8ThresholdEnabledOld;

    void setAllThresholdEnabledValues();
};

#endif // CONTROLPANELAUDIOANALOGTAB_H
