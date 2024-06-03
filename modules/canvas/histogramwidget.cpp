/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
 *                          Andrej Lajovic <andrej.lajovic@ad-vega.si>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "histogramwidget.h"
#include <QStyleOption>
#include <QPainter>

HistogramWidget::HistogramWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      unusedHists(&histograms2),
      histRed(histograms1.red),
      histGreen(histograms1.green),
      histBlue(histograms1.blue)
{
    indexed = true;
    logarithmic = false;

    QStyleOption opt;
    opt.initFrom(this);
    backgroundBrush = opt.palette.brush(QPalette::Window);
    foregroundBrush = opt.palette.brush(QPalette::WindowText);

    idleImageIcon = QIcon::fromTheme("histogram-symbolic");
    setIdle();
}

void HistogramWidget::setLogarithmic(bool logarithmic_)
{
    logarithmic = logarithmic_;
}

void HistogramWidget::setIdle()
{
    idle = true;
    update();
    return;
}

Histograms *HistogramWidget::unusedHistograms()
{
    return unusedHists;
}

void HistogramWidget::swapHistograms(bool grayscale)
{
    idle = false;
    indexed = grayscale;
    histRed = unusedHists->red;
    histGreen = unusedHists->green;
    histBlue = unusedHists->blue;
    if (unusedHists == &histograms1)
        unusedHists = &histograms2;
    else
        unusedHists = &histograms1;
    update();
}

void HistogramWidget::paintGL()
{
    QPainter painter(this);
    painter.setBackground(backgroundBrush);
    painter.fillRect(rect(), backgroundBrush);

    if (idle) {
        painter.drawPixmap(rect(), idleImageIcon.pixmap(rect().size()));
        return;
    }

    float wUnit = rect().width() / 256.;
    QPointF origin = rect().bottomLeft();

    if (indexed) {
        painter.setPen(foregroundBrush.color());
        painter.setBrush(foregroundBrush);

        float max = 0;
        for (int i = 0; i < 256; i++)
            if (histRed[i] > max)
                max = histRed[i];
        float hUnit = rect().height() / max;
        for (int i = 0; i < 256; i++) {
            float height = histRed[i] * hUnit;
            QPointF topLeft(origin + QPointF(i * wUnit, -height));
            QPointF bottomRight(origin + QPointF((i + 1) * wUnit, 0));
            painter.drawRect(QRectF(topLeft, bottomRight));
        }
    } else {
        QColor colors[] = {
            QColor::fromRgba(qRgba(255, 0, 0, 128)),
            QColor::fromRgba(qRgba(0, 255, 0, 128)),
            QColor::fromRgba(qRgba(0, 0, 255, 128))};
        float *histograms[] = {histRed, histGreen, histBlue};
        float max = 0;
        for (int c = 0; c < 3; c++) {
            for (int i = 0; i < 256; i++)
                if (histograms[c][i] > max)
                    max = histograms[c][i];
        }
        for (int c = 0; c < 3; c++) {
            painter.setPen(colors[c]);
            painter.setBrush(colors[c]);

            float hUnit = rect().height() / max;
            for (int i = 0; i < 256; i++) {
                float height = histograms[c][i] * hUnit;
                QPointF topLeft(origin + QPointF(i * wUnit, -height));
                QPointF bottomRight(origin + QPointF((i + 1) * wUnit, 0));
                painter.drawRect(QRectF(topLeft, bottomRight));
            }
        }
    }
}
