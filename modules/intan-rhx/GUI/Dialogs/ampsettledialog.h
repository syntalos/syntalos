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

#ifndef AMPSETTLEDIALOG_H
#define AMPSETTLEDIALOG_H

#include <QDialog>

class QDialogButtonBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QComboBox;

class AmpSettleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AmpSettleDialog(double lowerSettleBandwidth, bool useFastSettle, bool headstageGlobalSettle,
                             QWidget *parent);

    QLineEdit *lowFreqSettleLineEdit;
    QCheckBox *settleHeadstageCheckBox;
    QComboBox *ampSettleModeComboBox;
    QDialogButtonBox *buttonBox;

private slots:
    void onLineEditTextChanged();
    void setAmpSettleMode(int index);

private:
    QLabel *lowRangeSettleLabel;
};

#endif // AMPSETTLEDIALOG_H
