/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include <QLabel>
#include "sysinfo.h"

using namespace Syntalos;

namespace Ui {
class SysInfoDialog;
}

class SysInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SysInfoDialog(SysInfo *sysInfo, QWidget *parent = nullptr);
    ~SysInfoDialog();

private:
    Ui::SysInfoDialog *ui;
    QPalette m_okTextPal;
    QPalette m_hintTextPal;
    QPalette m_warnTextPal;

    void setLabelTextStyle(SysInfoCheckResult checkResult, QLabel *label);
};
