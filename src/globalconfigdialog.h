/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDialog>
#include <QSettings>

#include "globalconfig.h"
#include "utils/ipcconfig.h"

namespace Ui
{
class GlobalConfigDialog;
}

class GlobalConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GlobalConfigDialog(QWidget *parent = nullptr);
    ~GlobalConfigDialog();

private slots:
    void on_colorModeComboBox_currentIndexChanged(int index);
    void on_cbEmergencyOOMStop_toggled(bool checked);

    void on_defaultNicenessSpinBox_valueChanged(int arg1);
    void on_defaultRTPrioSpinBox_valueChanged(int arg1);
    void on_explicitCoreAffinitiesCheckBox_toggled(bool checked);
    void on_cpuAffinityWarnButton_clicked();

    void on_cbDisplayDevModules_toggled(bool checked);
    void on_cbSaveDiagnostic_toggled(bool checked);
    void on_btnCreateDevDir_clicked();
    void on_cbPythonVenvForScripts_toggled(bool checked);

    void on_cbRoudiMonitoringDisabled_toggled(bool checked);
    void on_sbPool1ChunkCount_editingFinished();
    void on_sbPool1ChunkSize_editingFinished();
    void on_sbPool2ChunkCount_editingFinished();
    void on_sbPool2ChunkSize_editingFinished();
    void on_btnIpcPoolReset_clicked();

signals:
    void defaultColorSchemeChanged();

private:
    void updateCreateDevDirButtonState();
    void checkMemPoolValuesValid();

    Ui::GlobalConfigDialog *ui;
    Syntalos::GlobalConfig *m_gc;
    Syntalos::IPCConfig *m_ipcc;
    bool m_acceptChanges;
};
