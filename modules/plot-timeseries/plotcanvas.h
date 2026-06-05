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
#include <set>

#include "datactl/datatypes.h"

using namespace Syntalos;

class PlotCanvas : public QOpenGLWidget, private QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    struct ChannelInfo {
        std::string portId;
        int colIdx;
        std::string signalName;
        bool digital;
        bool enabled;
        int graphId;
    };

    explicit PlotCanvas(QWidget *parent = nullptr);
    ~PlotCanvas() override;

    void setUpdateInterval(int frequency);
    void setBufferSize(size_t size);
    void setRunning(bool running);

    float historyLength() const;
    void setHistoryLength(float seconds);

    // Port lifecycle
    void registerPort(
        const std::string &portId,
        double timestampDivisor,
        const std::string &yLabel,
        double dataScale = 1.0,
        double dataOffset = 0.0);
    void unregisterPort(const std::string &portId);
    void clearAll();
    void clearRuntimeData();

    // Channel lifecycle
    int ensureChannel(const std::string &portId, int colIdx, const std::string &signalName);
    void updatePortChannels(const std::string &portId, const std::vector<std::string> &signalNames);
    bool channelEnabled(int channelIndex) const;
    void setChannelEnabled(int channelIndex, bool enabled);
    void setChannelDigital(int channelIndex, bool digital);

    // Data ingestion (thread-safe)
    void appendBlockF(
        const std::string &portId,
        const VectorXu64 &timestamps,
        const Eigen::Ref<const MatrixXf> &data,
        const int *channelIdx,
        int nCols);
    void appendBlockI(
        const std::string &portId,
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

    ssize_t findChannelIndex(const std::string &portId, int colIdx) const;
    int appendChannel(const std::string &portId, int colIdx, const std::string &signalName);
    void tombstoneChannels(const std::set<size_t> &removed);
    void moveChannelToGraph(int channelIndex, int destGraphId);
    void createGraphWithChannel(int channelIndex);
    int graphIndexById(int graphId) const;
};
