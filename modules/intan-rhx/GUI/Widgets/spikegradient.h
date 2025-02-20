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

#ifndef SPIKEGRADIENT_H
#define SPIKEGRADIENT_H

#include <QtWidgets>

#include "systemstate.h"
#include "rhxglobals.h"

class SpikeGradient : public QWidget
{
    Q_OBJECT
public:
    explicit SpikeGradient(SystemState* state_, QWidget *parent = nullptr);

    // Set up a QLinearGradient representing the portion of the black-white gradient that we want to represent time since last spike
    static QLinearGradient setupGradient();

    // Take a number of seconds and returns the corresponding color
    static QColor getColor(float seconds, float decayTime);

protected:
    void paintEvent(QPaintEvent *) override; // Initialize display for every paint event.
    void closeEvent(QCloseEvent *event) override; // Perform any clean-up here before application closes.

private:
    QSize minimumSizeHint() const override; // Minimum size of the gradient widget
    QSize sizeHint() const override; // Default suggested size of the page

    SystemState* state;
    QPixmap pixmap;
};

#endif // SPIKEGRADIENT_H
