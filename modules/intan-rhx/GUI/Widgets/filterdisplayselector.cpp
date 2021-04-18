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

#include <iostream>
#include "filterdisplayselector.h"

FilterDisplaySelector::FilterDisplaySelector(SystemState* state_, QWidget* parent) :
    QWidget(parent),
    state(state_),
    stimController(state->getControllerTypeEnum() == ControllerStimRecordUSB2)
{
    filterText.push_back("WIDE");
    filterText.push_back("LOW");
    filterText.push_back("HIGH");
    filterText.push_back("SPK");
    if (stimController) {
        filterText.push_back("DC");
    }

    filterLabels.resize(filterText.size());
    for (int i = 0; i < (int) filterLabels.size(); ++i) {
        filterLabels[i] = new QLabel(filterText[i], this);
    }

    wide1Button = new QRadioButton(this);
    wide2Button = new QRadioButton(this);
    wide3Button = new QRadioButton(this);
    wide4Button = new QRadioButton(this);

    low1Button = new QRadioButton(this);
    low2Button = new QRadioButton(this);
    low3Button = new QRadioButton(this);
    low4Button = new QRadioButton(this);

    high1Button = new QRadioButton(this);
    high2Button = new QRadioButton(this);
    high3Button = new QRadioButton(this);
    high4Button = new QRadioButton(this);

    spike1Button = new QRadioButton(this);
    spike2Button = new QRadioButton(this);
    spike3Button = new QRadioButton(this);
    spike4Button = new QRadioButton(this);

    if (stimController) {
        dc1Button = new QRadioButton(this);
        dc2Button = new QRadioButton(this);
        dc3Button = new QRadioButton(this);
        dc4Button = new QRadioButton(this);
    } else {
        dc1Button = nullptr;
        dc2Button = nullptr;
        dc3Button = nullptr;
        dc4Button = nullptr;
    }

    order1ButtonGroup = new QButtonGroup(this);
    order1ButtonGroup->addButton(wide1Button, 0);
    order1ButtonGroup->addButton(low1Button, 1);
    order1ButtonGroup->addButton(high1Button, 2);
    order1ButtonGroup->addButton(spike1Button, 3);
    if (stimController) {
        order1ButtonGroup->addButton(dc1Button, 4);
    }
    wide1Button->setChecked(true);

    order2ButtonGroup = new QButtonGroup(this);
    order2ButtonGroup->addButton(wide2Button, 0);
    order2ButtonGroup->addButton(low2Button, 1);
    order2ButtonGroup->addButton(high2Button, 2);
    order2ButtonGroup->addButton(spike2Button, 3);
    if (stimController) {
        order2ButtonGroup->addButton(dc2Button, 4);
    }
    low2Button->setChecked(true);

    order3ButtonGroup = new QButtonGroup(this);
    order3ButtonGroup->addButton(wide3Button, 0);
    order3ButtonGroup->addButton(low3Button, 1);
    order3ButtonGroup->addButton(high3Button, 2);
    order3ButtonGroup->addButton(spike3Button, 3);
    if (stimController) {
        order3ButtonGroup->addButton(dc3Button, 4);
    }
    high3Button->setChecked(true);

    order4ButtonGroup = new QButtonGroup(this);
    order4ButtonGroup->addButton(wide4Button, 0);
    order4ButtonGroup->addButton(low4Button, 1);
    order4ButtonGroup->addButton(high4Button, 2);
    order4ButtonGroup->addButton(spike4Button, 3);
    if (stimController) {
        order4ButtonGroup->addButton(dc4Button, 4);
    }
    spike4Button->setChecked(true);

    order1CheckBox = new QCheckBox(this);
    order2CheckBox = new QCheckBox(this);
    order3CheckBox = new QCheckBox(this);
    order4CheckBox = new QCheckBox(this);

    connect(order1CheckBox, SIGNAL(stateChanged(int)), this, SLOT(enableOrder1(int)));
    connect(order2CheckBox, SIGNAL(stateChanged(int)), this, SLOT(enableOrder2(int)));
    connect(order3CheckBox, SIGNAL(stateChanged(int)), this, SLOT(enableOrder3(int)));
    connect(order4CheckBox, SIGNAL(stateChanged(int)), this, SLOT(enableOrder4(int)));

    connect(order1CheckBox, SIGNAL(clicked(bool)), this, SLOT(filterOrderChanged()));
    connect(order2CheckBox, SIGNAL(clicked(bool)), this, SLOT(filterOrderChanged()));
    connect(order3CheckBox, SIGNAL(clicked(bool)), this, SLOT(filterOrderChanged()));
    connect(order4CheckBox, SIGNAL(clicked(bool)), this, SLOT(filterOrderChanged()));

    connect(order1ButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(filterOrderChanged()));
    connect(order2ButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(filterOrderChanged()));
    connect(order3ButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(filterOrderChanged()));
    connect(order4ButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(filterOrderChanged()));

    order1CheckBox->setChecked(true);
    order2CheckBox->setChecked(false);
    order3CheckBox->setChecked(false);
    order4CheckBox->setChecked(false);
    enableOrder1(Qt::Checked);
    enableOrder2(Qt::Unchecked);
    enableOrder3(Qt::Unchecked);
    enableOrder4(Qt::Unchecked);

    QGridLayout* grid = new QGridLayout;

    grid->addWidget(new QLabel("          ", this), 0, 0);  // Fixed spacing to prevent widget resizing when WIDE label goes from
                                                            // bold to non-bold.
    grid->addWidget(order1CheckBox, 0, 1, Qt::AlignCenter | Qt::AlignBottom);
    grid->addWidget(order2CheckBox, 0, 3, Qt::AlignCenter | Qt::AlignBottom);
    grid->addWidget(order3CheckBox, 0, 5, Qt::AlignCenter | Qt::AlignBottom);
    grid->addWidget(order4CheckBox, 0, 7, Qt::AlignCenter | Qt::AlignBottom);

    grid->addWidget(filterLabels[0], 1, 0, Qt::AlignRight | Qt::AlignCenter);
    grid->addWidget(wide1Button, 1, 1, Qt::AlignCenter);
    grid->addWidget(wide2Button, 1, 3, Qt::AlignCenter);
    grid->addWidget(wide3Button, 1, 5, Qt::AlignCenter);
    grid->addWidget(wide4Button, 1, 7, Qt::AlignCenter);

    grid->addWidget(filterLabels[1], 2, 0, Qt::AlignRight | Qt::AlignCenter);
    grid->addWidget(low1Button, 2, 1, Qt::AlignCenter);
    grid->addWidget(low2Button, 2, 3, Qt::AlignCenter);
    grid->addWidget(low3Button, 2, 5, Qt::AlignCenter);
    grid->addWidget(low4Button, 2, 7, Qt::AlignCenter);

    grid->addWidget(filterLabels[2], 3, 0, Qt::AlignRight | Qt::AlignCenter);
    grid->addWidget(high1Button, 3, 1, Qt::AlignCenter);
    grid->addWidget(high2Button, 3, 3, Qt::AlignCenter);
    grid->addWidget(high3Button, 3, 5, Qt::AlignCenter);
    grid->addWidget(high4Button, 3, 7, Qt::AlignCenter);

    grid->addWidget(filterLabels[3], 4, 0, Qt::AlignRight | Qt::AlignCenter);
    grid->addWidget(spike1Button, 4, 1, Qt::AlignCenter);
    grid->addWidget(spike2Button, 4, 3, Qt::AlignCenter);
    grid->addWidget(spike3Button, 4, 5, Qt::AlignCenter);
    grid->addWidget(spike4Button, 4, 7, Qt::AlignCenter);

    if (stimController) {
        grid->addWidget(filterLabels[4], 5, 0, Qt::AlignRight | Qt::AlignCenter);
        grid->addWidget(dc1Button, 5, 1, Qt::AlignCenter);
        grid->addWidget(dc2Button, 5, 3, Qt::AlignCenter);
        grid->addWidget(dc3Button, 5, 5, Qt::AlignCenter);
        grid->addWidget(dc4Button, 5, 7, Qt::AlignCenter);
    }

    QFrame* separator1 = new QFrame(this);
    QFrame* separator2 = new QFrame(this);
    QFrame* separator3 = new QFrame(this);

    separator1->setFrameShape(QFrame::VLine);
    separator2->setFrameShape(QFrame::VLine);
    separator3->setFrameShape(QFrame::VLine);

    int numColumns = stimController ? 6 : 5;
    grid->addWidget(separator1, 0, 2, numColumns, 1, Qt::AlignLeft);
    grid->addWidget(separator2, 0, 4, numColumns, 1, Qt::AlignLeft);
    grid->addWidget(separator3, 0, 6, numColumns, 1, Qt::AlignLeft);


    arrangeByComboBox = new QComboBox(this);
    state->arrangeBy->setupComboBox(arrangeByComboBox);
    connect(arrangeByComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(arrangeByChanged(int)));

    labelTextComboBox = new QComboBox(this);
    state->displayLabelText->setupComboBox(labelTextComboBox);
    connect(labelTextComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(displayLabelTextChanged(int)));

    labelWidthComboBox = new QComboBox(this);
    state->labelWidth->setupComboBox(labelWidthComboBox);
    connect(labelWidthComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeLabelWidth(int)));

    showDisabledCheckBox = new QCheckBox(tr("Show Disabled Channels"), this);
    showDisabledCheckBox->setChecked(true);
    connect(showDisabledCheckBox, SIGNAL(stateChanged(int)), this, SLOT(showDisabledChannels(int)));

    QVBoxLayout* vLayout = new QVBoxLayout;
    vLayout->addWidget(arrangeByComboBox);
    vLayout->addWidget(labelWidthComboBox);
    vLayout->addWidget(labelTextComboBox);
    vLayout->addWidget(showDisabledCheckBox);
    vLayout->addStretch(1);

    QFrame* separator4 = new QFrame(this);
    separator4->setFrameShape(QFrame::VLine);
    separator4->setLineWidth(2);

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addLayout(grid);
    hLayout->addWidget(separator4);
    hLayout->addLayout(vLayout);
    hLayout->addStretch(1);

    QGroupBox* groupBox = new QGroupBox("Filter Display Selector");
    groupBox->setLayout(hLayout);

    QHBoxLayout* mainLayout = new QHBoxLayout;
    mainLayout->addWidget(groupBox);
    mainLayout->addStretch(1);

    setLayout(mainLayout);

    boldSelectedFilters();
}

void FilterDisplaySelector::enableOrder1(int checked)
{
    bool enable = (checked == Qt::Checked);
    wide1Button->setEnabled(enable);
    high1Button->setEnabled(enable);
    low1Button->setEnabled(enable);
    spike1Button->setEnabled(enable);
    if (dc1Button) dc1Button->setEnabled(enable);
}

void FilterDisplaySelector::enableOrder2(int checked)
{
    bool enable = (checked == Qt::Checked);
    wide2Button->setEnabled(enable);
    high2Button->setEnabled(enable);
    low2Button->setEnabled(enable);
    spike2Button->setEnabled(enable);
    if (dc2Button) dc2Button->setEnabled(enable);
}

void FilterDisplaySelector::enableOrder3(int checked)
{
    bool enable = (checked == Qt::Checked);
    wide3Button->setEnabled(enable);
    high3Button->setEnabled(enable);
    low3Button->setEnabled(enable);
    spike3Button->setEnabled(enable);
    if (dc3Button) dc3Button->setEnabled(enable);
}

void FilterDisplaySelector::enableOrder4(int checked)
{
    bool enable = (checked == Qt::Checked);
    wide4Button->setEnabled(enable);
    high4Button->setEnabled(enable);
    low4Button->setEnabled(enable);
    spike4Button->setEnabled(enable);
    if (dc4Button) dc4Button->setEnabled(enable);
}

void FilterDisplaySelector::filterOrderChanged()
{
    boldSelectedFilters();

    state->holdUpdate();

    QString order1Filter = "None";
    if (order1CheckBox->isChecked()) {
        order1Filter = filterText[order1ButtonGroup->checkedId()].toLower();
    }
    state->filterDisplay1->setValue(order1Filter);

    QString order2Filter = "None";
    if (order2CheckBox->isChecked()) {
        order2Filter = filterText[order2ButtonGroup->checkedId()].toLower();
    }
    state->filterDisplay2->setValue(order2Filter);

    QString order3Filter = "None";
    if (order3CheckBox->isChecked()) {
        order3Filter = filterText[order3ButtonGroup->checkedId()].toLower();
    }
    state->filterDisplay3->setValue(order3Filter);

    QString order4Filter = "None";
    if (order4CheckBox->isChecked()) {
        order4Filter = filterText[order4ButtonGroup->checkedId()].toLower();
    }
    state->filterDisplay4->setValue(order4Filter);

    state->releaseUpdate();
}

void FilterDisplaySelector::arrangeByChanged(int index)
{
    state->arrangeBy->setIndex(index);
}

void FilterDisplaySelector::displayLabelTextChanged(int index)
{
    state->displayLabelText->setIndex(index);
}

void FilterDisplaySelector::changeLabelWidth(int index)
{
    state->labelWidth->setIndex(index);
}

void FilterDisplaySelector::showDisabledChannels(int checked)
{
    state->showDisabledChannels->setValue(checked != 0);
}

void FilterDisplaySelector::boldSelectedFilters()
{
    vector<bool> alreadySelected(filterText.size(), false);

    if (order1CheckBox->isChecked()) {
        int filterIndex = order1ButtonGroup->checkedId();
        alreadySelected[filterIndex] = true;
    }
    if (order2CheckBox->isChecked()) {
        int filterIndex = order2ButtonGroup->checkedId();
        if (!alreadySelected[filterIndex]) {
            alreadySelected[filterIndex] = true;
        }
    }
    if (order3CheckBox->isChecked()) {
        int filterIndex = order3ButtonGroup->checkedId();
        if (!alreadySelected[filterIndex]) {
            alreadySelected[filterIndex] = true;
        }
    }
    if (order4CheckBox->isChecked()) {
        int filterIndex = order4ButtonGroup->checkedId();
        if (!alreadySelected[filterIndex]) {
            alreadySelected[filterIndex] = true;
        }
    }

    // Highlight labels of selected filters.
    for (int i = 0; i < (int) alreadySelected.size(); ++i) {
        if (alreadySelected[i]) {
            filterLabels[i]->setText("<b>" + filterText[i] + "</b>");
        } else {
            filterLabels[i]->setText(filterText[i]);
        }
    }
}

void FilterDisplaySelector::updateFromState()
{
    // Order1Filter Widgets
    QString order1FilterStr = state->filterDisplay1->getValueString();
    order1CheckBox->setChecked(order1FilterStr != "None");
    int order1FilterInt = 0;
    for (int i = 0; i < (int) filterText.size(); ++i) {
        if (filterText[i].toLower() == order1FilterStr.toLower()) {
            order1FilterInt = i;
        }
    }
    order1ButtonGroup->button(order1FilterInt)->setChecked(true);

    // Order2Filter Widgets
    QString order2FilterStr = state->filterDisplay2->getValueString();
    order2CheckBox->setChecked(order2FilterStr != "None");
    int order2FilterInt = 1;
    for (int i = 0; i < (int) filterText.size(); ++i) {
        if (filterText[i].toLower() == order2FilterStr.toLower()) {
            order2FilterInt = i;
        }
    }
    order2ButtonGroup->button(order2FilterInt)->setChecked(true);

    // Order3Filter Widgets
    QString order3FilterStr = state->filterDisplay3->getValueString();
    order3CheckBox->setChecked(order3FilterStr != "None");
    int order3FilterInt = 2;
    for (int i = 0; i < (int) filterText.size(); ++i) {
        if (filterText[i].toLower() == order3FilterStr.toLower()) {
            order3FilterInt = i;
        }
    }
    order3ButtonGroup->button(order3FilterInt)->setChecked(true);

    // Order4Filter Widgets
    QString order4FilterStr = state->filterDisplay4->getValueString();
    order4CheckBox->setChecked(order4FilterStr != "None");
    int order4FilterInt = 3;
    for (int i = 0; i < (int) filterText.size(); ++i) {
        if (filterText[i].toLower() == order4FilterStr.toLower()) {
            order4FilterInt = i;
        }
    }
    order4ButtonGroup->button(order4FilterInt)->setChecked(true);

    // ComboBoxes
    arrangeByComboBox->setCurrentIndex(state->arrangeBy->getIndex());
    labelTextComboBox->setCurrentIndex(state->displayLabelText->getIndex());
    labelWidthComboBox->setCurrentIndex(state->labelWidth->getIndex());

    // CheckBoxes
    showDisabledCheckBox->setChecked(state->showDisabledChannels->getValue());
}
