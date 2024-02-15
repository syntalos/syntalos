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
class JSONSettingsDialog;
}

class JSONSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JSONSettingsDialog(QWidget *parent = nullptr);
    ~JSONSettingsDialog();

    void setRunning(bool running);

    bool useNameFromSource() const;
    void setUseNameFromSource(bool fromSource);

    QString dataName() const;
    void setDataName(const QString &value);

    QString jsonFormat() const;
    void setJsonFormat(const QString &format);

    bool recordAllData() const;
    void setRecordAllData(bool enabled);

    void setAvailableEntries(const QStringList &list);
    QStringList availableEntries();

    QSet<QString> recordedEntriesSet() const;
    QStringList recordedEntries() const;
    void setRecordedEntries(const QStringList &list);

private slots:
    void on_nameLineEdit_textChanged(const QString &arg1);
    void on_nameFromSrcCheckBox_toggled(bool checked);

    void on_useAllDataCheckBox_toggled(bool checked);
    void on_addRecordedButton_clicked();

    void on_removeRecordedButton_clicked();

private:
    Ui::JSONSettingsDialog *ui;

    QString m_dataName;
};
