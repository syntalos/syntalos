/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QSet>

namespace Ui
{
class TableSettingsDialog;
}

class TableSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TableSettingsDialog(QWidget *parent = nullptr);
    ~TableSettingsDialog();

    void setRunning(bool running);

    bool useNameFromSource() const;
    void setUseNameFromSource(bool fromSource);

    QString dataName() const;
    void setDataName(const QString &value);

    bool saveData() const;
    void setSaveData(bool save);

    bool displayData() const;
    void setDisplayData(bool display);

private slots:
    void on_nameLineEdit_textChanged(const QString &arg1);
    void on_nameFromSrcCheckBox_toggled(bool checked);

private:
    Ui::TableSettingsDialog *ui;

    QString m_dataName;
};
