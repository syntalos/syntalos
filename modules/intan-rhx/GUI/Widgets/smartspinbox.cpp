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

#include "smartspinbox.h"

#include <QtWidgets>

SmartSpinBox::SmartSpinBox(double step_, QWidget *parent) :
    QWidget(parent), step(step_)
{
    // Create new double spin box, and set its default units to microseconds.
    doubleSpinBox = new QDoubleSpinBox();
    doubleSpinBox->setSingleStep(step);

    // When the Smart Spin Box's value is changed, emit a signal and scale units.
    connect(this, SIGNAL(valueChanged(double)), this, SLOT(sendSignalValueMicro(double)));
    connect(this, SIGNAL(trueValueChanged(double)), this, SLOT(scaleUnits(double)));

    // When the Double Spin Box within Smart Spin Box's value is changed or editing has finished,
    // emit a complementary signal from the Smart Spin Box.
    connect(doubleSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(valueChanged(double)));
    connect(doubleSpinBox, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));

    // Set the Smart Spin Box to contain the Double Spin Box widget.
    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(doubleSpinBox);
    setLayout(mainLayout);
}

CurrentSpinBox::CurrentSpinBox(double step_, QWidget *parent) :
    SmartSpinBox(step_, parent)
{
    doubleSpinBox->setDecimals(2);
    doubleSpinBox->setSuffix(" " + MicroAmpsSymbol);
    doubleSpinBox->setMaximumWidth(doubleSpinBox->fontMetrics().horizontalAdvance("99999999  uA  +-"));
}

void CurrentSpinBox::setTrueMinimum(double min)
{
    if (getTrueValue() >= 1000) {  // If units are milliamps, set minimum in milliamps.
        doubleSpinBox->setMinimum(min/1000);
    } else if (getTrueValue() < 1000 && getTrueValue() >= 1) {  // If units are microamps, set minimum in microamps.
        doubleSpinBox->setMinimum(min);
    } else {  // If units are nanoamps, set minimum in nanoamps.
        doubleSpinBox->setMinimum(min * 1000);
    }
}

void CurrentSpinBox::loadValue(double val)
{
    // Block signals so that the initialization does not activate the constrain() functions.
    blockSignals(true);

    if (val == 0 && step >= 1) {  // If the step is in microamps, display the value to be loaded in microamps.
        doubleSpinBox->setMaximum(255 * step);
        doubleSpinBox->setSuffix(" " + MicroAmpsSymbol);
        doubleSpinBox->setDecimals(2);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setSingleStep(step);
    } else if (val < 1) {  // If the value to be loaded is less than 1 microamp, display the initial value in nanoamps.
        doubleSpinBox->setMaximum((255 * step) * 1000);
        doubleSpinBox->setSuffix(" nA");
        doubleSpinBox->setDecimals(0);
        doubleSpinBox->setValue(val * 1000);
        doubleSpinBox->setSingleStep(step * 1000);
    } else if (val >= 1000) {  // If the value to be loaded is 1 milliamp or greater, display the initial value in milliamps.
        doubleSpinBox->setMaximum((255 * step) / 1000);
        doubleSpinBox->setSuffix(" mA");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setValue(val / 1000);
        doubleSpinBox->setSingleStep(step / 1000);
    } else {  // If the value to be loaded is within the range of 1 to 1000 microamps, display the initial value in microamps.
        doubleSpinBox->setMaximum(255 * step);
        doubleSpinBox->setSuffix(" " + MicroAmpsSymbol);
        doubleSpinBox->setDecimals(2);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setSingleStep(step);
    }

    // Enable signals so that further user changes can active the constrain() functions.
    blockSignals(false);

    // Set the private member value_us to hold the time value of the Double Spin box in microseconds.
    value = val;
}

void CurrentSpinBox::roundValue()
{
    // Use modulo to find the difference between the current value and the closest multiple of step.
    // Due to errors in floating point modulo, conduct modulo in the integer domain, and then bring back to double.
    long valueint = (long) ((getTrueValue() * 1000) + 0.5);
    long currentstepint = (long) (round(step * 1000));
    int modint = valueint % currentstepint;
    double mod = modint / 1000.0;

    if (mod != 0.0) {  // If the modulo is not zero, the current value is not a multiple of current step.
        if (mod < step / 2.0) {  // If the modulo is less than half of the current step, the value should round down.
            if (getTrueValue() >= 1000.0) {
                // If the current units are milliamps, the difference should be scaled to milliamps as well.
                doubleSpinBox->setValue(doubleSpinBox->value() - mod / 1000.0);
            } else if (getTrueValue() < 1000.0 && getTrueValue() >= 1.0) {
                // If the current units are microamps, the difference doesn't need to be scaled.
                doubleSpinBox->setValue(doubleSpinBox->value() - mod);
            } else {
                // If the current units are nanoamps, the difference should be scaled to nanoamps as well.
                doubleSpinBox->setValue(doubleSpinBox->value() - mod * 1000.0);
            }
        } else {
            // If the modulo is greater than or equal to half of the current step, the value should round up.
            if (getTrueValue() >= 1000.0) {
                // If the current units are milliamps, the difference should be scaled to milliamps as well.
                doubleSpinBox->setValue(doubleSpinBox->value() + (step - mod) / 1000.0);
            } else if (getTrueValue() < 1000.0 && getTrueValue() >= 1.0) {
                // If the current units are microamps, the difference doesn't need to be scaled.
                doubleSpinBox->setValue(doubleSpinBox->value() + (step - mod));
            } else {
                // If the current units are nanoamps, the difference should be scaled to nanoamps as well.
                doubleSpinBox->setValue(doubleSpinBox->value() + (step - mod) * 1000.0);
            }
        }
    }
}

void CurrentSpinBox::scaleUnits(double val)
{
    // Block signals so that the scaling does not activate the constrain() functions.
    blockSignals(true);

    // Examine the value (in microamps) to determine the correct units, and examine the suffix to determine the
    // current units. When they don't match, change the range, suffix, displayed value, decimal precision,
    // and single step to reflect the correct units.

    // mA to uA
    if ((val < 1000.0) && (val >= 1.0) && (doubleSpinBox->suffix() == (" mA"))) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() * 1000.0);
        doubleSpinBox->setSuffix(" " + MicroAmpsSymbol);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setDecimals(2);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() * 1000.0);
    }

    // mA to nA
    if ((val < 1.0) && (doubleSpinBox->suffix() == " mA") && step < 0.1) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() * 1000000.0);
        doubleSpinBox->setSuffix(" nA");
        doubleSpinBox->setValue(val * 1000);
        doubleSpinBox->setDecimals(0);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() * 1000000.0);
    }

    // uA to mA
    if ((val >= 1000.0) && (doubleSpinBox->suffix() == (" " + MicroAmpsSymbol))) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() / 1000.0);
        doubleSpinBox->setSuffix(" mA");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setValue(val / 1000);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() / 1000.0);
    }

    // uA to nA
    if ((val < 1.0) && (doubleSpinBox->suffix() == (" " + MicroAmpsSymbol)) && step < 0.1) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() * 1000.0);
        doubleSpinBox->setSuffix(" nA");
        doubleSpinBox->setValue(val * 1000.0);
        doubleSpinBox->setDecimals(0);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() * 1000.0);
    }

    // nA to mA
    if ((val >= 1000.0) && (doubleSpinBox->suffix() == " nA")) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() / 1000000.0);
        doubleSpinBox->setSuffix(" mA");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setValue(val / 1000);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() / 1000000.0);
    }

    // nA to uA
    if ((val < 1000.0) && (val >= 1.0) && (doubleSpinBox->suffix() == (" nA"))) {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() / 1000.0);
        doubleSpinBox->setSuffix(" " + MicroAmpsSymbol);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setDecimals(2);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() / 1000.0);
    }

    // Enable signals so that further user changes can activate the constrain() functions.
    blockSignals(false);
}

void CurrentSpinBox::sendSignalValueMicro(double val)
{
    if (doubleSpinBox->suffix() == " nA") {  // If units are nA, value in uA is value in nA divided by 1000.
        value = val / 1000;
    } else if (doubleSpinBox->suffix() == " " + MicroAmpsSymbol) {  // If units are uA, units are consistent.
        value = val;
    } else if (doubleSpinBox->suffix() == " mA") {  // If units are mA, value in uA is value in mA multiplied by 1000.
        value = val * 1000;
    }

    // Emit a signal, with a parameter containing the value of the Double Spin Box in uA.
    emit trueValueChanged(value);
}


TimeSpinBox::TimeSpinBox(double step_, QWidget *parent) :
    SmartSpinBox(step_, parent)
{
    doubleSpinBox->setDecimals(1);
    doubleSpinBox->setSuffix(" " + MicroSecondsSymbol);
    doubleSpinBox->setMaximumWidth(doubleSpinBox->fontMetrics().horizontalAdvance("99999999  ms  +-"));
}

void TimeSpinBox::setTrueMinimum(double min)
{
    if (getTrueValue() >= 1000.0) {
        if (min >= 1000.0) {
            doubleSpinBox->setMinimum(min / 1000.0);
        } else {
            // Do nothing: the constrained box is in milliseconds, and the minimum box is in microseconds.
            // The constrained box will switch to milliseconds before the minimum is reached.
            doubleSpinBox->setMinimum(min / 1000.0);
        }
    } else if (getTrueValue() < 1000.0) {
        doubleSpinBox->setMinimum(min);
    }
}

void TimeSpinBox::loadValue(double val)
{
    // Block signals so that the initialization does not activate the constrain() functions.
    blockSignals(true);

    if (val >= 1000.0 && doubleSpinBox->suffix() != " ms") {
        // If the time value should be in ms, but it isn't, change the maximum, suffix, decimal precision, value, and
        // single step to reflect this.
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() / 1000.0);
        doubleSpinBox->setSuffix(" ms");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setValue(val / 1000.0);
        doubleSpinBox->setSingleStep(step / 1000.0);
    } else if (val >= 1000 && doubleSpinBox->suffix() == " ms") {
        // If the time value should be in ms, and it is, only change the value to reflect this.
        doubleSpinBox->setValue(val / 1000.0);
    }

    if (val < 1000 && doubleSpinBox->suffix() == " ms") {
        // If the time value should be in microseconds, but it isn't, change the suffix, decimal precision, value, and
        // single step to reflect this.
        doubleSpinBox->setSuffix(" " + MicroSecondsSymbol);
        doubleSpinBox->setDecimals(1);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setSingleStep(step);
    } else if (val < 1000.0 && doubleSpinBox->suffix() != " ms") {
        // If the time value should be in microseconds, and it is, only change the value to reflect this.
        doubleSpinBox->setValue(val);
    }

    // Enable signals so that further user changes can active the constrain() functions.
    blockSignals(false);

    // Set the private member value to hold the time value of the Double Spin box in microseconds.
    value = val;
}

void TimeSpinBox::roundValue()
{
    // Use modulo to find the difference between the current value and the closest multiple of step.
    // Due to errors in floating point modulo, coduct modulo in the integer domain, and then bring back to double.
    long valueint = (long) ((getTrueValue() * 1000) + 0.5);
    long timestepint = (long) (round(step * 1000));
    int modint = valueint % timestepint;
    double mod = modint / 1000.0;

    // If the modulo is not zero, the current value is not a multiple of time step.
    if (mod != 0.0) {
        // If the modulo is less than half of the time step, the value should round down.
        if (mod < step / 2.0) {
            if (getTrueValue() < 1000.0) {
                // If the current units are microseconds, the difference doesn't need to be scaled.
                doubleSpinBox->setValue(doubleSpinBox->value() - mod);
            } else {
                // If the current units are milliseconds, the difference should be scaled to milliseconds as well.
                doubleSpinBox->setValue(doubleSpinBox->value() - mod / 1000.0);
            }
        } else {  // If the modulo is greater than or equal to half of the time step, the value should round up.
            // If the current units are microseconds, the difference doesn't need to be scaled.
            if (getTrueValue() < 1000) {
                doubleSpinBox->setValue(doubleSpinBox->value() + (step - mod));
            } else {
            // If the current units are milliseconds, the difference should be scaled to milliseconds as well.
                doubleSpinBox->setValue(doubleSpinBox->value() + (step - mod) / 1000.0);
            }
        }
    }
}

void TimeSpinBox::scaleUnits(double val)
{
    // Store the original minimum of the Double Spin box so that it can be referenced with satisfactory precision.
    double storedMinimum = doubleSpinBox->minimum();

    // Block signals so that the scaling does not activate the constrain() functions.
    blockSignals(true);

    // Examine the value (in microseconds) to determine the correct units, and examine the suffix to determine the current units.
    // When they don't match, change the range, suffix, displayed value, decimal precision, and single step to reflect the
    // correct units.

    // us to ms
    if (val >= 1000 && doubleSpinBox->suffix() != " ms") {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() / 1000.0);
        doubleSpinBox->setMinimum(storedMinimum / 1000.0);
        doubleSpinBox->setSuffix(" ms");
        doubleSpinBox->setDecimals(3);
        doubleSpinBox->setValue(val / 1000.0);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() / 1000.0);
    }

    // ms to us
    if (val < 1000 && doubleSpinBox->suffix() == " ms") {
        doubleSpinBox->setMaximum(doubleSpinBox->maximum() * 1000.0);
        doubleSpinBox->setMinimum(storedMinimum * 1000.0);
        doubleSpinBox->setSuffix(" " + MicroSecondsSymbol);
        doubleSpinBox->setValue(val);
        doubleSpinBox->setDecimals(1);
        doubleSpinBox->setSingleStep(doubleSpinBox->singleStep() * 1000.0);
    }

    // Enable signals so that further user changes can activate the constrain() functions.
    blockSignals(false);
}

void TimeSpinBox::sendSignalValueMicro(double val)
{
    // If units are us, units are consistent.
    // If units are ms, value in us is value in ms multiplied by 1000.
    if (doubleSpinBox->suffix() != " ms")
        value = val;
    else
        value = val * 1000;

    // Emit a signal, with a parameter containing the value of the Double Spin Box in us.
    emit trueValueChanged(value);
}
