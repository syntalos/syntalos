/*
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMainWindow>

#include "queuemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class EncodeWindow; }
QT_END_NAMESPACE

class QSvgWidget;

class TaskManager;

class EncodeWindow : public QMainWindow
{
    Q_OBJECT

public:
    EncodeWindow(QWidget *parent = nullptr);
    ~EncodeWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_runButton_clicked();
    void on_parallelTasksCountSpinBox_valueChanged(int value);
    void on_tasksTable_activated(const QModelIndex &index);

private:
    void obtainSleepShutdownIdleInhibitor();

private:
    Ui::EncodeWindow *ui;

    QueueModel *m_queueModel;
    TaskManager *m_taskManager;

    QSvgWidget *m_busyIndicator;
};
