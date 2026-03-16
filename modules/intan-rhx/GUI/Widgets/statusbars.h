//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.0
//
//  Copyright (c) 2020-2026 Intan Technologies
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
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef STATUSBARS_H
#define STATUSBARS_H

#include <QtWidgets>

enum BufferStatus {
    BufferNoWarning,
    BufferMinorWarning,
    BufferMajorWarning
};

class StatusBars : public QWidget
{
    Q_OBJECT
public:
    explicit StatusBars(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

public slots:
    void updateBars(double hwBufferPercent_, double swBufferPercent_, double cpuLoadPercent_);

signals:
    void bufferStatusChanged(BufferStatus newStatus);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage image;
    QImage background;

    const double MinorWarningThreshold = 50.0;
    const double MajorWarningThreshold = 80.0;

    double hwBufferPercent;
    double swBufferPercent;
    double cpuLoadPercent;

    QColor colorFromPercent(double percent) const;
};

#endif // STATUSBARS_H
