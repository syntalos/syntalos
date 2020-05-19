/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QDialog>
#include <QLabel>

#include "moduleapi.h"

namespace Ui {
class TimingsDialog;
}

namespace Syntalos {

class TimingDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimingDisplayWidget(const QString &title, QWidget *parent = nullptr);

    void setStrategies(const TimeSyncStrategies &strategies);
    void setCheckInterval(const std::chrono::microseconds &interval);
    void setTolerance(const std::chrono::microseconds &tolerance);
    void setCurrentOffset(const std::chrono::microseconds &offset);

private:
    QLabel *m_lblTitle;
    QLabel *m_lblStrategies;
    QLabel *m_lblTolerance;
    QLabel *m_lblOffset;
    QLabel *m_lblInfo;
};

class TimingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TimingsDialog(QWidget *parent = nullptr);
    ~TimingsDialog();

    void onSynchronizerDetailsChanged(const QString &id, const TimeSyncStrategies &strategies,
                                      const microseconds_t &tolerance);
    void onSynchronizerOffsetChanged(const QString &id, const microseconds_t &currentOffset);

    void clear();

private:
    Ui::TimingsDialog *ui;

    QHash<AbstractModule*, TimingDisplayWidget*> m_tdispMap;
};

}; // end of namespace
