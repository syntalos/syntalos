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

#include "performanceoptimizationdialog.h"

#include <QtWidgets>

PerformanceOptimizationDialog::PerformanceOptimizationDialog(SystemState* state_, QWidget* parent) :
    QDialog(parent),
    state(state_)
{
    setWindowTitle(tr("Performance Optimization"));

    XPUSelectionComboBox = new QComboBox(this);

    if (state->cpuInfo.rank == 1) {
        XPUSelectionComboBox->addItem(state->cpuInfo.name + tr(" (recommended)"));
    } else {
        XPUSelectionComboBox->addItem(state->cpuInfo.name);
    }

    for (int gpu = 0; gpu < state->gpuList.size(); gpu++) {
        if (state->gpuList[gpu].rank == 1) {
            XPUSelectionComboBox->addItem(state->gpuList[gpu].name + tr(" (recommended)"));
        } else {
            XPUSelectionComboBox->addItem(state->gpuList[gpu].name);
        }
    }

    QHBoxLayout *XPUSelectionRow = new QHBoxLayout;
    XPUSelectionRow->addWidget(new QLabel(tr("Selected XPU:"), this));
    XPUSelectionRow->addWidget(XPUSelectionComboBox);

    QVBoxLayout *XPUGroupBoxLayout = new QVBoxLayout;
    XPUGroupBoxLayout->addWidget(new QLabel(tr("This software can use any connected XPU (CPU or GPU) to accelerate filtering\n"
                                               "and spike detection. Upon startup, a diganostic is run and the fastest XPU is\n"
                                               "detected to be used by default. However, the user can override this choice by\n"
                                               "selecting the XPU to use manually."), this));
    XPUGroupBoxLayout->addLayout(XPUSelectionRow);

    QGroupBox *XPUGroupBox = new QGroupBox(tr("XPU"), this);
    XPUGroupBox->setLayout(XPUGroupBoxLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(XPUGroupBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    initialize();
}

void PerformanceOptimizationDialog::initialize()
{
    // Find the currently used XPU and make that the selected entry in the combo box.
    int usedXPUIndex = state->usedXPUIndex();
    if (usedXPUIndex >= 0)
        XPUSelectionComboBox->setCurrentIndex(state->usedXPUIndex());
    else
        qDebug() << "Error: Invalid usedXPUIndex value";
}
