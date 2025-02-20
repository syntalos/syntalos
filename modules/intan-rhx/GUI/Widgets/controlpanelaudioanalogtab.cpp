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

#include <iostream>
#include "analogoutconfigdialog.h"
#include "controlpanelaudioanalogtab.h"

ControlPanelAudioAnalogTab::ControlPanelAudioAnalogTab(ControllerInterface* controllerInterface_, SystemState* state_,
                                                       QWidget *parent) :
    QWidget(parent),
    state(state_),
    controllerInterface(controllerInterface_),
    analogOutConfigDialog(nullptr),
    audioEnabledCheckBox(nullptr),
    audioFilterComboBox(nullptr),
    audioChannelLabel(nullptr),
    audioVolumeLabel(nullptr),
    audioVolumeSlider(nullptr),
    audioVolumeValueLabel(nullptr),
    audioThresholdLabel(nullptr),
    audioThresholdSlider(nullptr),
    audioThresholdValueLabel(nullptr),
    dacGainSlider(nullptr),
    dacNoiseSuppressSlider(nullptr),
    dacGainLabel(nullptr),
    dacNoiseSuppressLabel(nullptr),
    dacHighpassFilterCheckBox(nullptr),
    dacLockToSelectedCheckBox(nullptr),
    dacHighpassFilterLineEdit(nullptr),
    dac1Label(nullptr),
    dac2Label(nullptr),
    dac1SetButton(nullptr),
    dac2SetButton(nullptr),
    dac1DisableButton(nullptr),
    dac2DisableButton(nullptr),
    openDacConfigButton(nullptr),
    gainIndexOld(-1),
    noiseSuppressIndexOld(-1),
    analogOutHighpassFilterEnabledOld(true),
    analogOutHighpassFilterFrequencyOld(-1.0),
    analogOut1ThresholdOld(0),
    analogOut2ThresholdOld(0),
    analogOut3ThresholdOld(0),
    analogOut4ThresholdOld(0),
    analogOut5ThresholdOld(0),
    analogOut6ThresholdOld(0),
    analogOut7ThresholdOld(0),
    analogOut8ThresholdOld(0),
    analogOut1ThresholdEnabledOld(false),
    analogOut2ThresholdEnabledOld(false),
    analogOut3ThresholdEnabledOld(false),
    analogOut4ThresholdEnabledOld(false),
    analogOut5ThresholdEnabledOld(false),
    analogOut6ThresholdEnabledOld(false),
    analogOut7ThresholdEnabledOld(false),
    analogOut8ThresholdEnabledOld(false)
{
    QLabel* dacGainPreLabel = new QLabel(tr("Electrode to ANALOG OUT Gain"), this);
    QLabel* dacNoiseSuppressPreLabel = new QLabel(tr("Noise Slicer (ANALOG OUT 1,2)"), this);
    int labelWidth = std::max(fontMetrics().horizontalAdvance(dacGainPreLabel->text()), fontMetrics().horizontalAdvance(dacNoiseSuppressPreLabel->text()));
    dacGainPreLabel->setFixedWidth(labelWidth);
    dacNoiseSuppressPreLabel->setFixedWidth(labelWidth);

    dacGainSlider = new QSlider(Qt::Horizontal, this);
    dacNoiseSuppressSlider = new QSlider(Qt::Horizontal, this);

    dacGainSlider->setRange(0, 7);
    dacGainSlider->setValue(state->analogOutGainIndex->getValue());
    dacNoiseSuppressSlider->setRange(0, 64);
    dacNoiseSuppressSlider->setValue(state->analogOutNoiseSlicerIndex->getValue());

    dacGainSlider->setStyleSheet("height: 16px");
    dacNoiseSuppressSlider->setStyleSheet("height: 16px");
    dacGainSlider->setFixedWidth(65);
    dacNoiseSuppressSlider->setFixedWidth(65);

    dacGainLabel = new QLabel(this);
    dacNoiseSuppressLabel = new QLabel(this);

    labelWidth = fontMetrics().horizontalAdvance("204.8 mV/" + MicroVoltsSymbol);
    dacGainLabel->setFixedWidth(labelWidth);
    dacNoiseSuppressLabel->setFixedWidth(labelWidth);

    connect(dacGainSlider, SIGNAL(valueChanged(int)), this, SLOT(changeDacGain(int)));
    connect(dacNoiseSuppressSlider, SIGNAL(valueChanged(int)), this, SLOT(changeDacNoiseSuppress(int)));

    dacHighpassFilterCheckBox = new QCheckBox(tr("Enable ANALOG OUT High Pass Filter"), this);
    connect(dacHighpassFilterCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableDacHighpassFilter(bool)));

    dacHighpassFilterLineEdit = new QLineEdit(QString::number(state->analogOutHighpassFilterFrequency->getValue(), 'f', 0), this);
    dacHighpassFilterLineEdit->setFixedWidth(dacHighpassFilterLineEdit->fontMetrics().horizontalAdvance("20000.0"));
    QDoubleValidator* dacHighpassFilterValidator = new QDoubleValidator(0.01, 9999.99, 2, this);
    dacHighpassFilterValidator->setNotation(QDoubleValidator::StandardNotation);
    dacHighpassFilterValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    dacHighpassFilterLineEdit->setValidator(dacHighpassFilterValidator);
    connect(dacHighpassFilterLineEdit, SIGNAL(editingFinished()), this, SLOT(setDacHighpassFilterFrequency()));

    dacLockToSelectedCheckBox = new QCheckBox(tr("Lock to Selected"), this);
    connect(dacLockToSelectedCheckBox, SIGNAL(clicked(bool)), this, SLOT(dac1LockToSelected(bool)));
    dac1Label = new QLabel(tr("off"), this);
    dac2Label = new QLabel(tr("off"), this);
    dac1SetButton = new QPushButton(tr("Set to Selected"), this);
    dac2SetButton = new QPushButton(tr("Set to Selected"), this);
    dac1DisableButton = new QPushButton(tr("Disable"), this);
    dac1DisableButton->setFixedWidth(dac1DisableButton->fontMetrics().horizontalAdvance(dac1DisableButton->text()) + 14);
    dac2DisableButton = new QPushButton(tr("Disable"), this);
    dac2DisableButton->setFixedWidth(dac2DisableButton->fontMetrics().horizontalAdvance(dac2DisableButton->text()) + 14);
    openDacConfigButton = new QPushButton(tr("Advanced Configuration"), this);
    connect(openDacConfigButton, SIGNAL(clicked()), this, SLOT(openDacConfigDialog()));

    connect(dac1SetButton, SIGNAL(clicked()), this, SLOT(dac1SetChannel()));
    connect(dac2SetButton, SIGNAL(clicked()), this, SLOT(dac2SetChannel()));
    connect(dac1DisableButton, SIGNAL(clicked()), this, SLOT(dac1Disable()));
    connect(dac2DisableButton, SIGNAL(clicked()), this, SLOT(dac2Disable()));

    audioEnabledCheckBox = new QCheckBox(tr("Enable"));
    connect(audioEnabledCheckBox, SIGNAL(clicked(bool)), this, SLOT(enableAudio(bool)));

    audioFilterComboBox = new QComboBox(this);
    state->audioFilter->setupComboBox(audioFilterComboBox);
    connect(audioFilterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeAudioFilter(int)));

    audioChannelLabel = new QLabel(tr(""), this);

    audioEnabledCheckBox->setFixedWidth(audioEnabledCheckBox->minimumSizeHint().width());

    audioVolumeLabel = new QLabel(tr("Volume"));
    audioVolumeLabel->setAlignment(Qt::AlignRight);
    audioVolumeSlider = new QSlider(Qt::Horizontal);
    audioVolumeSlider->setMinimum(0);
    audioVolumeSlider->setMaximum(100);
    audioVolumeSlider->setValue(state->audioVolume->getValue());
    audioVolumeSlider->setStyleSheet("height: 16px");
    connect(audioVolumeSlider, SIGNAL(valueChanged(int)), this, SLOT(changeVolume(int)));
    audioVolumeValueLabel = new QLabel(QString::number(audioVolumeSlider->value()));

    audioThresholdLabel = new QLabel(tr("Noise Slicer"));
    audioThresholdLabel->setAlignment(Qt::AlignRight);
    audioThresholdSlider = new QSlider(Qt::Horizontal);
    audioThresholdSlider->setMinimum(0);
    audioThresholdSlider->setMaximum(200);
    audioThresholdSlider->setValue(state->audioThreshold->getValue());
    audioThresholdSlider->setStyleSheet("height: 16px");
    connect(audioThresholdSlider, SIGNAL(valueChanged(int)), this, SLOT(changeThreshold(int)));
    audioThresholdValueLabel = new QLabel(PlusMinusSymbol + QString::number(audioThresholdSlider->value()) +
                                          " " + MicroVoltsSymbol);

    audioVolumeLabel->setFixedWidth(fontMetrics().horizontalAdvance(audioVolumeLabel->text()) + 8);
    audioThresholdLabel->setFixedWidth(fontMetrics().horizontalAdvance(audioThresholdLabel->text()));

    audioFilterComboBox->setFixedWidth(audioFilterComboBox->minimumSizeHint().width());
    audioChannelLabel->setFixedWidth(audioEnabledCheckBox->width() + audioFilterComboBox->width() +
                                     audioVolumeLabel->width() - audioThresholdLabel->width() + 6);

    audioVolumeSlider->setFixedWidth(60);
    audioThresholdSlider->setFixedWidth(60);

    audioVolumeValueLabel->setFixedWidth(fontMetrics().horizontalAdvance(PlusMinusSymbol + "200 " + MicroVoltsSymbol));
    audioThresholdValueLabel->setFixedWidth(fontMetrics().horizontalAdvance(PlusMinusSymbol + "200 " + MicroVoltsSymbol));

    if (state->playback->getValue()) {
        dacGainSlider->setEnabled(false);
        dacNoiseSuppressSlider->setEnabled(false);
        dacHighpassFilterCheckBox->setEnabled(false);
        dacLockToSelectedCheckBox->setEnabled(false);
        dacHighpassFilterLineEdit->setEnabled(false);
        dac1SetButton->setEnabled(false);
        dac2SetButton->setEnabled(false);
        dac1DisableButton->setEnabled(false);
        dac2DisableButton->setEnabled(false);
        openDacConfigButton->setEnabled(false);
    }

    QHBoxLayout *dacGainLayout = new QHBoxLayout;
    dacGainLayout->addWidget(dacGainPreLabel);
    dacGainLayout->addWidget(dacGainSlider);
    dacGainLayout->addWidget(dacGainLabel);
    dacGainLayout->addStretch(1);

    QHBoxLayout *dacNoiseSuppressLayout = new QHBoxLayout;
    dacNoiseSuppressLayout->addWidget(dacNoiseSuppressPreLabel);
    dacNoiseSuppressLayout->addWidget(dacNoiseSuppressSlider);
    dacNoiseSuppressLayout->addWidget(dacNoiseSuppressLabel);
    dacNoiseSuppressLayout->addStretch(1);

    QHBoxLayout *dacHighpassFilterLayout = new QHBoxLayout;
    dacHighpassFilterLayout->addWidget(dacHighpassFilterCheckBox);
    dacHighpassFilterLayout->addWidget(dacHighpassFilterLineEdit);
    dacHighpassFilterLayout->addWidget(new QLabel(tr("Hz"), this));
    dacHighpassFilterLayout->addStretch(1);

    QHBoxLayout *dacControlLayout1 = new QHBoxLayout;
    dacControlLayout1->addWidget(new QLabel(tr("ANALOG OUT 1 (Audio L):"), this));
    dacControlLayout1->addWidget(dac1Label);
    dacControlLayout1->addStretch(1);

    QHBoxLayout *dacControlLayout2 = new QHBoxLayout;
    dacControlLayout2->addWidget(dac1SetButton);
    dacControlLayout2->addWidget(dac1DisableButton);
    dacControlLayout2->addWidget(dacLockToSelectedCheckBox);
    dacControlLayout2->addStretch(1);

    QHBoxLayout *dacControlLayout3 = new QHBoxLayout;
    dacControlLayout3->addWidget(new QLabel(tr("ANALOG OUT 2 (Audio R):"), this));
    dacControlLayout3->addWidget(dac2Label);
    dacControlLayout3->addStretch(1);

    QHBoxLayout *dacControlLayout4 = new QHBoxLayout;
    dacControlLayout4->addWidget(dac2SetButton);
    dacControlLayout4->addWidget(dac2DisableButton);
    dacControlLayout4->addStretch(1);

    QHBoxLayout *dacConfigLayout = new QHBoxLayout;
    dacConfigLayout->addStretch(1);
    dacConfigLayout->addWidget(openDacConfigButton);
    dacConfigLayout->addStretch(1);

    QVBoxLayout* dac1Layout = new QVBoxLayout;
    dac1Layout->addLayout(dacControlLayout1);
    dac1Layout->addLayout(dacControlLayout2);

    QVBoxLayout* dac2Layout = new QVBoxLayout;
    dac2Layout->addLayout(dacControlLayout3);
    dac2Layout->addLayout(dacControlLayout4);

    QFrame* frame1 = new QFrame(this);
    frame1->setFrameShape(QFrame::HLine);
    QFrame* frame2 = new QFrame(this);
    frame2->setFrameShape(QFrame::HLine);
    QFrame* frame3 = new QFrame(this);
    frame3->setFrameShape(QFrame::HLine);

    QVBoxLayout *dacMainLayout = new QVBoxLayout;
    dacMainLayout->addLayout(dacGainLayout);
    dacMainLayout->addLayout(dacNoiseSuppressLayout);
    dacMainLayout->addLayout(dacHighpassFilterLayout);
    dacMainLayout->addWidget(frame1);
    dacMainLayout->addLayout(dac1Layout);
    dacMainLayout->addWidget(frame2);
    dacMainLayout->addLayout(dac2Layout);
    dacMainLayout->addWidget(frame3);
    dacMainLayout->addLayout(dacConfigLayout);
    dacMainLayout->addStretch(1);

    QGroupBox *hardwareGroupBox = new QGroupBox(tr("Hardware Analog Out/Audio"));
    hardwareGroupBox->setLayout(dacMainLayout);

    QHBoxLayout *audioLayout1 = new QHBoxLayout;
    audioLayout1->addWidget(audioEnabledCheckBox);
    audioLayout1->addWidget(audioFilterComboBox);
    audioLayout1->addWidget(audioVolumeLabel);
    audioLayout1->addWidget(audioVolumeSlider);
    audioLayout1->addWidget(audioVolumeValueLabel);
    audioLayout1->addStretch(1);

    QHBoxLayout *audioLayout2 = new QHBoxLayout;
    audioLayout2->addWidget(audioChannelLabel);
    audioLayout2->addWidget(audioThresholdLabel);
    audioLayout2->addWidget(audioThresholdSlider);
    audioLayout2->addWidget(audioThresholdValueLabel);
    audioLayout2->addStretch(2);

    QVBoxLayout *audioLayout = new QVBoxLayout;
    audioLayout->addLayout(audioLayout1);
    audioLayout->addLayout(audioLayout2);
    audioLayout->addStretch(1);

    QGroupBox *pcAudioGroupBox = new QGroupBox(tr("PC Audio"));
    pcAudioGroupBox->setLayout(audioLayout);

    QVBoxLayout *analogOutAudioMainLayout = new QVBoxLayout;
    analogOutAudioMainLayout->addWidget(pcAudioGroupBox);
    analogOutAudioMainLayout->addWidget(hardwareGroupBox);
    analogOutAudioMainLayout->addStretch(1);
    setLayout(analogOutAudioMainLayout);
}

ControlPanelAudioAnalogTab::~ControlPanelAudioAnalogTab()
{
    if (analogOutConfigDialog) delete analogOutConfigDialog;
}

void ControlPanelAudioAnalogTab::updateFromState()
{
    audioEnabledCheckBox->setChecked(state->audioEnabled->getValue());

    if (state->audioVolume->getValue() != audioVolumeSlider->value()) {
        audioVolumeSlider->setValue(state->audioVolume->getValue());
    }
    if (state->audioThreshold->getValue() != audioThresholdSlider->value()) {
        audioThresholdSlider->setValue(state->audioThreshold->getValue());
    }
    audioChannelLabel->setText(controllerInterface->getCurrentAudioChannel());
    audioFilterComboBox->setCurrentIndex(state->audioFilter->getIndex());

    int gainIndex = state->analogOutGainIndex->getValue();
    if (gainIndex != gainIndexOld) {
        gainIndexOld = gainIndex;
        double gainFactor = (state->getControllerTypeEnum() == ControllerRecordUSB2) ? 515.0 : 1600.0;
        double trueGain = gainFactor * pow(2.0, gainIndex);
        dacGainLabel->setText(QString::number(trueGain  / 1000.0) + " mV/" + MicroVoltsSymbol);
        controllerInterface->setDacGain(gainIndex);
        dacGainSlider->setValue(gainIndex);
    }

    int noiseSuppressIndex = state->analogOutNoiseSlicerIndex->getValue();
    if (noiseSuppressIndex != noiseSuppressIndexOld) {
        dacNoiseSuppressLabel->setText(PlusMinusSymbol + QString::number(3.12 * noiseSuppressIndex, 'f', 0) +
                                       " " + MicroVoltsSymbol);
        controllerInterface->setAudioNoiseSuppress(noiseSuppressIndex);
        dacNoiseSuppressSlider->setValue(noiseSuppressIndex);
    }

    bool highpassEnabled = state->analogOutHighpassFilterEnabled->getValue();
    if (highpassEnabled != analogOutHighpassFilterEnabledOld) {
        analogOutHighpassFilterEnabledOld = highpassEnabled;
        dacHighpassFilterCheckBox->setChecked(highpassEnabled);
        controllerInterface->setDacHighpassFilterEnabled(highpassEnabled);
    }

    double highpassFreq = state->analogOutHighpassFilterFrequency->getValue();
    if (highpassFreq != analogOutHighpassFilterFrequencyOld) {
        analogOutHighpassFilterFrequencyOld = highpassFreq;
        if (dacHighpassFilterLineEdit->text().toDouble() != highpassFreq) {
            dacHighpassFilterLineEdit->setText(QString::number(highpassFreq));
        }
        controllerInterface->setDacHighpassFilterFrequency(highpassFreq);
    }

    dacLockToSelectedCheckBox->setChecked(state->analogOut1LockToSelected->getValue());

    QString analogChannel;
    if (state->analogOut1LockToSelected->getValue()) {
        analogChannel = state->signalSources->singleSelectedAmplifierChannelName();
        if (!analogChannel.isEmpty()) {
            state->analogOut1Channel->setValue(analogChannel);
        }
    }

    analogChannel = state->analogOut1Channel->getValueString();
    if (analogChannel != analogOut1ChannelOld) {
        analogOut1ChannelOld = analogChannel;
        dac1Label->setText(analogChannel);
        controllerInterface->setDacChannel(0, analogChannel);
    }
    analogChannel = state->analogOut2Channel->getValueString();
    if (analogChannel != analogOut2ChannelOld) {
        analogOut2ChannelOld = analogChannel;
        dac2Label->setText(analogChannel);
        controllerInterface->setDacChannel(1, analogChannel);
    }
    analogChannel = state->analogOut3Channel->getValueString();
    if (analogChannel != analogOut3ChannelOld) {
        analogOut3ChannelOld = analogChannel;
        controllerInterface->setDacChannel(2, analogChannel);
    }
    analogChannel = state->analogOut4Channel->getValueString();
    if (analogChannel != analogOut4ChannelOld) {
        analogOut4ChannelOld = analogChannel;
        controllerInterface->setDacChannel(3, analogChannel);
    }
    analogChannel = state->analogOut5Channel->getValueString();
    if (analogChannel != analogOut5ChannelOld) {
        analogOut5ChannelOld = analogChannel;
        controllerInterface->setDacChannel(4, analogChannel);
    }
    analogChannel = state->analogOut6Channel->getValueString();
    if (analogChannel != analogOut6ChannelOld) {
        analogOut6ChannelOld = analogChannel;
        controllerInterface->setDacChannel(5, analogChannel);
    }
    analogChannel = state->analogOut7Channel->getValueString();
    if (analogChannel != analogOut7ChannelOld) {
        analogOut7ChannelOld = analogChannel;
        controllerInterface->setDacChannel(6, analogChannel);
    }
    analogChannel = state->analogOut8Channel->getValueString();
    if (analogChannel != analogOut8ChannelOld) {
        analogOut8ChannelOld = analogChannel;
        controllerInterface->setDacChannel(7, analogChannel);
    }
    analogChannel = state->analogOutRefChannel->getValueString();
    if (analogChannel != analogRefChannelOld) {
        analogRefChannelOld = analogChannel;
        controllerInterface->setDacRefChannel(analogChannel);
    }

    int threshold;
    threshold = state->analogOut1Threshold->getValue();
    if (threshold != analogOut1ThresholdOld) {
        analogOut1ThresholdOld = threshold;
        controllerInterface->setDacThreshold(0, threshold);
    }
    threshold = state->analogOut2Threshold->getValue();
    if (threshold != analogOut2ThresholdOld) {
        analogOut2ThresholdOld = threshold;
        controllerInterface->setDacThreshold(1, threshold);
    }
    threshold = state->analogOut3Threshold->getValue();
    if (threshold != analogOut3ThresholdOld) {
        analogOut3ThresholdOld = threshold;
        controllerInterface->setDacThreshold(2, threshold);
    }
    threshold = state->analogOut4Threshold->getValue();
    if (threshold != analogOut4ThresholdOld) {
        analogOut4ThresholdOld = threshold;
        controllerInterface->setDacThreshold(3, threshold);
    }
    threshold = state->analogOut5Threshold->getValue();
    if (threshold != analogOut5ThresholdOld) {
        analogOut5ThresholdOld = threshold;
        controllerInterface->setDacThreshold(4, threshold);
    }
    threshold = state->analogOut6Threshold->getValue();
    if (threshold != analogOut6ThresholdOld) {
        analogOut6ThresholdOld = threshold;
        controllerInterface->setDacThreshold(5, threshold);
    }
    threshold = state->analogOut7Threshold->getValue();
    if (threshold != analogOut7ThresholdOld) {
        analogOut7ThresholdOld = threshold;
        controllerInterface->setDacThreshold(6, threshold);
    }
    threshold = state->analogOut8Threshold->getValue();
    if (threshold != analogOut8ThresholdOld) {
        analogOut8ThresholdOld = threshold;
        controllerInterface->setDacThreshold(7, threshold);
    }

    bool thresholdEnabled;
    thresholdEnabled = state->analogOut1ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut1ThresholdEnabledOld) {
        analogOut1ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut2ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut2ThresholdEnabledOld) {
        analogOut2ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut3ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut3ThresholdEnabledOld) {
        analogOut3ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut4ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut4ThresholdEnabledOld) {
        analogOut4ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut5ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut5ThresholdEnabledOld) {
        analogOut5ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut6ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut6ThresholdEnabledOld) {
        analogOut6ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut7ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut7ThresholdEnabledOld) {
        analogOut7ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }
    thresholdEnabled = state->analogOut8ThresholdEnabled->getValue();
    if (thresholdEnabled != analogOut8ThresholdEnabledOld) {
        analogOut8ThresholdEnabledOld = thresholdEnabled;
        setAllThresholdEnabledValues();
    }

    bool oneAmpSelected = (state->signalSources->numChannelsSelected() == 1) &&
            state->signalSources->onlyAmplifierChannelsSelected();

    bool nonPlayback = !(state->playback->getValue());
    dac1SetButton->setEnabled(oneAmpSelected && !dacLockToSelectedCheckBox->isChecked() && nonPlayback);
    dac2SetButton->setEnabled(oneAmpSelected && nonPlayback);

    const QString offString = "Off";
    dac1DisableButton->setEnabled((state->analogOut1Channel->getValueString() != offString) && nonPlayback);
    dac2DisableButton->setEnabled((state->analogOut2Channel->getValueString() != offString) && nonPlayback);

    if (analogOutConfigDialog) analogOutConfigDialog->updateFromState(oneAmpSelected);
}

void ControlPanelAudioAnalogTab::setAllThresholdEnabledValues()
{
    controllerInterface->setTtlOutMode(state->analogOut1ThresholdEnabled->getValue(),
                                       state->analogOut2ThresholdEnabled->getValue(),
                                       state->analogOut3ThresholdEnabled->getValue(),
                                       state->analogOut4ThresholdEnabled->getValue(),
                                       state->analogOut5ThresholdEnabled->getValue(),
                                       state->analogOut6ThresholdEnabled->getValue(),
                                       state->analogOut7ThresholdEnabled->getValue(),
                                       state->analogOut8ThresholdEnabled->getValue());
}

void ControlPanelAudioAnalogTab::enableAudio(bool enabled)
{
    state->audioEnabled->setValue(enabled);
    if (!enabled) audioChannelLabel->setText("");
}

void ControlPanelAudioAnalogTab::changeVolume(int volume)
{
    audioVolumeValueLabel->setText(QString::number(volume));
    state->audioVolume->setValue(volume);
}

void ControlPanelAudioAnalogTab::changeThreshold(int threshold)
{
    audioThresholdValueLabel->setText(PlusMinusSymbol + QString::number(threshold) + " " + MicroVoltsSymbol);
    state->audioThreshold->setValue(threshold);
}

void ControlPanelAudioAnalogTab::changeDacGain(int index)
{
    state->analogOutGainIndex->setValue(index);
}

void ControlPanelAudioAnalogTab::changeDacNoiseSuppress(int index)
{
    state->analogOutNoiseSlicerIndex->setValue(index);
}

void ControlPanelAudioAnalogTab::openDacConfigDialog()
{
    if (analogOutConfigDialog) {
        disconnect(this, nullptr, analogOutConfigDialog, nullptr);
        delete analogOutConfigDialog;
        analogOutConfigDialog = nullptr;
    }
    analogOutConfigDialog = new AnalogOutConfigDialog(state, controllerInterface, this);
    state->forceUpdate();
    analogOutConfigDialog->show();
    analogOutConfigDialog->raise();
    analogOutConfigDialog->activateWindow();
}

void ControlPanelAudioAnalogTab::dac1LockToSelected(bool enable)
{
    state->analogOut1LockToSelected->setValue(enable);
}

void ControlPanelAudioAnalogTab::enableDacHighpassFilter(bool enable)
{
    state->analogOutHighpassFilterEnabled->setValue(enable);
}

void ControlPanelAudioAnalogTab::setDacHighpassFilterFrequency()
{
    double frequency = dacHighpassFilterLineEdit->text().toDouble();
    state->analogOutHighpassFilterFrequency->setValueWithLimits(frequency);
}

void ControlPanelAudioAnalogTab::dac1SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut1Channel->setValue(channelName);
    }
}

void ControlPanelAudioAnalogTab::dac2SetChannel()
{
    QString channelName = state->signalSources->singleSelectedAmplifierChannelName();
    if (!channelName.isEmpty()) {
        state->analogOut2Channel->setValue(channelName);
    }
}

void ControlPanelAudioAnalogTab::dac1Disable()
{
    state->analogOut1Channel->setValue("Off");
}

void ControlPanelAudioAnalogTab::dac2Disable()
{
    state->analogOut2Channel->setValue("Off");
}
