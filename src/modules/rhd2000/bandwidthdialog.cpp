//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>

#include "qtincludes.h"

#include "bandwidthdialog.h"

// Bandwidth selection dialog.
// This dialog allows users to select desired values for upper and lower amplifier
// bandwidth, and DSP offset-removal cutoff frequency.  Frequency constraints
// are enforced.

BandwidthDialog::BandwidthDialog(double lowerBandwidth, double upperBandwidth,
                                 double dspCutoffFreq, bool dspEnabled, double sampleRate,
                                 QWidget *parent) :
    QDialog(parent)
{
    QGroupBox *dspFreqGroupBox = new QGroupBox(tr("DSP Offset Removal Cutoff Frequency"));
    QGroupBox *lowFreqGroupBox = new QGroupBox(tr("Low Frequency Bandwidth"));
    QGroupBox *highFreqGroupBox = new QGroupBox(tr("High Frequency Bandwidth"));

    QVBoxLayout *dspFreqLayout = new QVBoxLayout();
    QVBoxLayout *lowFreqLayout = new QVBoxLayout();
    QVBoxLayout *highFreqLayout = new QVBoxLayout();

    QHBoxLayout *dspFreqSelectLayout = new QHBoxLayout();
    QHBoxLayout *lowFreqSelectLayout = new QHBoxLayout();
    QHBoxLayout *highFreqSelectLayout = new QHBoxLayout();
    QHBoxLayout *dspEnableLayout = new QHBoxLayout();

    dspEnableCheckBox = new QCheckBox();
    connect(dspEnableCheckBox, SIGNAL(toggled(bool)),
            this, SLOT(onDspCheckBoxChanged(bool)));

    QString dspRangeText("DSP Cutoff Range at ");
    dspRangeText.append(QString::number(sampleRate / 1000.0, 'f', 2));
    dspRangeText.append(" kS/s: ");
    dspRangeText.append(QString::number(0.000004857 * sampleRate, 'f', 3));
    dspRangeText.append(" Hz to ");
    dspRangeText.append(QString::number(0.1103 * sampleRate, 'f', 0));
    dspRangeText.append(" Hz.");


    dspRangeLabel = new QLabel(dspRangeText);
    lowRangeLabel = new QLabel(tr("Lower Bandwidth Range: 0.1 Hz to 500 Hz."));
    highRangeLabel = new QLabel(tr("Upper Bandwidth Range: 100 Hz to 20 kHz."));

    dspFreqLineEdit = new QLineEdit(QString::number(dspCutoffFreq, 'f', 2));
    dspFreqLineEdit->setValidator(new QDoubleValidator(0.001, 9999.999, 3, this));
    connect(dspFreqLineEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(onLineEditTextChanged()));

    lowFreqLineEdit = new QLineEdit(QString::number(lowerBandwidth, 'f', 2));
    lowFreqLineEdit->setValidator(new QDoubleValidator(0.1, 500.0, 3, this));
    connect(lowFreqLineEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(onLineEditTextChanged()));

    highFreqLineEdit = new QLineEdit(QString::number(upperBandwidth, 'f', 0));
    highFreqLineEdit->setValidator(new QDoubleValidator(100.0, 20000.0, 0, this));
    connect(highFreqLineEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(onLineEditTextChanged()));

    dspFreqSelectLayout->addWidget(new QLabel(tr("DSP Cutoff Frequency")));
    dspFreqSelectLayout->addWidget(dspFreqLineEdit);
    dspFreqSelectLayout->addWidget(new QLabel(tr("Hz")));
    dspFreqSelectLayout->addStretch();

    lowFreqSelectLayout->addWidget(new QLabel(tr("Amplifier Lower Bandwidth")));
    lowFreqSelectLayout->addWidget(lowFreqLineEdit);
    lowFreqSelectLayout->addWidget(new QLabel(tr("Hz")));
    lowFreqSelectLayout->addStretch();

    highFreqSelectLayout->addWidget(new QLabel(tr("Amplifier Upper Bandwidth")));
    highFreqSelectLayout->addWidget(highFreqLineEdit);
    highFreqSelectLayout->addWidget(new QLabel(tr("Hz")));
    highFreqSelectLayout->addStretch();

    dspEnableLayout->addWidget(dspEnableCheckBox);
    dspEnableLayout->addWidget(new QLabel(tr("Enable On-Chip DSP Offset Removal Filter")));
    dspEnableLayout->addStretch();

    lowFreqLayout->addLayout(lowFreqSelectLayout);
    lowFreqLayout->addWidget(lowRangeLabel);

    dspFreqLayout->addLayout(dspEnableLayout);
    dspFreqLayout->addLayout(dspFreqSelectLayout);
    dspFreqLayout->addWidget(dspRangeLabel);

    highFreqLayout->addLayout(highFreqSelectLayout);
    highFreqLayout->addWidget(highRangeLabel);

    dspFreqGroupBox->setLayout(dspFreqLayout);
    lowFreqGroupBox->setLayout(lowFreqLayout);
    highFreqGroupBox->setLayout(highFreqLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(dspFreqGroupBox);
    mainLayout->addWidget(lowFreqGroupBox);
    mainLayout->addWidget(highFreqGroupBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Select Amplifier Bandwidth"));

    onLineEditTextChanged();
    dspEnableCheckBox->setChecked(dspEnabled);
    onDspCheckBoxChanged(dspEnabled);

}

// Check the validity of requested frequencies.
void BandwidthDialog::onLineEditTextChanged()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                (dspFreqLineEdit->hasAcceptableInput() || !dspEnableCheckBox->isChecked()) &&
                lowFreqLineEdit->hasAcceptableInput() &&
                highFreqLineEdit->hasAcceptableInput());

    if (!dspFreqLineEdit->hasAcceptableInput() && dspEnableCheckBox->isChecked()) {
        dspRangeLabel->setStyleSheet("color: red");
    } else {
        dspRangeLabel->setStyleSheet("");
    }

    if (!lowFreqLineEdit->hasAcceptableInput()) {
        lowRangeLabel->setStyleSheet("color: red");
    } else {
        lowRangeLabel->setStyleSheet("");
    }

    if (!highFreqLineEdit->hasAcceptableInput()) {
        highRangeLabel->setStyleSheet("color: red");
    } else {
        highRangeLabel->setStyleSheet("");
    }
}

void BandwidthDialog::onDspCheckBoxChanged(bool enable)
{
    dspFreqLineEdit->setEnabled(enable);
    onLineEditTextChanged();
}
