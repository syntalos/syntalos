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

#include <vector>
#include <iostream>
#include "rhxregisters.h"
#include "bandwidthdialog.h"
#include "controlpanelbandwidthtab.h"

ControlPanelBandwidthTab::ControlPanelBandwidthTab(ControllerInterface* controllerInterface_, SystemState* state_,
                                                   QWidget *parent) :
    QWidget(parent),
    state(state_),
    controllerInterface(controllerInterface_),
    viewFiltersWindow(nullptr),
    bandwidthLabel(nullptr),
    changeBandwidthButton(nullptr),
    advancedBandwidthButton(nullptr),
    notchFilterComboBox(nullptr),
    lpTypeComboBox(nullptr),
    lpOrderLabel(nullptr),
    lpOrderSpinBox(nullptr),
    lpCutoffLineEdit(nullptr),
    hpTypeComboBox(nullptr),
    hpOrderLabel(nullptr),
    hpOrderSpinBox(nullptr),
    hpCutoffLineEdit(nullptr),
    viewFiltersButton(nullptr)
{
    changeBandwidthButton = new QPushButton(tr("Change Bandwidth"), this);
    advancedBandwidthButton = new QPushButton(tr("Advanced"), this);
    connect(changeBandwidthButton, SIGNAL(clicked()), this, SLOT(simpleBandwidthDialog()));
    connect(advancedBandwidthButton, SIGNAL(clicked()), this, SLOT(advancedBandwidthDialog()));

    bandwidthLabel = new QLabel("", this);

    QHBoxLayout *changeBandwidthLayout = new QHBoxLayout;
    changeBandwidthLayout->addWidget(changeBandwidthButton);
    changeBandwidthLayout->addWidget(advancedBandwidthButton);
    changeBandwidthLayout->addStretch(1);

    QVBoxLayout *bandwidthLayout = new QVBoxLayout;
    bandwidthLayout->addWidget(bandwidthLabel);
    bandwidthLayout->addLayout(changeBandwidthLayout);

    QGroupBox *bandwidthGroupBox = new QGroupBox(tr("Hardware Bandwidth"), this);
    bandwidthGroupBox->setLayout(bandwidthLayout);

    notchFilterComboBox = new QComboBox(this);
    state->notchFreq->setupComboBox(notchFilterComboBox);
    connect(notchFilterComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeNotchFilter(int)));

    QHBoxLayout *notchFilterRow = new QHBoxLayout;
    notchFilterRow->addWidget(new QLabel(tr("Notch Filter"), this));
    notchFilterRow->addWidget(notchFilterComboBox);
    notchFilterRow->addStretch(1);

    lpTypeComboBox = new QComboBox(this);
    state->lowType->setupComboBox(lpTypeComboBox);
    connect(lpTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeLowType(int)));

    lpOrderLabel = new QLabel(tr("Order"));

    lpOrderSpinBox = new QSpinBox(this);
    state->lowOrder->setupSpinBox(lpOrderSpinBox);
    connect(lpOrderSpinBox, SIGNAL(valueChanged(int)), this, SLOT(changeLowOrder(int)));

    QDoubleValidator *lpCutoffValidator = new QDoubleValidator(0.01, 20000.0, 2, this);
    lpCutoffValidator->setNotation(QDoubleValidator::StandardNotation);
    lpCutoffValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    QDoubleValidator *hpCutoffValidator = new QDoubleValidator(0.01, 20000.0, 2, this);
    hpCutoffValidator->setNotation(QDoubleValidator::StandardNotation);
    hpCutoffValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','

    lpCutoffLineEdit = new QLineEdit();
    lpCutoffLineEdit->setValidator(lpCutoffValidator);
    lpCutoffLineEdit->setMaxLength(10);
    int lineEditWidth = lpCutoffLineEdit->fontMetrics().horizontalAdvance("20000.0");
    lpCutoffLineEdit->setFixedWidth(lineEditWidth);
    connect(lpCutoffLineEdit, SIGNAL(editingFinished()), this, SLOT(changeLowCutoff()));

    QHBoxLayout *lowPassLayout = new QHBoxLayout;
    lowPassLayout->addWidget(lpTypeComboBox);
    lowPassLayout->addWidget(lpOrderLabel);
    lowPassLayout->addWidget(lpOrderSpinBox);
    lowPassLayout->addWidget(new QLabel(tr("Cutoff"), this));
    lowPassLayout->addWidget(lpCutoffLineEdit);
    lowPassLayout->addWidget(new QLabel(tr("Hz")));

    QGroupBox *lowPassGroupBox = new QGroupBox(tr("Low Pass Filter (LFP Band)"), this);
    lowPassGroupBox->setLayout(lowPassLayout);

    hpTypeComboBox = new QComboBox(this);
    state->highType->setupComboBox(hpTypeComboBox);
    connect(hpTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeHighType(int)));

    hpOrderLabel = new QLabel(tr("Order"));

    hpOrderSpinBox = new QSpinBox(this);
    state->highOrder->setupSpinBox(hpOrderSpinBox);
    connect(hpOrderSpinBox, SIGNAL(valueChanged(int)), this, SLOT(changeHighOrder(int)));

    hpCutoffLineEdit = new QLineEdit();
    hpCutoffLineEdit->setValidator(hpCutoffValidator);
    hpCutoffLineEdit->setMaxLength(10);
    hpCutoffLineEdit->setFixedWidth(lineEditWidth);
    connect(hpCutoffLineEdit, SIGNAL(editingFinished()), this, SLOT(changeHighCutoff()));

    QHBoxLayout *highPassLayout = new QHBoxLayout;
    highPassLayout->addWidget(hpTypeComboBox);
    highPassLayout->addWidget(hpOrderLabel);
    highPassLayout->addWidget(hpOrderSpinBox);
    highPassLayout->addWidget(new QLabel(tr("Cutoff"), this));
    highPassLayout->addWidget(hpCutoffLineEdit);
    highPassLayout->addWidget(new QLabel(tr("Hz")));

    QGroupBox *highPassGroupBox = new QGroupBox(tr("High Pass Filter (Spike Band)"), this);
    highPassGroupBox->setLayout(highPassLayout);

    viewFiltersButton = new QPushButton(tr("View Frequency Response"), this);
    connect(viewFiltersButton, SIGNAL(clicked(bool)), this, SLOT(viewFiltersSlot()));

    QHBoxLayout *viewFiltersRow = new QHBoxLayout;
    viewFiltersRow->addStretch(1);
    viewFiltersRow->addWidget(viewFiltersButton);
    viewFiltersRow->addStretch(1);

    QVBoxLayout *softwareFilterLayout = new QVBoxLayout;
    softwareFilterLayout->addLayout(notchFilterRow);
    softwareFilterLayout->addWidget(lowPassGroupBox);
    softwareFilterLayout->addWidget(highPassGroupBox);

    QGroupBox *softwareFilterGroupBox = new QGroupBox(tr("Software Filtering"), this);
    softwareFilterGroupBox->setLayout(softwareFilterLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(bandwidthGroupBox);
    mainLayout->addWidget(softwareFilterGroupBox);
    mainLayout->addLayout(viewFiltersRow);
    mainLayout->addStretch();
    setLayout(mainLayout);
}

ControlPanelBandwidthTab::~ControlPanelBandwidthTab()
{
    if (viewFiltersWindow) delete viewFiltersWindow;
}

void ControlPanelBandwidthTab::updateFromState()
{
    // Calculate actual lower -3dB point based on amplifier analog highpass filter (fL) and on-chip DSP highpass filter.
    state->actualLower3dBCutoff->setValueWithLimits(lower3dBPoint(state->actualLowerBandwidth->getValue(),
                                                                  state->actualDspCutoffFreq->getValue(),
                                                                  state->dspEnabled->getValue()));
    QString upperString;
    if (state->actualUpperBandwidth->getValue() >= 1000.0) {
        upperString = QString::number(state->actualUpperBandwidth->getValue() / 1000.0, 'f', 2) + tr(" kHz");
    } else {
        upperString = QString::number(state->actualUpperBandwidth->getValue(), 'f', 0) + tr(" Hz");
    }
    int precision = 2;
    if (state->actualLower3dBCutoff->getValue() >= 10.0) precision = 1;
    if (state->actualLower3dBCutoff->getValue() >= 100.0) precision = 0;
    bandwidthLabel->setText(tr("Amplifier Bandwidth: ") + QString::number(state->actualLower3dBCutoff->getValue(), 'f', precision) +
                            tr(" Hz - ") + upperString);

    bool notRunning = !(state->running || state->sweeping);
    bool nonPlayback = !(state->playback->getValue());
    changeBandwidthButton->setEnabled(notRunning && nonPlayback);
    advancedBandwidthButton->setEnabled(notRunning && nonPlayback);

    notchFilterComboBox->setCurrentIndex(state->notchFreq->getIndex());
    notchFilterComboBox->setEnabled(!state->recording);

    lpTypeComboBox->setCurrentIndex(state->lowType->getIndex());
    lpOrderSpinBox->setValue(state->lowOrder->getValue());
    lpCutoffLineEdit->setText(state->lowSWCutoffFreq->getValueString());

    if (state->recording) {
        lpTypeComboBox->setEnabled(false);
        lpOrderSpinBox->setEnabled(false);
        lpCutoffLineEdit->setEnabled(false);
    } else {
        lpTypeComboBox->setEnabled(true);
        lpOrderSpinBox->setEnabled(true);
        lpCutoffLineEdit->setEnabled(true);
    }

    hpTypeComboBox->setCurrentIndex(state->highType->getIndex());
    hpOrderSpinBox->setValue(state->highOrder->getValue());
    hpCutoffLineEdit->setText(state->highSWCutoffFreq->getValueString());

    if (state->recording) {
        hpTypeComboBox->setEnabled(false);
        hpOrderSpinBox->setEnabled(false);
        hpCutoffLineEdit->setEnabled(false);
    } else {
        hpTypeComboBox->setEnabled(true);
        hpOrderSpinBox->setEnabled(true);
        hpCutoffLineEdit->setEnabled(true);
    }
}

// If this is the first time the view filters window has been activated, create a view filters window. Activate the view filters window.
void ControlPanelBandwidthTab::viewFiltersSlot()
{
    if (viewFiltersWindow) delete viewFiltersWindow;

    viewFiltersWindow = new ViewFiltersWindow(state, this);
    // Add any 'connect' statements here.

    viewFiltersWindow->setWindowModality(Qt::NonModal);
    viewFiltersWindow->show();
    viewFiltersWindow->raise();
    viewFiltersWindow->activateWindow();
}

// This function calculates the effective lower bandwidth (i.e., the frequency where the midband gain drops by 3 dB)
// with the combined effects of the analog one-pole highpass filter built into the amplifier (hpf1Cutoff) and the
// optional on-chip DSP offset removal filter, which is another one-pole highpass filter (hpf2Cutoff) which may or
// may not be enabled (hpf2Enabled).
double ControlPanelBandwidthTab::lower3dBPoint(double hpf1Cutoff, double hpf2Cutoff, bool hpf2Enabled)
{
    if (!hpf2Enabled) return hpf1Cutoff;

    double f1Squared = hpf1Cutoff * hpf1Cutoff;
    double f2Squared = hpf2Cutoff * hpf2Cutoff;
    double term = sqrt(f1Squared * f1Squared + f2Squared * f2Squared + 6.0 * f1Squared * f2Squared);
    return sqrt((f1Squared + f2Squared + term) / 2.0);
}

// The lower bandwidth of each headstage is determined by two single-pole highpass filters: the analog lower bandwidth
// filter built into the amplifiers and the on-chip DSP offset removal filter.  Given the cutoff Fupdfrequency of either
// of these filters (hpf1Cutoff) and the desired effective lower bandwidth (i.e., the frequency where the midband gain
// drops by 3 dB), this function calculates the required cutoff frequency of the second filter.
double ControlPanelBandwidthTab::secondPoleLocation(double target3dBPoint, double hpf1Cutoff)
{
    if (hpf1Cutoff > target3dBPoint) return 0.0;  // Return zero if solution is impossible.
    double fTargetSquared = target3dBPoint * target3dBPoint;
    double f1Squared = hpf1Cutoff * hpf1Cutoff;
    return target3dBPoint * sqrt((fTargetSquared - f1Squared) / (fTargetSquared + f1Squared));
}

void ControlPanelBandwidthTab::simpleBandwidthDialog()
{
    SimpleBandwidthDialog bandwidthDialog(state->desiredLower3dBCutoff->getValue(), state->desiredUpperBandwidth->getValue(),
                                          state->sampleRate->getNumericValue(), this);
    if (bandwidthDialog.exec()) {
        state->desiredLower3dBCutoff->setValueWithLimits(bandwidthDialog.lowFreqLineEdit->text().toDouble());
        std::vector<double> dspCutoffFreq = RHXRegisters::getDspFreqTable(state->sampleRate->getNumericValue());
        state->desiredDspCutoffFreq->setValueWithLimits(dspCutoffFreq[15]);
        for (int i = 1; i < 16; ++i) {
            if (dspCutoffFreq[i] < state->desiredLower3dBCutoff->getValue()) {
                state->desiredDspCutoffFreq->setValueWithLimits(dspCutoffFreq[i]);
                break;
            }
        }
        state->dspEnabled->setValue(true);
        double fL = secondPoleLocation(state->desiredLower3dBCutoff->getValue(), state->desiredDspCutoffFreq->getValue());
        state->desiredLowerBandwidth->setValueWithLimits(fL);
        const double FreqRatioLimit = 6.0;
        if (state->desiredLowerBandwidth->getValue() < state->desiredDspCutoffFreq->getValue() / FreqRatioLimit) {
            state->desiredLowerBandwidth->setValueWithLimits(state->desiredDspCutoffFreq->getValue() / FreqRatioLimit);
        }

        state->desiredUpperBandwidth->setValueWithLimits(bandwidthDialog.highFreqLineEdit->text().toDouble());
        controllerInterface->updateChipCommandLists(false);

        // Recalculate with new 'actual' filter values.
        state->actualLower3dBCutoff->setValueWithLimits(lower3dBPoint(state->actualLowerBandwidth->getValue(),
                                                                      state->actualDspCutoffFreq->getValue(),
                                                                      state->dspEnabled->getValue()));
        state->forceUpdate();
    }
}

void ControlPanelBandwidthTab::advancedBandwidthDialog()
{
    AdvancedBandwidthDialog bandwidthDialog(state->desiredLowerBandwidth->getValue(), state->desiredUpperBandwidth->getValue(),
                                            state->desiredDspCutoffFreq->getValue(), state->dspEnabled->getValue(),
                                            state->sampleRate->getNumericValue(), this);
    if (bandwidthDialog.exec()) {
        state->desiredDspCutoffFreq->setValueWithLimits(bandwidthDialog.dspFreqLineEdit->text().toDouble());
        state->desiredLowerBandwidth->setValueWithLimits(bandwidthDialog.lowFreqLineEdit->text().toDouble());
        state->desiredUpperBandwidth->setValueWithLimits(bandwidthDialog.highFreqLineEdit->text().toDouble());
        state->dspEnabled->setValue(bandwidthDialog.dspEnableCheckBox->isChecked());
        controllerInterface->updateChipCommandLists(false);
    }
}
