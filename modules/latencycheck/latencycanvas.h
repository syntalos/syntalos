/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

/**
 * @brief Live-updating latency histogram
 *
 * Latency values (in milliseconds) are fed in from the module thread via
 * addValue() and rendered on the GUI thread by a repaint timer. Access to the
 * value buffer is guarded by a mutex; the render path takes a brief snapshot.
 */
class LatencyCanvas : public QOpenGLWidget, private QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit LatencyCanvas(QWidget *parent = nullptr);
    ~LatencyCanvas() override;

    void setRunning(bool running);

    // Data ingestion (thread-safe; called from the module thread)
    void addValue(float latencyMs);

    // Clear all recorded values
    void clearRuntimeData();

    int binCount() const;
    void setBinCount(int bins);

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    class Private;
    Q_DISABLE_COPY(LatencyCanvas)
    QScopedPointer<Private> d;
};
