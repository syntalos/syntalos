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

    writeLatencyComboBox = new QComboBox(this);
    state->writeToDiskLatency->setupComboBox(writeLatencyComboBox);

    plottingModeComboBox = new QComboBox(this);
    state->plottingMode->setupComboBox(plottingModeComboBox);

    QHBoxLayout *XPUSelectionRow = new QHBoxLayout;
    XPUSelectionRow->addWidget(new QLabel(tr("Selected XPU:"), this));
    XPUSelectionRow->addWidget(XPUSelectionComboBox);

    QHBoxLayout *writeLatencySelectionRow = new QHBoxLayout;
    writeLatencySelectionRow->addWidget(new QLabel(tr("Write Latency:"), this));
    writeLatencySelectionRow->addWidget(writeLatencyComboBox);

    QHBoxLayout *plottingModeRow = new QHBoxLayout;
    plottingModeRow->addWidget(new QLabel(tr("Plotting Mode:"), this));
    plottingModeRow->addWidget(plottingModeComboBox);

    QVBoxLayout *XPUGroupBoxLayout = new QVBoxLayout;
    XPUGroupBoxLayout->addWidget(new QLabel(tr(         "This software can use any connected XPU (CPU or GPU) to accelerate filtering\n"
                                                        "and spike detection. Upon startup, a diganostic is run and the fastest XPU is\n"
                                                        "detected to be used by default. However, the user can override this choice by\n"
                                                        "selecting the XPU to use manually."), this));
    XPUGroupBoxLayout->addLayout(XPUSelectionRow);

    QVBoxLayout *writeLatencyGroupBoxLayout = new QVBoxLayout;
    writeLatencyGroupBoxLayout->addWidget(new QLabel(tr("By reducing the write-to-disk latency, it is possible to eliminate some of the\n"
                                                        "lag between data being acquired and being written to disk. This can be useful\n"
                                                        "if another application needs to access the data immediately as it is written.\n"
                                                        "However, every block of data that is written incurs some overhead, so this\n"
                                                        "comes at the cost of less efficient writing, and may limit data writing rates.\n"
                                                        "When saving large amounts of data, the highest latency is recommended."), this));
    writeLatencyGroupBoxLayout->addLayout(writeLatencySelectionRow);

    QVBoxLayout *plottingModeGroupBoxLayout = new QVBoxLayout;
    plottingModeGroupBoxLayout->addWidget(new QLabel(tr("For large resolution monitors, the Original method of plotting may be too\n"
                                                        "slow to keep up in real time. High Efficiency plotting mode significantly\n"
                                                        "improves performance by only plotting the most recently acquired sections\n"
                                                        "of the waveforms. This feature works only for sweep mode."), this));
    plottingModeGroupBoxLayout->addLayout(plottingModeRow);

    QGroupBox *XPUGroupBox = new QGroupBox(tr("XPU"), this);
    XPUGroupBox->setLayout(XPUGroupBoxLayout);

    QGroupBox *writeLatencyGroupBox = new QGroupBox(tr("Write to Disk Latency"), this);
    writeLatencyGroupBox->setLayout(writeLatencyGroupBoxLayout);

    QGroupBox *plottingModeGroupBox = new QGroupBox(tr("Plotting Mode"), this);
    plottingModeGroupBox->setLayout(plottingModeGroupBoxLayout);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(XPUGroupBox);
    mainLayout->addWidget(writeLatencyGroupBox);
    mainLayout->addWidget(plottingModeGroupBox);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    initialize();
}

void PerformanceOptimizationDialog::initialize()
{
    // Find the currently used XPU and make that the selected entry in its combo box.
    int usedXPUIndex = state->usedXPUIndex();
    if (usedXPUIndex >= 0)
        XPUSelectionComboBox->setCurrentIndex(state->usedXPUIndex());
    else
        qDebug() << "Error: Invalid usedXPUIndex value";

    // Find the current write-to-disk latency and make the selected entry in its combo box.
    writeLatencyComboBox->setCurrentIndex(state->writeToDiskLatency->getIndex());

    // Find the current plottingMode state and make the selected entry in its combo box.
    plottingModeComboBox->setCurrentIndex(state->plottingMode->getIndex());

    if (state->testMode->getValue()) {
        writeLatencyComboBox->setEnabled(false);
        plottingModeComboBox->setEnabled(false);
    }
}
