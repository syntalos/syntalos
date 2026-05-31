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

#include "latencycanvas.h"

#include <QTimer>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#include "qtimgui.h"
#include <imgui.h>
#include <implot.h>

// Keep at most this many recent measurements for the histogram.
static constexpr size_t kMaxValues = 200000;

class LatencyCanvas::Private
{
public:
    Private() = default;

    QTimer *updateTimer = nullptr;
    std::mutex dataMutex;
    ImVec4 clearColor = ImColor(38, 38, 42);
    QtImGui::RenderRef qigr = nullptr;
    ImPlotContext *impCtx = nullptr;

    bool isRunning = false;
    int bins = 30;

    // All recorded latency values (milliseconds), oldest first. Capped at
    // kMaxValues with drop-oldest semantics.
    std::vector<float> values;

    // Render-frame snapshot, copied under the lock and rendered without it.
    std::vector<float> snapshot;
};

LatencyCanvas::LatencyCanvas(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new Private)
{
    d->values.reserve(8192);
    d->updateTimer = new QTimer(this);
    d->updateTimer->setInterval(1000 / 40);
}

LatencyCanvas::~LatencyCanvas()
{
    if (d->impCtx != nullptr)
        ImPlot::DestroyContext(d->impCtx);
}

void LatencyCanvas::setRunning(bool running)
{
    d->isRunning = running;
    if (d->updateTimer) {
        // ImGui is immediate-mode and needs a steady frame stream to stay
        // interactive. Redraw faster while running, slower while idle.
        d->updateTimer->setInterval(running ? 1000 / 40 : 1000 / 30);
        if (!d->updateTimer->isActive())
            d->updateTimer->start();
    }
}

void LatencyCanvas::addValue(float latencyMs)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    if (d->values.size() >= kMaxValues)
        d->values.erase(d->values.begin(), d->values.begin() + (d->values.size() - kMaxValues + 1));
    d->values.push_back(latencyMs);
}

void LatencyCanvas::clearRuntimeData()
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->values.clear();
}

int LatencyCanvas::binCount() const
{
    return d->bins;
}

void LatencyCanvas::setBinCount(int bins)
{
    if (bins < 1)
        bins = 1;
    if (bins > 1000)
        bins = 1000;
    d->bins = bins;
}

void LatencyCanvas::initializeGL()
{
    initializeOpenGLFunctions();
    d->qigr = QtImGui::initialize(this, false);
    d->impCtx = ImPlot::CreateContext();

    QObject::connect(d->updateTimer, SIGNAL(timeout()), this, SLOT(update()));
    d->updateTimer->start();
}

void LatencyCanvas::paintGL()
{
    QtImGui::newFrame(d->qigr);
    ImPlot::SetCurrentContext(d->impCtx);

    static ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("LatencyCanvasMain", nullptr, winFlags);

    // Controls
    ImGui::SliderInt("Bins", &d->bins, 1, 200);
    ImGui::SameLine();
    const bool clearClicked = ImGui::Button("Clear");

    // Snapshot phase (lock held only for the copy + clear)
    size_t count = 0;
    {
        const std::lock_guard<std::mutex> lock(d->dataMutex);
        if (clearClicked)
            d->values.clear();
        d->snapshot = d->values;
        count = d->snapshot.size();
    }

    // Compute statistics from the snapshot (no lock needed)
    if (count == 0) {
        ImGui::TextDisabled("Waiting for data…");
    } else {
        double sum = 0.0;
        float vMin = d->snapshot[0];
        float vMax = d->snapshot[0];
        for (const float v : d->snapshot) {
            sum += v;
            vMin = std::min(vMin, v);
            vMax = std::max(vMax, v);
        }
        const double mean = sum / count;

        double variance = 0.0;
        for (const float v : d->snapshot) {
            const double dv = v - mean;
            variance += dv * dv;
        }
        const double stddev = count > 1 ? std::sqrt(variance / (count - 1)) : 0.0;

        // Median (operate on a sorted copy)
        std::vector<float> sorted = d->snapshot;
        std::sort(sorted.begin(), sorted.end());
        const double median = (count % 2 == 0) ? 0.5 * (sorted[count / 2 - 1] + sorted[count / 2]) : sorted[count / 2];

        ImGui::Text(
            "n=%zu   last=%.3f ms   min=%.3f   max=%.3f   mean=%.3f   median=%.3f   sd=%.3f ms",
            count,
            (double)d->snapshot.back(),
            (double)vMin,
            (double)vMax,
            mean,
            median,
            stddev);
    }

    // Histogram
    if (ImPlot::BeginPlot("Latencies", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Latency [ms]", "Count", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        if (count > 0)
            ImPlot::PlotHistogram("##lat", d->snapshot.data(), (int)count, d->bins);
        ImPlot::EndPlot();
    }

    ImGui::End();

    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui::Render();
    QtImGui::render(d->qigr);
}
