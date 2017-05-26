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

#ifndef TRACEPLOTPROXY_H
#define TRACEPLOTPROXY_H

#include <QObject>
#include <QMap>

#include "traceplot.h"

namespace QtCharts {
class QXYSeries;
}

class ChannelDetails : public QObject
{
    Q_OBJECT
public:
    explicit ChannelDetails(QObject *parent = 0)
        : QObject(parent),
          xPos(0)
    {
        data.reserve(20000);
    }

    void reset ()
    {
        xPos = 0;
        prevData = data;
        data.clear();
    }

    QXYSeries *series;
    QPair<int, int> portChan;

    double multiplier;
    double yShift;

    QList<QPointF> data;
    QList<QPointF> prevData;

    ssize_t xPos;
};

class TracePlotProxy : public QObject
{
    Q_OBJECT
public:
    explicit TracePlotProxy(QObject *parent = 0);

    TracePlot *plot() const;

    void addChannel(int port, int chan);
    void removeChannel(int port, int chan);

    QList<ChannelDetails*> channels() const;

    void updatePlot();

    ChannelDetails *getDetails(int port, int chan) const;

    void adjustView();

signals:

public slots:

private:
    TracePlot *m_plot;

    QMap<int, ChannelDetails*> m_channels;

    int m_maxXVal;
};

#endif // TRACEPLOTPROXY_H
