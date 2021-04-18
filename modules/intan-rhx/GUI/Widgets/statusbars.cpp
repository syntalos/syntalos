//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include "statusbars.h"

StatusBars::StatusBars(QWidget *parent) :
    QWidget(parent)
{
    hwBufferPercent = 0.0;
    swBufferPercent = 0.0;
    cpuLoadPercent = 0.0;

    background = QImage(":images/status_header.png");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMinimumSize(background.size());
    setMaximumSize(background.size());
    image = QImage(background.size(), QImage::Format_ARGB32_Premultiplied);
}

QSize StatusBars::minimumSizeHint() const
{
    return background.size();
}

QSize StatusBars::sizeHint() const
{
    return background.size();
}

void StatusBars::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(&image);
    painter.drawImage(QPoint(0, 0), background);

    painter.fillRect(QRect(48, 3, round(hwBufferPercent), 5), colorFromPercent(hwBufferPercent));
    painter.fillRect(QRect(48, 11, round(swBufferPercent), 5), colorFromPercent(swBufferPercent));
    painter.fillRect(QRect(48, 19, round(cpuLoadPercent), 5), colorFromPercent(cpuLoadPercent));

    QStylePainter stylePainter(this);
    stylePainter.drawImage(0, 0, image);
}

void StatusBars::updateBars(double hwBufferPercent_, double swBufferPercent_, double cpuLoadPercent_)
{
    hwBufferPercent = hwBufferPercent_;
    swBufferPercent = swBufferPercent_;
    cpuLoadPercent = cpuLoadPercent_;
    update();
}

QColor StatusBars::colorFromPercent(double percent) const
{
    if (percent >= 90.0) return QColor(245, 10, 0);  // red
    else if (percent >= 80.0) return QColor(255, 191, 0);  // amber
    else return QColor(0, 150, 0);  // green
}
