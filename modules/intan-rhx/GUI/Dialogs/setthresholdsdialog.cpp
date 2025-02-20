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

#include "rhxglobals.h"
#include "setthresholdsdialog.h"

SetThresholdsDialog::SetThresholdsDialog(bool absoluteThreshold_, int threshold_, double rmsMultiple_,
                                         bool negativeRelativeThreshold_, QWidget *parent) :
    QDialog(parent)
{
    absoluteThresholdButton = new QRadioButton(tr("Absolute Threshold"), this);
    relativeThresholdButton = new QRadioButton(tr("Relative Threshold"), this);

    buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(absoluteThresholdButton);
    buttonGroup->addButton(relativeThresholdButton);
    absoluteThresholdButton->setChecked(absoluteThreshold_);
    relativeThresholdButton->setChecked(!absoluteThreshold_);

    connect(buttonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(updateThresholdType()));

    thresholdSpinBox = new QSpinBox(this);
    thresholdSpinBox->setSingleStep(5);
    thresholdSpinBox->setMinimum(-5000);
    thresholdSpinBox->setMaximum(5000);
    thresholdSpinBox->setValue(threshold_);
    thresholdSpinBox->setSuffix(" " + MicroVoltsSymbol);

    rmsSignComboBox = new QComboBox(this);
    rmsSignComboBox->addItem(tr("Positive"));
    rmsSignComboBox->addItem(tr("Negative"));
    rmsSignComboBox->setCurrentIndex(negativeRelativeThreshold_ ? 1 : 0);

    rmsMultipleSpinBox = new QDoubleSpinBox(this);
    rmsMultipleSpinBox->setMinimum(3.0);
    rmsMultipleSpinBox->setMaximum(20.0);
    rmsMultipleSpinBox->setSingleStep(0.5);
    rmsMultipleSpinBox->setDecimals(1);
    rmsMultipleSpinBox->setSuffix("x RMS");
    rmsMultipleSpinBox->setValue(rmsMultiple_);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QHBoxLayout *absoluteRow = new QHBoxLayout;
    absoluteRow->addWidget(thresholdSpinBox);
    absoluteRow->addStretch(1);

    QVBoxLayout *absoluteLayout = new QVBoxLayout;
    absoluteLayout->addWidget(absoluteThresholdButton);
    absoluteLayout->addWidget(new QLabel(tr("Set all spike detection thresholds to a specified level."), this));
    absoluteLayout->addLayout(absoluteRow);

    QHBoxLayout *relativeRow = new QHBoxLayout;
    relativeRow->addWidget(rmsSignComboBox);
    relativeRow->addWidget(rmsMultipleSpinBox);
    relativeRow->addStretch(1);

    QVBoxLayout *relativeLayout = new QVBoxLayout;
    relativeLayout->addWidget(relativeThresholdButton);
    relativeLayout->addWidget(new QLabel(tr("Set spike detection thresholds to a multiple of the RMS level on each channel."),
                                         this));
    relativeLayout->addLayout(relativeRow);

    QGroupBox *absoluteBox = new QGroupBox();
    absoluteBox->setLayout(absoluteLayout);

    QGroupBox *relativeBox = new QGroupBox();
    relativeBox->setLayout(relativeLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(absoluteBox);
    mainLayout->addWidget(relativeBox);
    mainLayout->addWidget(new QLabel(tr("Disabled channels will be unaffected.  This operation cannot be undone."), this));
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle(tr("Set Spike Detection Thresholds"));

    updateThresholdType();
}

bool SetThresholdsDialog::absoluteThreshold() const
{
    return absoluteThresholdButton->isChecked();
}

double SetThresholdsDialog::threshold() const
{
    return thresholdSpinBox->value();
}

double SetThresholdsDialog::rmsMultiple() const
{
    double rmsSign = 1.0;
    if (rmsSignComboBox->currentIndex() == 1) rmsSign = -1.0;

    return rmsSign * rmsMultipleSpinBox->value();
}

void SetThresholdsDialog::updateThresholdType()
{
    bool absolute = (buttonGroup->checkedButton() == absoluteThresholdButton);

    thresholdSpinBox->setEnabled(absolute);
    rmsSignComboBox->setEnabled(!absolute);
    rmsMultipleSpinBox->setEnabled(!absolute);
}
