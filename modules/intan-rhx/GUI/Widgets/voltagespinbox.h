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

#ifndef VOLTAGESPINBOX_H
#define VOLTAGESPINBOX_H

#include <QWidget>
#include <QDoubleSpinBox>

class QDoubleSpinBox;

class VoltageSpinBox : public QWidget
{
    Q_OBJECT
public:
    explicit VoltageSpinBox(QWidget *parent = nullptr);
    void setRange(double minimum, double maximum) { doubleSpinBox->setRange(minimum, maximum); }
    double getValue() { return doubleSpinBox->value(); }
    QWidget* pointer() {  return (QWidget*) doubleSpinBox; }

public slots:
    void setValue(double val) { doubleSpinBox->setValue(val); }
    void roundValue();

private:
    QDoubleSpinBox *doubleSpinBox;
    double voltagestep;

signals:
    void editingFinished();
    void valueChanged(double);
    void trueValueChanged(double);
};

#endif // VOLTAGESPINBOX_H
