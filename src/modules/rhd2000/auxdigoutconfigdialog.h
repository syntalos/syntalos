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

#ifndef AUXDIGOUTCONFIGDIALOG_H
#define AUXDIGOUTCONFIGDIALOG_H

#include <QDialog>
#include "qtincludes.h"

class AuxDigOutConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuxDigOutConfigDialog(QVector<bool> &auxOutEnabledIn, QVector<int> &auxOutChannelIn, QWidget *parent = 0);
    
    QCheckBox *enablePortACheckBox;
    QCheckBox *enablePortBCheckBox;
    QCheckBox *enablePortCCheckBox;
    QCheckBox *enablePortDCheckBox;

    QComboBox *channelPortAComboBox;
    QComboBox *channelPortBComboBox;
    QComboBox *channelPortCComboBox;
    QComboBox *channelPortDComboBox;

    QDialogButtonBox *buttonBox;

    bool enabled(int port);
    int channel(int port);

private:
    QVector<bool> auxOutEnabled;
    QVector<int> auxOutChannel;

signals:
    
public slots:
    
private slots:
    void enablePortAChanged(bool enable);
    void enablePortBChanged(bool enable);
    void enablePortCChanged(bool enable);
    void enablePortDChanged(bool enable);
    void channelPortAChanged(int channel);
    void channelPortBChanged(int channel);
    void channelPortCChanged(int channel);
    void channelPortDChanged(int channel);

};

#endif // AUXDIGOUTCONFIGDIALOG_H
