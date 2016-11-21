/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <QWidget>

class QFormLayout;
class QLabel;
class QEvent;

class StatusWidget : public QWidget
{
    Q_OBJECT
public:
    enum Status {
        Unknown,
        Configured,
        Ready,
        Active,
        Broken,
        Missing
    };

    explicit StatusWidget(QWidget *parent = 0);

    void setSystemStatus(Status status);
    void setIntanStatus(Status status);
    void setFirmataStatus(Status status);
    void setVideoStatus(Status status);

signals:

public slots:

private:
    void labelSetStatus(QLabel *label, Status status);

    QFormLayout *m_layout;

    QLabel *m_sysStatus;
    QLabel *m_intanStatus;
    QLabel *m_firmataStatus;
    QLabel *m_videoStatus;
};

#endif // STATUSWIDGET_H
