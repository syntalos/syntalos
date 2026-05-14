/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QVariantHash>

#include "datactl/datatypes.h"

using namespace Syntalos;

class PlotCanvas : public QOpenGLWidget, private QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    struct ChannelInfo {
        QString portId;
        int colIdx;
        QString signalName;
        bool digital;
        bool enabled;
        int graphId;
    };

    explicit PlotCanvas(QWidget *parent = nullptr);
    ~PlotCanvas() override;

    void setUpdateInterval(int frequency);
    void setBufferSize(size_t size);
    void setRunning(bool running);

    // Port lifecycle
    void registerPort(const QString &portId, double timestampDivisor, const QString &yLabel);
    void unregisterPort(const QString &portId);
    void clearAll();
    void clearRuntimeData();

    // Channel lifecycle
    int ensureChannel(const QString &portId, int colIdx, const QString &signalName);
    bool channelEnabled(int channelIndex) const;
    void setChannelEnabled(int channelIndex, bool enabled);
    void setChannelDigital(int channelIndex, bool digital);

    // Data ingestion (thread-safe)
    void appendBlockF(
        const QString &portId,
        const VectorXu64 &timestamps,
        const Eigen::Ref<const MatrixXf> &data,
        const int *channelIdx,
        int nCols);
    void appendBlockI(
        const QString &portId,
        const VectorXu64 &timestamps,
        const Eigen::Ref<const MatrixXi32> &data,
        const int *channelIdx,
        int nCols);

    // Graph operations
    void resetLayoutOneChannelPerGraph();
    int graphIdForChannel(int channelIndex) const;

    // Channel introspection
    int channelCount() const;
    ChannelInfo channelInfo(int channelIndex) const;

    // Persistence (called from module's serialize/load)
    QVariantList saveChannels() const;
    QVariantList saveGraphs() const;
    void loadChannels(const QVariantList &v);
    void loadGraphs(const QVariantList &v);

signals:
    void layoutChanged();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    class Private;
    Q_DISABLE_COPY(PlotCanvas)
    QScopedPointer<Private> d;

    void moveChannelToGraph(int channelIndex, int destGraphId);
    void createGraphWithChannel(int channelIndex);
    int graphIndexById(int graphId) const;
};
