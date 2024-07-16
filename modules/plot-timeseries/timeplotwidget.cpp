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

#include "timeplotwidget.h"

#include <QTimer>
#include "qtimgui.h"
#include <imgui.h>
#include <implot.h>

template<typename T>
class AutoTrimBuffer
{
public:
    explicit AutoTrimBuffer(size_t cap = 80 * 1000)
        : m_capacity(cap)
    {
        m_buffer.reserve(cap);
    }

    void add(const T &value)
    {
        if (m_buffer.size() >= m_capacity)
            m_buffer.erase(m_buffer.begin());

        m_buffer.push_back(value);
    }

    const std::vector<T> &buffer() const
    {
        return m_buffer;
    }

    const T *data() const
    {
        return m_buffer.data();
    }

    bool isEmpty() const
    {
        return m_buffer.empty();
    }

    const T last() const
    {
        return m_buffer.back();
    }

    size_t size() const
    {
        return m_buffer.size();
    }

    void clear()
    {
        m_buffer.clear();
    }

    void setCapacity(size_t cap)
    {
        m_capacity = cap;
        m_buffer.reserve(cap);
    }

private:
    std::vector<T> m_buffer;
    size_t m_capacity;
};

class TimePlotWidget::Private
{
public:
    Private() {}
    ~Private() {}

    QTimer *updateTimer;
    std::mutex dataMutex;
    ImVec4 clearColor = ImColor(114, 144, 154);
    QtImGui::RenderRef qigr = nullptr;
    ImPlotContext *impCtx = nullptr;

    bool isRunning;
    bool showTitle;
    QString title;
    QString yAxisLabel;
    size_t bufferSize;

    AutoTrimBuffer<double> timeseries;
    std::vector<AutoTrimBuffer<double>> xdata;
    std::vector<PlotSeriesSettings> xdataSettings;

    float historyLen;
};

TimePlotWidget::TimePlotWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new TimePlotWidget::Private)
{
    d->isRunning = false;
    d->showTitle = false;
    d->title = QStringLiteral("Time Plot");
    d->yAxisLabel = "y";
    d->historyLen = 10.0f;

    // about 16MB per displayed series of double-precision entries by default
    setBufferSize(2080000);

    d->updateTimer = new QTimer(this);

    // update at ~60fps by default
    d->updateTimer->setInterval(1000 / 60);
}

TimePlotWidget::~TimePlotWidget()
{
    if (d->impCtx != nullptr)
        ImPlot::DestroyContext(d->impCtx);
}

void TimePlotWidget::setUpdateInterval(int frequency)
{
    if (frequency < 1)
        frequency = 1;
    d->updateTimer->setInterval(1000 / frequency);
}

void TimePlotWidget::setTitle(const QString &title)
{
    if (title.isEmpty())
        return;
    d->title = title;
}

void TimePlotWidget::setTitleVisible(bool visible)
{
    d->showTitle = visible;
}

void TimePlotWidget::clear()
{
    d->xdata.clear();
    d->xdataSettings.clear();
    d->timeseries.clear();
}

void TimePlotWidget::setRunning(bool running)
{
    d->isRunning = running;
}

void TimePlotWidget::setYAxisLabel(const QString &label)
{
    d->yAxisLabel = label;
}

void TimePlotWidget::setBufferSize(size_t size)
{
    if (size < 10)
        size = 10;
    d->bufferSize = size;
    d->timeseries = AutoTrimBuffer<double>(d->bufferSize);

    for (auto &buf : d->xdata)
        buf.setCapacity(d->bufferSize);
}

int TimePlotWidget::addSeries(const QString &seriesName, const PlotSeriesSettings &settings)
{
    d->xdata.push_back(AutoTrimBuffer<double>(d->bufferSize));

    auto sc = PlotSeriesSettings(settings);
    sc.name = seriesName;
    d->xdataSettings.push_back(sc);

    return d->xdata.size() - 1;
}

void TimePlotWidget::addToSeriesF(int seriesIndex, const Eigen::RowVectorXd &vec)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);

    for (int i = 0; i < vec.size(); ++i)
        d->xdata[seriesIndex].add(vec(i));
}

void TimePlotWidget::addToSeriesI(int seriesIndex, const Eigen::RowVectorXi &vec)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);

    for (int i = 0; i < vec.size(); ++i)
        d->xdata[seriesIndex].add(vec(i));
}

void TimePlotWidget::addToTimeseries(const VectorXul &vec, double divisor)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);

    for (int i = 0; i < vec.rows(); ++i)
        d->timeseries.add(vec(i, 0) / divisor);
}

void TimePlotWidget::initializeGL()
{
    initializeOpenGLFunctions();
    d->qigr = QtImGui::initialize(this, false);
    d->impCtx = ImPlot::CreateContext();

    // start updating graphics
    QObject::connect(d->updateTimer, SIGNAL(timeout()), this, SLOT(update()));
    d->updateTimer->start();
}

void TimePlotWidget::paintGL()
{
    QtImGui::newFrame(d->qigr);
    ImPlot::SetCurrentContext(d->impCtx);

    static ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::Begin("W", nullptr, winFlags);

    ImGui::BeginDisabled(!d->isRunning);
    ImGui::SliderFloat("History Length", &d->historyLen, 1, 60, "%.1f s");
    ImGui::EndDisabled();

    const float yLimit = d->timeseries.isEmpty() ? 0 : d->timeseries.last();

    bool ok;
    if (d->showTitle)
        ok = ImPlot::BeginPlot(qPrintable(d->title), ImVec2(-1, -1));
    else
        ok = ImPlot::BeginPlot("##Plot", ImVec2(-1, -1));

    if (ok) {
        // ensure no new data is added to our buffer while we render existing data
        const std::lock_guard<std::mutex> lock(d->dataMutex);

        ImPlot::SetupAxes("time [s]", qPrintable(d->yAxisLabel), ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
        if (d->isRunning) {
            ImPlot::SetupAxisLimits(ImAxis_X1, yLimit - d->historyLen, yLimit, ImGuiCond_Always);
        }
        // ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5, +0.5);
        ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);

        for (uint i = 0; i < d->xdata.size(); ++i) {
            const auto pss = d->xdataSettings[i];
            if (pss.isDigital) {
                ImPlot::PlotDigital(
                    qPrintable(pss.name), d->timeseries.data(), d->xdata[i].data(), d->timeseries.size());
            } else {
                ImPlot::PlotLine(qPrintable(pss.name), d->timeseries.data(), d->xdata[i].data(), d->timeseries.size());
            }
        }

        ImPlot::EndPlot();
    }

    ImGui::End();

    // Clear previous content
    glViewport(0, 0, width(), height());
    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    QtImGui::render(d->qigr);
}
