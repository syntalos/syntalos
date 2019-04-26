//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.4
//  Copyright (C) 2013 Intan Technologies
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

#ifndef CABLEDELAYDIALOG_H
#define CABLEDELAYDIALOG_H

#include <QDialog>
#include "qtincludes.h"

using namespace std;

class CableDelayDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CableDelayDialog(QVector<bool> &manualDelayEnabled, vector<int> &currentDelay, QWidget *parent = 0);
    
    QCheckBox *manualPortACheckBox;
    QCheckBox *manualPortBCheckBox;
    QCheckBox *manualPortCCheckBox;
    QCheckBox *manualPortDCheckBox;

    QSpinBox *delayPortASpinBox;
    QSpinBox *delayPortBSpinBox;
    QSpinBox *delayPortCSpinBox;
    QSpinBox *delayPortDSpinBox;

    QDialogButtonBox *buttonBox;

signals:
    
public slots:
    
};

#endif // CABLEDELAYDIALOG_H
