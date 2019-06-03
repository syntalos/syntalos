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
#include "modules/rhd2000/waveplot.h"

namespace QtCharts {
class QXYSeries;
}
class QTimer;

class ChannelDetails : public QObject
{
    Q_OBJECT
public:
    explicit ChannelDetails(QObject *parent = nullptr)
        : QObject(parent),
          enabled(true),
          multiplier(1),
          xPos(0),
          storeOrig(false)
    {
        data.reserve(20000);
    }

    void reset ()
    {
        xPos = 0;
        dataPrev = data;
        data.clear();
        dataOrig.clear();
    }

    void addNewYValue(double xval)
    {
        if ((multiplier > 1) || (yShift != 0)) {
            if (multiplier == 0)
                multiplier = 1;

            storeOrig = true;
            data.append(QPointF(xPos, xval * multiplier + yShift));
        } else {
            data.append(QPointF(xPos, xval));
        }

        if (storeOrig)
            dataOrig.append(QPointF(xPos, xval));

        xPos += 1;
    }

    bool enabled;

    QXYSeries *series;
    QPair<int, int> portChan;

    double multiplier;
    double yShift;

    QList<QPointF> data;
    QList<QPointF> dataOrig;
    QList<QPointF> dataPrev;

    ssize_t xPos;
    bool storeOrig;
};

class TracePlotProxy : public QObject
{
    Q_OBJECT
public:
    explicit TracePlotProxy(QObject *parent = nullptr);

    TracePlot *plot() const;

    void addChannel(int port, int chan);
    void removeChannel(int port, int chan);

    QList<ChannelDetails*> channels() const;

    ChannelDetails *getDetails(int port, int chan) const;

    void adjustView();

    void applyDisplayModifiers();

    void reset();

    int refreshTime() const;
    void setRefreshTime(int v);

    void setWavePlot(WavePlot *wp);
    WavePlot *wavePlot() const;

signals:
    void maxHorizontalPositionChanged(int maxPos);

public slots:
    void updatePlot(bool nowait = false);
    void moveTo(int position);

private slots:
    void repaintPlot();

private:
    TracePlot *m_plot;
    QTimer *m_timer;

    QMap<int, ChannelDetails*> m_channels;

    int m_maxXVal;
    int m_refreshTime;
    WavePlot *m_waveplot;
};

#endif // TRACEPLOTPROXY_H
