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

#include <QtWidgets>
#include "abstractrhxcontroller.h"
#include "bandwidthdialog.h"

SimpleBandwidthDialog::SimpleBandwidthDialog(double lowerBandwidth, double upperBandwidth, double sampleRate,
                                             QWidget *parent) :
    QDialog(parent)
{
    nyquistFrequency = sampleRate / 2.0;

    QGroupBox *lowFreqGroupBox = new QGroupBox(tr("Low Frequency Bandwidth"), this);
    QGroupBox *highFreqGroupBox = new QGroupBox(tr("High Frequency Bandwidth"), this);

    QVBoxLayout *lowFreqLayout = new QVBoxLayout;
    QVBoxLayout *highFreqLayout = new QVBoxLayout;

    QHBoxLayout *lowFreqSelectLayout = new QHBoxLayout;
    QHBoxLayout *highFreqSelectLayout = new QHBoxLayout;

    lowRangeLabel = new QLabel(tr("Lower Bandwidth Range: 0.1 Hz to 500 Hz."), this);
    highRangeLabel = new QLabel(tr("Upper Bandwidth Range: 100 Hz to 20 kHz."), this);

    string sampleRateString =
            AbstractRHXController::getSampleRateString(AbstractRHXController::nearestSampleRate(sampleRate));
    nyquistWarningLabel = new QLabel(tr("Warning: Nyquist frequency violation (sample rate = ") +
                                     QString::fromStdString(sampleRateString) + ")", this);
    nyquistWarningLabel->setStyleSheet("color: red");

    lowFreqLineEdit = new QLineEdit(QString::number(lowerBandwidth, 'f', 2), this);
    lowFreqLineEdit->setFixedWidth(lowFreqLineEdit->fontMetrics().horizontalAdvance("500.000"));
    QDoubleValidator* lowFreqValidator = new QDoubleValidator(0.1, 500.0, 3, this);
    lowFreqValidator->setNotation(QDoubleValidator::StandardNotation);
    lowFreqValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    lowFreqLineEdit->setValidator(lowFreqValidator);
    connect(lowFreqLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onLineEditTextChanged()));

    highFreqLineEdit = new QLineEdit(QString::number(upperBandwidth, 'f', 0), this);
    highFreqLineEdit->setFixedWidth(highFreqLineEdit->fontMetrics().horizontalAdvance("20000.00"));
    QDoubleValidator* highFreqValidator = new QDoubleValidator(100.0, 20000.0, 0, this);
    highFreqValidator->setNotation(QDoubleValidator::StandardNotation);
    highFreqValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    highFreqLineEdit->setValidator(highFreqValidator);
    connect(highFreqLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onLineEditTextChanged()));

    lowFreqSelectLayout->addWidget(new QLabel(tr("Amplifier Lower Bandwidth"), this));
    lowFreqSelectLayout->addWidget(lowFreqLineEdit);
    lowFreqSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    lowFreqSelectLayout->addStretch();

    highFreqSelectLayout->addWidget(new QLabel(tr("Amplifier Upper Bandwidth"), this));
    highFreqSelectLayout->addWidget(highFreqLineEdit);
    highFreqSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    highFreqSelectLayout->addStretch();

    lowFreqLayout->addLayout(lowFreqSelectLayout);
    lowFreqLayout->addWidget(lowRangeLabel);

    highFreqLayout->addLayout(highFreqSelectLayout);
    highFreqLayout->addWidget(highRangeLabel);
    highFreqLayout->addWidget(nyquistWarningLabel);

    lowFreqGroupBox->setLayout(lowFreqLayout);
    highFreqGroupBox->setLayout(highFreqLayout);

    QLabel *label1 = new QLabel(tr("Headstage filter parameters will be set to the closest achievable values."),
                                this);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(lowFreqGroupBox);
    mainLayout->addWidget(highFreqGroupBox);
    mainLayout->addWidget(label1);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle(tr("Select Amplifier Bandwidth"));

    onLineEditTextChanged();
}

void SimpleBandwidthDialog::onLineEditTextChanged()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(lowFreqLineEdit->hasAcceptableInput() &&
                                                        highFreqLineEdit->hasAcceptableInput());

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

    nyquistWarningLabel->setVisible(highFreqLineEdit->text().toDouble() > nyquistFrequency);
}



AdvancedBandwidthDialog::AdvancedBandwidthDialog(double lowerBandwidth, double upperBandwidth, double dspCutoffFreq,
                                                 bool dspEnabled, double sampleRate, QWidget *parent) :
    QDialog(parent)
{
    nyquistFrequency = sampleRate / 2.0;

    QGroupBox *dspFreqGroupBox = new QGroupBox(tr("DSP Offset Removal Cutoff Frequency"), this);
    QGroupBox *lowFreqGroupBox = new QGroupBox(tr("Low Frequency Bandwidth"), this);
    QGroupBox *highFreqGroupBox = new QGroupBox(tr("High Frequency Bandwidth"), this);

    QVBoxLayout *dspFreqLayout = new QVBoxLayout;
    QVBoxLayout *lowFreqLayout = new QVBoxLayout;
    QVBoxLayout *highFreqLayout = new QVBoxLayout;

    QHBoxLayout *dspFreqSelectLayout = new QHBoxLayout;
    QHBoxLayout *lowFreqSelectLayout = new QHBoxLayout;
    QHBoxLayout *highFreqSelectLayout = new QHBoxLayout;
    QHBoxLayout *dspEnableLayout = new QHBoxLayout;

    dspEnableCheckBox = new QCheckBox(this);
    connect(dspEnableCheckBox, SIGNAL(toggled(bool)), this, SLOT(onDspCheckBoxChanged(bool)));

    QString dspRangeText(tr("Valid Range at "));
    dspRangeText.append(QString::number(sampleRate / 1000.0, 'f', 2));
    dspRangeText.append(" kHz sampling: ");
    dspRangeText.append(QString::number(0.000004857 * sampleRate, 'f', 3));
    dspRangeText.append(" Hz to ");
    dspRangeText.append(QString::number(0.1103 * sampleRate, 'f', 0));
    dspRangeText.append(" Hz.");

    dspRangeLabel = new QLabel(dspRangeText, this);
    lowRangeLabel = new QLabel(tr("Valid Range: 0.1 Hz to 500 Hz."), this);
    highRangeLabel = new QLabel(tr("Valid Range: 100 Hz to 20 kHz."), this);

    string sampleRateString =
            AbstractRHXController::getSampleRateString(AbstractRHXController::nearestSampleRate(sampleRate));
    nyquistWarningLabel = new QLabel(tr("Warning: Nyquist frequency violation (sample rate = ") +
                                     QString::fromStdString(sampleRateString) + ")", this);
    nyquistWarningLabel->setStyleSheet("color: red");

    dspFreqLineEdit = new QLineEdit(QString::number(dspCutoffFreq, 'f', 2), this);
    dspFreqLineEdit->setFixedWidth(dspFreqLineEdit->fontMetrics().horizontalAdvance("500.000"));
    QDoubleValidator* dspFreqValidator = new QDoubleValidator(0.001, 9999.999, 3, this);
    dspFreqValidator->setNotation(QDoubleValidator::StandardNotation);
    dspFreqValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    dspFreqLineEdit->setValidator(dspFreqValidator);
    connect(dspFreqLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onLineEditTextChanged()));

    lowFreqLineEdit = new QLineEdit(QString::number(lowerBandwidth, 'f', 2), this);
    lowFreqLineEdit->setFixedWidth(lowFreqLineEdit->fontMetrics().horizontalAdvance("500.000"));
    QDoubleValidator* lowFreqValidator = new QDoubleValidator(0.1, 500.0, 3, this);
    lowFreqValidator->setNotation(QDoubleValidator::StandardNotation);
    lowFreqValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    lowFreqLineEdit->setValidator(lowFreqValidator);
    connect(lowFreqLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onLineEditTextChanged()));

    highFreqLineEdit = new QLineEdit(QString::number(upperBandwidth, 'f', 0), this);
    highFreqLineEdit->setFixedWidth(highFreqLineEdit->fontMetrics().horizontalAdvance("20000.00"));
    QDoubleValidator* highFreqValidator = new QDoubleValidator(100.0, 20000.0, 0, this);
    highFreqValidator->setNotation(QDoubleValidator::StandardNotation);
    highFreqValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    highFreqLineEdit->setValidator(highFreqValidator);
    connect(highFreqLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onLineEditTextChanged()));

    dspFreqSelectLayout->addWidget(new QLabel(tr("DSP Cutoff Frequency"), this));
    dspFreqSelectLayout->addWidget(dspFreqLineEdit);
    dspFreqSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    dspFreqSelectLayout->addStretch();

    lowFreqSelectLayout->addWidget(new QLabel(tr("Analog Lower Cutoff"), this));
    lowFreqSelectLayout->addWidget(lowFreqLineEdit);
    lowFreqSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    lowFreqSelectLayout->addStretch();

    highFreqSelectLayout->addWidget(new QLabel(tr("Analog Upper Cutoff"), this));
    highFreqSelectLayout->addWidget(highFreqLineEdit);
    highFreqSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    highFreqSelectLayout->addStretch();

    dspEnableLayout->addWidget(dspEnableCheckBox);
    dspEnableLayout->addWidget(new QLabel(tr("Enable On-Chip DSP Offset Removal Filter"), this));
    dspEnableLayout->addStretch();

    lowFreqLayout->addLayout(lowFreqSelectLayout);
    lowFreqLayout->addWidget(lowRangeLabel);

    dspFreqLayout->addLayout(dspEnableLayout);
    dspFreqLayout->addLayout(dspFreqSelectLayout);
    dspFreqLayout->addWidget(dspRangeLabel);

    highFreqLayout->addLayout(highFreqSelectLayout);
    highFreqLayout->addWidget(highRangeLabel);
    highFreqLayout->addWidget(nyquistWarningLabel);

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
    setWindowTitle(tr("Select Amplifier Filter Parameters"));

    onLineEditTextChanged();
    dspEnableCheckBox->setChecked(dspEnabled);
    onDspCheckBoxChanged(dspEnabled);
}

void AdvancedBandwidthDialog::onLineEditTextChanged()
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

    nyquistWarningLabel->setVisible(highFreqLineEdit->text().toDouble() > nyquistFrequency);
}

void AdvancedBandwidthDialog::onDspCheckBoxChanged(bool enable)
{
    dspFreqLineEdit->setEnabled(enable);
    onLineEditTextChanged();
}
