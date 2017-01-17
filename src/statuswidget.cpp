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

#include "statuswidget.h"

#include <QFormLayout>
#include <QLabel>
#include <QEvent>

StatusWidget::StatusWidget(QWidget *parent)
    : QWidget(parent),
      m_layout(new QFormLayout)
{
    setLayout(m_layout);
    setWindowTitle("Status");

    auto lblSystem = new QLabel(this);
    lblSystem->setText("Project Status");

    auto lblIntan = new QLabel(this);
    lblIntan->setText("Intan Recording");

    auto lblFirmata = new QLabel(this);
    lblFirmata->setText("Firmata I/O");

    auto lblVideo = new QLabel(this);
    lblVideo->setText("Video / Tracking");

    m_sysStatus = new QLabel(this);
    m_intanStatus = new QLabel(this);
    m_firmataStatus = new QLabel(this);
    m_videoStatus = new QLabel(this);

    setSystemStatus(Unknown);
    setIntanStatus(Unknown);
    setFirmataStatus(Unknown);
    setVideoStatus(Unknown);

    m_layout->addRow(lblSystem, m_sysStatus);
    m_layout->addRow(lblIntan, m_intanStatus);
    m_layout->addRow(lblFirmata, m_firmataStatus);
    m_layout->addRow(lblVideo, m_videoStatus);
}

void StatusWidget::labelSetStatus(QLabel *label, Status status)
{
    switch (status) {
    case Disabled:
        label->setText("disabled");
        label->setStyleSheet("QLabel {background-color: sandybrown; color: black; }");
        break;
    case Configured:
        label->setText("configured");
        label->setStyleSheet("QLabel {background-color: ghostwhite; color: black; }");
        break;
    case Ready:
        label->setText("ready");
        label->setStyleSheet("QLabel {background-color: green; color: black; }");
        break;
    case Active:
        label->setText("active");
        label->setStyleSheet("QLabel {background-color: lawngreen; color: black; }");
        break;
    case Broken:
        label->setText("broken");
        label->setStyleSheet("QLabel {background-color: red; color: black; }");
        break;
    case Missing:
        label->setText("missing");
        label->setStyleSheet("QLabel {background-color: black; color: white; }");
        break;
    default:
        label->setText("unknown");
        label->setStyleSheet("QLabel {background-color: grey; color: black; }");
        break;
    }
}

void StatusWidget::setSystemStatus(Status status)
{
    labelSetStatus(m_sysStatus, status);
}

void StatusWidget::setIntanStatus(Status status)
{
    labelSetStatus(m_intanStatus, status);
}

void StatusWidget::setFirmataStatus(Status status)
{
    labelSetStatus(m_firmataStatus, status);
}

void StatusWidget::setVideoStatus(Status status)
{
    labelSetStatus(m_videoStatus, status);
}
