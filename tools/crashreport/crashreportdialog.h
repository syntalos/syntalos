/*
 * Copyright (C) 2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class CrashReportDialog; }
QT_END_NAMESPACE

class CrashReportDialog : public QDialog
{
    Q_OBJECT

public:
    CrashReportDialog(QWidget *parent = nullptr);
    ~CrashReportDialog();

private slots:
    void on_closeButton_clicked();
    void on_nextButton_clicked();

private:
    Ui::CrashReportDialog *ui;
};
