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
#include "voltagespinbox.h"

VoltageSpinBox::VoltageSpinBox(QWidget *parent) :
    QWidget(parent)
{
    voltagestep = 0.01;

    doubleSpinBox = new QDoubleSpinBox();
    doubleSpinBox->setSingleStep(voltagestep);
    doubleSpinBox->setDecimals(2);
    doubleSpinBox->setSuffix(" V");

    connect(doubleSpinBox, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(doubleSpinBox);
    setLayout(mainLayout);
}

void VoltageSpinBox::roundValue()
{
    int sign = (getValue() >= 0 ? 1 : -1);
    // Use modulo to find the difference between the current value and the closest multiple of voltage step
    // due to errors in floating point modulo, conduct modulo in the integer domain, and then bring back to double.
    long valueInt = (long) ((this->getValue() * 1000.0) + sign * 0.5);
    long voltagestepInt = (long) (voltagestep * 1000.0);
    int modint = (sign * valueInt) % voltagestepInt;
    double mod = modint / 1000.0;

    // If the modulo is not zero, the current value is not a multiple of voltage step.
    if (mod != 0) {
        // If the modulo is less than half of the voltage step, the value should round down.
        if (mod < voltagestep / 2.0) {
            doubleSpinBox->setValue(doubleSpinBox->value() - sign * mod);
        } else {
            doubleSpinBox->setValue(doubleSpinBox->value() + sign * (voltagestep - mod));
        }
    }
}
