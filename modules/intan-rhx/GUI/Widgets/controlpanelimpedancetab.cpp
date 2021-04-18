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

#include <QSettings>
#include <iostream>
#include "impedancefreqdialog.h"
#include "controlpanelimpedancetab.h"

using namespace std;

ControlPanelImpedanceTab::ControlPanelImpedanceTab(ControllerInterface* controllerInterface_, SystemState* state_,
                                                   CommandParser* parser_, QWidget *parent) :
    QWidget(parent),
    state(state_),
    parser(parser_),
    controllerInterface(controllerInterface_),
    impedanceFreqSelectButton(nullptr),
    runImpedanceTestButton(nullptr),
    saveImpedancesButton(nullptr),
    desiredImpedanceFreqLabel(nullptr),
    actualImpedanceFreqLabel(nullptr)
{
    impedanceFreqSelectButton = new QPushButton(tr("Select Impedance Test Frequency"), this);

    desiredImpedanceFreqLabel = new QLabel(this);
    actualImpedanceFreqLabel = new QLabel(this);

    runImpedanceTestButton = new QPushButton(tr("Run Impedance Measurement"), this);
    saveImpedancesButton = new QPushButton(tr("Save Impedance Measurements in CSV Format"), this);

    QHBoxLayout* impedanceFreqSelectLayout = new QHBoxLayout;
    impedanceFreqSelectLayout->addWidget(impedanceFreqSelectButton);
    impedanceFreqSelectLayout->addStretch(1);

    QHBoxLayout* runImpedanceTestLayout = new QHBoxLayout;
    runImpedanceTestLayout->addWidget(runImpedanceTestButton);
    runImpedanceTestLayout->addStretch(1);

    QHBoxLayout* saveImpedancesLayout = new QHBoxLayout;
    saveImpedancesLayout->addWidget(saveImpedancesButton);
    saveImpedancesLayout->addStretch(1);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(impedanceFreqSelectLayout);
    mainLayout->addWidget(desiredImpedanceFreqLabel);
    mainLayout->addWidget(actualImpedanceFreqLabel);
    mainLayout->addLayout(runImpedanceTestLayout);
    mainLayout->addLayout(saveImpedancesLayout);
    mainLayout->addWidget(new QLabel(tr("(Impedance measurements are also saved with data)"), this));
    mainLayout->addStretch(1);

    connect(this, SIGNAL(sendExecuteCommand(QString)), parser, SLOT(executeCommandSlot(QString)));
    connect(this, SIGNAL(sendSetCommand(QString, QString)), parser, SLOT(setCommandSlot(QString, QString)));
    connect(impedanceFreqSelectButton, SIGNAL(clicked()), this, SLOT(changeImpedanceFrequency()));
    connect(runImpedanceTestButton, SIGNAL(clicked()), this, SLOT(runImpedanceTest()));
    connect(saveImpedancesButton, SIGNAL(clicked()), this, SLOT(saveImpedance()));

    setLayout(mainLayout);

    updateFromState();
}

void ControlPanelImpedanceTab::updateFromState()
{
    bool notRunning = !(state->running || state->sweeping);
    bool nonPlayback = !(state->playback->getValue());
    updateImpedanceFrequency();
    impedanceFreqSelectButton->setEnabled(notRunning && nonPlayback);
    runImpedanceTestButton->setEnabled(notRunning && nonPlayback && state->impedanceFreqValid->getValue());
    saveImpedancesButton->setEnabled(notRunning && state->impedancesHaveBeenMeasured->getValue());

    desiredImpedanceFreqLabel->setText(tr("Desired Impedance Test Frequency: ") +
                                       QString::number(state->desiredImpedanceFreq->getValue(), 'f', 0) + tr(" Hz"));
    actualImpedanceFreqLabel->setText(tr("Actual Impedance Test Frequency: ") +
                                      QString::number(state->actualImpedanceFreq->getValue(), 'f', 1) + tr(" Hz"));
}

void ControlPanelImpedanceTab::changeImpedanceFrequency()
{
    ImpedanceFreqDialog impedanceFreqDialog(state->desiredImpedanceFreq->getValue(), state->actualLowerBandwidth->getValue(), state->actualUpperBandwidth->getValue(),
                                            state->actualDspCutoffFreq->getValue(), state->dspEnabled->getValue(), state->sampleRate->getNumericValue(), this);

    if (impedanceFreqDialog.exec()) {
        emit sendSetCommand("DesiredImpedanceFreqHertz", QString::number(impedanceFreqDialog.impedanceFreqLineEdit->text().toDouble()));
    }
}

void ControlPanelImpedanceTab::runImpedanceTest()
{
    emit sendExecuteCommand("MeasureImpedance");
}

void ControlPanelImpedanceTab::saveImpedance()
{
    QSettings settings;
    QString defaultDirectory = settings.value("saveDirectory", ".").toString();
    QString impedanceFilename;
    impedanceFilename = QFileDialog::getSaveFileName(this, tr("Save Impedance Data As"),
                                                     defaultDirectory, tr("CSV (Comma delimited) (*.csv)"));

    if (impedanceFilename.isEmpty()) {
        return;
    } else {
        QFileInfo fileInfo(impedanceFilename);
        settings.setValue("saveDirectory", fileInfo.absolutePath());
        state->impedanceFilename->setBaseFilename(fileInfo.baseName());
        state->impedanceFilename->setPath(fileInfo.path());
        emit sendExecuteCommand("SaveImpedance");
    }
}

void ControlPanelImpedanceTab::updateImpedanceFrequency()
{
    int impedancePeriod;
    double lowerBandwidthLimit, upperBandwidthLimit;
    double rate = state->sampleRate->getNumericValue();

    upperBandwidthLimit = state->actualUpperBandwidth->getValue() / 1.5;
    lowerBandwidthLimit = state->actualLowerBandwidth->getValue() * 1.5;
    if (state->dspEnabled->getValue()) {
        if (state->actualDspCutoffFreq->getValue() > state->actualLowerBandwidth->getValue()) {
            lowerBandwidthLimit = state->actualDspCutoffFreq->getValue() * 1.5;
        }
    }

    if (state->desiredImpedanceFreq->getValue() > 0.0) {
        desiredImpedanceFreqLabel->setText(tr("Desired Impedance Test Frequency: ") +
                                           QString::number(state->desiredImpedanceFreq->getValue(), 'f', 0) + tr(" Hz"));
        impedancePeriod = qRound(rate / state->desiredImpedanceFreq->getValue());
        if (impedancePeriod >= 4 &&
                impedancePeriod <= 1024 &&
                state->desiredImpedanceFreq->getValue() >= lowerBandwidthLimit &&
                state->desiredImpedanceFreq->getValue() <= upperBandwidthLimit) {
            state->actualImpedanceFreq->setValueWithLimits(rate / impedancePeriod);
            state->impedanceFreqValid->setValue(true);
        } else {
            state->actualImpedanceFreq->setValue(0.0);
            state->impedanceFreqValid->setValue(false);
        }
    } else {
        desiredImpedanceFreqLabel->setText(tr("Desired Impedance Test Frequency: -"));
        state->actualImpedanceFreq->setValue(0.0);
        state->impedanceFreqValid->setValue(false);
    }
    if (state->impedanceFreqValid->getValue()) {
        actualImpedanceFreqLabel->setText(tr("Actual Impedance Test Frequency: ") + QString::number(state->actualImpedanceFreq->getValue(), 'f', 1) + tr(" Hz"));
    } else {
        actualImpedanceFreqLabel->setText(tr("Actual Impedance Test Frequency: -"));
    }
}
