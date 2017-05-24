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

#include "traceplot.h"

namespace QtCharts {
class QLineSeries;
}

class TracePlotProxy : public QObject
{
    Q_OBJECT
public:
    explicit TracePlotProxy(QObject *parent = 0);

    TracePlot *plot() const;

signals:

public slots:

private:
    TracePlot *m_plot;
    QList<QLineSeries*> m_series;
};

#endif // TRACEPLOTPROXY_H
