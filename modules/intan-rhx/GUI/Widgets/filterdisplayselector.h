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

#ifndef FILTERDISPLAYSELECTOR_H
#define FILTERDISPLAYSELECTOR_H

#include <QtWidgets>
#include <vector>
#include "systemstate.h"

using namespace std;

class FilterDisplaySelector : public QWidget
{
    Q_OBJECT
public:
    explicit FilterDisplaySelector(SystemState* state_, QWidget* parent = nullptr);

public slots:
    void updateFromState();

private slots:
    void enableOrder1(int checked);
    void enableOrder2(int checked);
    void enableOrder3(int checked);
    void enableOrder4(int checked);
    void filterOrderChanged();
    void arrangeByChanged(int index);
    void displayLabelTextChanged(int index);
    void changeLabelWidth(int index);
    void showDisabledChannels(int checked);

private:
    void boldSelectedFilters();

    SystemState* state;
    bool stimController;
    vector<QLabel*> filterLabels;

    QRadioButton* wide1Button;
    QRadioButton* wide2Button;
    QRadioButton* wide3Button;
    QRadioButton* wide4Button;

    QRadioButton* low1Button;
    QRadioButton* low2Button;
    QRadioButton* low3Button;
    QRadioButton* low4Button;

    QRadioButton* high1Button;
    QRadioButton* high2Button;
    QRadioButton* high3Button;
    QRadioButton* high4Button;

    QRadioButton* spike1Button;
    QRadioButton* spike2Button;
    QRadioButton* spike3Button;
    QRadioButton* spike4Button;

    QRadioButton* dc1Button;
    QRadioButton* dc2Button;
    QRadioButton* dc3Button;
    QRadioButton* dc4Button;

    QComboBox* arrangeByComboBox;
    QComboBox* labelTextComboBox;
    QComboBox* labelWidthComboBox;

    QButtonGroup* order1ButtonGroup;
    QButtonGroup* order2ButtonGroup;
    QButtonGroup* order3ButtonGroup;
    QButtonGroup* order4ButtonGroup;

    QCheckBox* order1CheckBox;
    QCheckBox* order2CheckBox;
    QCheckBox* order3CheckBox;
    QCheckBox* order4CheckBox;
    QCheckBox* clipWaveformsCheckBox;
    QCheckBox* showDisabledCheckBox;

    vector<QString> filterText;
};

#endif // FILTERDISPLAYSELECTOR_H
