//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
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

#ifndef BANDWIDTHDIALOG_H
#define BANDWIDTHDIALOG_H

#include <QDialog>
#include "qtincludes.h"

class QDialogButtonBox;
class QLineEdit;
class QLabel;
class QCheckBox;

class BandwidthDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BandwidthDialog(double lowerBandwidth, double upperBandwidth, double dspCutoffFreq, bool dspEnabled, double sampleRate, QWidget *parent);

    QLineEdit *dspFreqLineEdit;
    QLineEdit *lowFreqLineEdit;
    QLineEdit *highFreqLineEdit;
    QCheckBox *dspEnableCheckBox;
    QDialogButtonBox *buttonBox;

signals:
    
public slots:

private slots:
    void onLineEditTextChanged();
    void onDspCheckBoxChanged(bool enable);

private:
    QLabel *dspRangeLabel;
    QLabel *lowRangeLabel;
    QLabel *highRangeLabel;
};

#endif // BANDWIDTHDIALOG_H
