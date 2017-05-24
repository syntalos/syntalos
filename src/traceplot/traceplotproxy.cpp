/*
 * Copyright (C) 2017 Matthias Klumpp <matthias@tenstral.net>
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

#include "traceplotproxy.h"

#include <QtCore/QtMath>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

TracePlotProxy::TracePlotProxy(QObject *parent)
    : QObject(parent)
{
    m_plot = new TracePlot();

    QLineSeries *series = new QLineSeries();
    for (int i = 0; i < 500; i++) {
        QPointF p((qreal) i, qSin(M_PI / 50 * i) * 100);
        p.ry() += qrand() % 20;
        *series << p;
    }
    m_plot->addSeries(series);
    m_series.append(series);

    m_plot->setAnimationOptions(QChart::SeriesAnimations);
    m_plot->legend()->hide();
    m_plot->createDefaultAxes();
}

TracePlot *TracePlotProxy::plot() const
{
    return m_plot;
}
