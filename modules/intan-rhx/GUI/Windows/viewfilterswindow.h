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

#ifndef VIEWFILTERSWINDOW_H
#define VIEWFILTERSWINDOW_H

#include <QtWidgets>
#include <QMainWindow>
#include "filterplot.h"

class ViewFiltersWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ViewFiltersWindow(SystemState* state_, QWidget *parent = nullptr);
    ~ViewFiltersWindow();

private slots:
    void updateFromState();
    void toggleLogarithmic(bool log);
    void updateStatusBar(double frequency, double wideNormalizedGain, double lowNormalizedGain, double highNormalizedGain);
    void clearStatusBar();

private:
    SystemState* state;

    FilterPlot *filterPlot;

    QRadioButton *logarithmicButton;
    QRadioButton *linearButton;

    QLabel *hardwareLowCutoffLabel;
    QLabel *hardwareHighCutoffLabel;
    QLabel *hardwareDSPLabel;

    QLabel *softwareNotchLabel;
    QLabel *softwareLowPassLabel;
    QLabel *softwareHighPassLabel;

    QLabel *statusBarLabel;

    QDialogButtonBox *buttonBox;

    void updateFilterParameterText();
    QString freqAsString(double frequency, bool hz = true);  // Write given frequency as a string, in S/s, kS/s, Hz, or kHz
    QString ordinalNumberString(int number) const;
};

#endif // VIEWFILTERSWINDOW_H
