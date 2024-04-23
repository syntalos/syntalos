/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>

#include "datactl/datatypes.h"

class PlotSeriesSettings
{
public:
    PlotSeriesSettings()
        : isVisible(true),
          isDigital(false)
    {
    }

    explicit PlotSeriesSettings(const QString &signalName, bool visible = true)
        : name(signalName),
          isVisible(visible),
          isDigital(false)
    {
    }

    PlotSeriesSettings(const PlotSeriesSettings &other) = default;
    PlotSeriesSettings &operator=(const PlotSeriesSettings &other) = default;

    PlotSeriesSettings(PlotSeriesSettings &&other) noexcept = default;
    PlotSeriesSettings &operator=(PlotSeriesSettings &&other) noexcept = default;

    QString name;
    bool isVisible;
    bool isDigital;
};

class TimePlotWidget : public QOpenGLWidget, private QOpenGLExtraFunctions
{
public:
    explicit TimePlotWidget(QWidget *parent = nullptr);
    ~TimePlotWidget();

    void setUpdateInterval(int frequency);

    void setTitle(const QString &title);
    void setTitleVisible(bool visible);

    void clear();
    void setRunning(bool running);
    void setYAxisLabel(const QString &label);
    void setBufferSize(size_t size);

    int addSeries(const QString &seriesName, const PlotSeriesSettings &settings = PlotSeriesSettings());
    void addToSeriesF(int seriesIndex, const Eigen::RowVectorXd &vec);
    void addToSeriesI(int seriesIndex, const Eigen::RowVectorXi &vec);
    void addToTimeseries(const VectorXu &vec, double divisor);

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    class Private;
    Q_DISABLE_COPY(TimePlotWidget)
    QScopedPointer<Private> d;
};
