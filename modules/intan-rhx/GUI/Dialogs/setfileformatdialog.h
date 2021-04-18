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

#ifndef SETFILEFORMATDIALOG_H
#define SETFILEFORMATDIALOG_H

#include <QDialog>

#include "rhxglobals.h"
#include "systemstate.h"

class QDialogButtonBox;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QRadioButton;
class QButtonGroup;
class QLabel;

class SetFileFormatDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SetFileFormatDialog(SystemState* state_, QWidget *parent);

    void updateFromState();

    bool getSaveAuxInWithAmps() const;
    bool getSaveWidebandAmps() const;
    bool getSaveLowpassAmps() const;
    bool getSaveHighpassAmps() const;
    bool getSaveDCAmps() const;
    bool getSaveSpikeData() const;
    bool getSaveSpikeSnapshots() const;
    int getSnapshotPreDetect() const;
    int getSnapshotPostDetect() const;
    int getRecordTime() const;
    int getLowpassDownsampleFactorIndex() const;
    QString getFileFormat() const;

private slots:
    void updateOldFileFormat();
    void updateLowpassSampleRate();
    void updateSaveLowpass();
    void updateSaveSpikes();
    void updateSaveSnapshots();

private:
    SystemState* state;

    QCheckBox *saveAuxInWithAmpCheckBox;
    QCheckBox *saveWidebandAmplifierWaveformsCheckBox;
    QCheckBox *saveLowpassAmplifierWaveformsCheckBox;
    QCheckBox *saveHighpassAmplifierWaveformsCheckBox;
    QCheckBox *saveSpikeDataCheckBox;
    QCheckBox *saveSpikeSnapshotsCheckBox;
    QSpinBox *spikeSnapshotPreDetectSpinBox;
    QSpinBox *spikeSnapshotPostDetectSpinBox;
    QCheckBox *saveDCAmplifierWaveformsCheckBox;
    QSpinBox *recordTimeSpinBox;
    QComboBox *lowpassWaveformDownsampleRateComboBox;

    QButtonGroup *buttonGroup;
    QRadioButton *fileFormatIntanButton;
    QRadioButton *fileFormatNeuroScopeButton;
    QRadioButton *fileFormatOpenEphysButton;
    QDialogButtonBox *buttonBox;

    QLabel *downsampleLabel;
    QLabel *lowpassSampleRateLabel;
    QLabel *fromLabel;
    QLabel *toLabel;

    FileFormat fileFormat;
};

#endif // SETFILEFORMATDIALOG_H
