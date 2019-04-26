//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.5
//  Copyright (C) 2013-2016 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef TRIGGERRECORDDIALOG_H
#define TRIGGERRECORDDIALOG_H

#include <QDialog>
#include "qtincludes.h"

class QDialogButtonBox;
class QComboBox;
class QSpinBox;

class TriggerRecordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TriggerRecordDialog(int initialTriggerChannel, int initialTriggerPolarity,
                                 int initialTriggerBuffer, int initialPostTrigger,
                                 bool initialSaveTriggerChannel, QWidget *parent);

    QDialogButtonBox *buttonBox;
    QCheckBox *saveTriggerChannelCheckBox;
    int digitalInput;
    int triggerPolarity;
    int recordBuffer;
    int postTriggerTime;

signals:

public slots:

private slots:
    void setDigitalInput(int index);
    void setTriggerPolarity(int index);
    void recordBufferSeconds(int value);
    void postTriggerSeconds(int value);

private:
    QComboBox *digitalInputComboBox;
    QComboBox *triggerPolarityComboBox;

    QSpinBox *recordBufferSpinBox;
    QSpinBox *postTriggerSpinBox;

};

#endif // TRIGGERRECORDDIALOG_H
