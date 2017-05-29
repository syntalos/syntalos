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
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QDebug>

TracePlotProxy::TracePlotProxy(QObject *parent)
    : QObject(parent),
      m_maxXVal(0)
{
    m_plot = new TracePlot();

    m_plot->legend()->hide();
    m_plot->createDefaultAxes();
}

static inline int makePortChanMapId(int port, int chan)
{
    return port * 1000 + chan;
}

TracePlot *TracePlotProxy::plot() const
{
    return m_plot;
}

void TracePlotProxy::addChannel(int port, int chan)
{
    auto details = new ChannelDetails(this);
    details->portChan = qMakePair(port, chan);

    QLineSeries *series = new QLineSeries();
    series->setUseOpenGL(true);
    m_plot->addSeries(series);
    details->series = series;

    m_plot->createDefaultAxes();
    m_plot->axisY(series)->setMax(250);
    m_plot->axisY(series)->setMin(-250);

    details->series->points().reserve(20000);
    m_channels.insert(makePortChanMapId(port, chan), details);

    m_plot->setAnimationOptions(QChart::SeriesAnimations);
}

void TracePlotProxy::removeChannel(int port, int chan)
{
    auto key = makePortChanMapId(port, chan);
    if (!m_channels.contains(key))
        return;
    auto details = m_channels.value(key);
    m_plot->removeSeries(details->series);
    m_channels.remove(key);

    delete details;
}

QList<ChannelDetails *> TracePlotProxy::channels() const
{
    return m_channels.values();
}

void TracePlotProxy::updatePlot()
{
    ChannelDetails *details;

    foreach (details, m_channels.values()) {
        if (!details->enabled)
            continue;

        // replace is *much* faster than append(QPointF)
        // see https://bugreports.qt.io/browse/QTBUG-55714
        details->series->replace(details->data);
        if (details->xPos > m_maxXVal)
            m_maxXVal = details->xPos;
    }
}

ChannelDetails *TracePlotProxy::getDetails(int port, int chan) const
{
    auto key = makePortChanMapId(port, chan);
    if (!m_channels.contains(key))
        return nullptr;
    return m_channels.value(key);
}

void TracePlotProxy::adjustView()
{
    m_plot->axisX()->setRange(m_maxXVal - 2000, m_maxXVal);
}

void TracePlotProxy::applyDisplayModifiers()
{
    ChannelDetails *details;

    foreach (details, m_channels.values()) {
        if (details->dataOrig.size() == 0)
            details->dataOrig = details->data;

        if (details->multiplier == 0)
            details->multiplier = 1;

        details->storeOrig = true;

        for (auto i = 0; i < details->dataOrig.size(); i++) {
            details->data[i].setY(details->dataOrig[i].y() * details->multiplier + details->yShift);
        }
    }

    updatePlot();
}

void TracePlotProxy::reset()
{
    ChannelDetails *details;

    foreach (details, m_channels.values()) {
        details->reset();
    }

    m_maxXVal = 0;
}
