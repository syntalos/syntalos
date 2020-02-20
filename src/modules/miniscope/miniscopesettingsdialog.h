/**
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MINISCOPESETTINGSDIALOG_H
#define MINISCOPESETTINGSDIALOG_H

#include <QDialog>
#include <QList>

namespace MScope {
class MiniScope;
}
namespace Ui {
class MiniscopeSettingsDialog;
}

class MiniscopeSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MiniscopeSettingsDialog(MScope::MiniScope *mscope, QWidget *parent = nullptr);
    ~MiniscopeSettingsDialog();

    void updateValues();

private slots:
    void on_sbExposure_valueChanged(int arg1);
    void on_sbExcitation_valueChanged(double arg1);
    void on_dialExcitation_valueChanged(int value);
    void on_sbGain_valueChanged(int arg1);
    void on_cbExtRecTrigger_toggled(bool checked);
    void on_sbDisplayMax_valueChanged(int arg1);
    void on_sbDisplayMin_valueChanged(int arg1);
    void on_fpsSpinBox_valueChanged(int arg1);
    void on_bgDivCheckBox_toggled(bool checked);
    void on_bgSubstCheckBox_toggled(bool checked);
    void on_accAlphaSpinBox_valueChanged(double arg1);
    void on_sbCamId_valueChanged(int arg1);

private:
    Ui::MiniscopeSettingsDialog *ui;
    QString m_recName;
    MScope::MiniScope *m_mscope;
};

#endif // MINISCOPESETTINGSDIALOG_H
