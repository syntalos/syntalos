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
#include <QtWidgets>
#include "ampsettledialog.h"

// Amp settle dialog - this allows users to select desired values for amplifier settle parameters.
AmpSettleDialog::AmpSettleDialog(double lowerSettleBandwidth, bool useFastSettle, bool headstageGlobalSettle,
                                 QWidget *parent) :
    QDialog(parent)
{
    QVBoxLayout *lowFreqSettleLayout = new QVBoxLayout();
    QHBoxLayout *lowFreqSettleSelectLayout = new QHBoxLayout();

    settleHeadstageCheckBox = new QCheckBox(tr("Headstage Global Amp Settle"), this);
    settleHeadstageCheckBox->setChecked(headstageGlobalSettle);

    lowRangeSettleLabel = new QLabel(tr("Lower Bandwidth for Amp Settle Range: 0.1 Hz to 1000 Hz."), this);

    lowFreqSettleLineEdit = new QLineEdit(QString::number(lowerSettleBandwidth, 'f', 2), this);
    QDoubleValidator* lowFreqSettleValidator = new QDoubleValidator(0.1, 1000.0, 3, this);
    lowFreqSettleValidator->setNotation(QDoubleValidator::StandardNotation);
    lowFreqSettleValidator->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));  // Ensure '.' is used as decimal point, not ','
    lowFreqSettleLineEdit->setValidator(lowFreqSettleValidator);
    connect(lowFreqSettleLineEdit, SIGNAL(textChanged(QString)),
            this, SLOT(onLineEditTextChanged()));

    lowFreqSettleSelectLayout->addWidget(new QLabel(tr("Amplifier Lower Bandwidth for Amp Settle"), this));
    lowFreqSettleSelectLayout->addWidget(lowFreqSettleLineEdit);
    lowFreqSettleSelectLayout->addWidget(new QLabel(tr("Hz"), this));
    lowFreqSettleSelectLayout->addStretch();

    lowFreqSettleLayout->addLayout(lowFreqSettleSelectLayout);
    lowFreqSettleLayout->addWidget(lowRangeSettleLabel);

    // Amplifier settle mode combo box
    ampSettleModeComboBox = new QComboBox(this);
    ampSettleModeComboBox->addItem(tr("Switch Lower Bandwidth"));
    ampSettleModeComboBox->addItem(tr("Traditional Fast Settle"));
    ampSettleModeComboBox->setCurrentIndex(0);

    connect(ampSettleModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(setAmpSettleMode(int)));
    ampSettleModeComboBox->setCurrentIndex(useFastSettle ? 1 : 0);

    QHBoxLayout *ampSettleModeLayout = new QHBoxLayout;
    ampSettleModeLayout->addWidget(new QLabel(tr("Amp Settle Mode"), this));
    ampSettleModeLayout->addWidget(ampSettleModeComboBox);
    ampSettleModeLayout->addStretch();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(ampSettleModeLayout);
    mainLayout->addLayout(lowFreqSettleLayout);
    mainLayout->addWidget(settleHeadstageCheckBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Select Amplifier Settle Parameters"));

    onLineEditTextChanged();
}

// Check the validity of requested frequencies.
void AmpSettleDialog::onLineEditTextChanged()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                (lowFreqSettleLineEdit->hasAcceptableInput()));

    if (!lowFreqSettleLineEdit->hasAcceptableInput()) {
        lowRangeSettleLabel->setStyleSheet("color: red");
    } else {
        lowRangeSettleLabel->setStyleSheet("");
    }
}

// Change amp settle mode.
void AmpSettleDialog::setAmpSettleMode(int index)
{
    if (index == 1) {
        lowFreqSettleLineEdit->setEnabled(false);
    } else {
        lowFreqSettleLineEdit->setEnabled(true);
    }
}
