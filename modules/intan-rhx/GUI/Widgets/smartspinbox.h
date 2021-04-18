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

#ifndef SMARTSPINBOX_H
#define SMARTSPINBOX_H

#include <QWidget>
#include <QDoubleSpinBox>
#include "rhxglobals.h"

class SmartSpinBox : public QWidget
{
    Q_OBJECT
public:
    explicit SmartSpinBox(double step_, QWidget *parent = nullptr);
    void setRange(double minimum, double maximum) { doubleSpinBox->setRange(minimum, maximum); }
    double getTrueValue() { return value; }
    virtual void setTrueMinimum(double min) = 0;
    QWidget* pointer() { return doubleSpinBox; }

public slots:
    void setValue(double val) { doubleSpinBox->setValue(val); }
    virtual void loadValue(double val) = 0;
    virtual void roundValue() = 0;

protected slots:
    virtual void scaleUnits(double val) = 0;
    virtual void sendSignalValueMicro(double val) = 0;

protected:
    QDoubleSpinBox *doubleSpinBox;
    double value;
    double step;

signals:
    void editingFinished();
    void valueChanged(double);
    void trueValueChanged(double);
};


class CurrentSpinBox : public SmartSpinBox
{
    Q_OBJECT
public:
    explicit CurrentSpinBox(double step_, QWidget *parent = nullptr);
    void setTrueMinimum(double min);

public slots:
    void loadValue(double val);
    void roundValue();

private slots:
    void scaleUnits(double val);
    void sendSignalValueMicro(double val);
};


class TimeSpinBox : public SmartSpinBox
{
    Q_OBJECT
public:
    explicit TimeSpinBox(double step_, QWidget *parent = nullptr);
    void setTrueMinimum(double min);

public slots:
    void loadValue(double val);
    void roundValue();

private slots:
    void scaleUnits(double val);
    void sendSignalValueMicro(double val);
};

#endif // SMARTSPINBOX_H
