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

#ifndef AUXDIGOUTCONFIGDIALOG_H
#define AUXDIGOUTCONFIGDIALOG_H

#include <vector>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;

class AuxDigOutConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuxDigOutConfigDialog(std::vector<bool> &auxOutEnabledIn, std::vector<int> &auxOutChannelIn, int numPorts,
                                   QWidget *parent = nullptr);

    bool enabled(int port);
    int channel(int port);

private slots:
    void enablePortAChanged(bool enable);
    void enablePortBChanged(bool enable);
    void enablePortCChanged(bool enable);
    void enablePortDChanged(bool enable);
    void enablePortEChanged(bool enable);
    void enablePortFChanged(bool enable);
    void enablePortGChanged(bool enable);
    void enablePortHChanged(bool enable);
    void channelPortAChanged(int channel);
    void channelPortBChanged(int channel);
    void channelPortCChanged(int channel);
    void channelPortDChanged(int channel);
    void channelPortEChanged(int channel);
    void channelPortFChanged(int channel);
    void channelPortGChanged(int channel);
    void channelPortHChanged(int channel);

private:
    std::vector<bool> auxOutEnabled;
    std::vector<int> auxOutChannel;

    QCheckBox *enablePortACheckBox;
    QCheckBox *enablePortBCheckBox;
    QCheckBox *enablePortCCheckBox;
    QCheckBox *enablePortDCheckBox;
    QCheckBox *enablePortECheckBox;
    QCheckBox *enablePortFCheckBox;
    QCheckBox *enablePortGCheckBox;
    QCheckBox *enablePortHCheckBox;

    QComboBox *channelPortAComboBox;
    QComboBox *channelPortBComboBox;
    QComboBox *channelPortCComboBox;
    QComboBox *channelPortDComboBox;
    QComboBox *channelPortEComboBox;
    QComboBox *channelPortFComboBox;
    QComboBox *channelPortGComboBox;
    QComboBox *channelPortHComboBox;

    QDialogButtonBox *buttonBox;

    void populatePortComboBox(QComboBox* channelPortComboBox);
};

#endif // AUXDIGOUTCONFIGDIALOG_H
