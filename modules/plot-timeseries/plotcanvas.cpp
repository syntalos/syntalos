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

#include "plotcanvas.h"

#include <QTimer>
#include <QHash>
#include <mutex>
#include <vector>
#include <memory_resource>

#include "qtimgui.h"
#include <imgui.h>
#include <implot.h>

template<typename T>
class RingBuffer
{
public:
    explicit RingBuffer(size_t cap = 80 * 1000)
        : m_capacity(cap),
          m_head(0),
          m_count(0)
    {
        m_data.resize(cap);
    }

    void add(const T &value)
    {
        m_data[m_head] = value;
        m_head = (m_head + 1) % m_capacity;
        if (m_count < m_capacity)
            ++m_count;
    }

    const T *data() const
    {
        return m_data.data();
    }

    int offset() const
    {
        return (m_count == m_capacity) ? static_cast<int>(m_head) : 0;
    }

    bool isEmpty() const
    {
        return m_count == 0;
    }

    T last() const
    {
        return m_data[(m_head + m_capacity - 1) % m_capacity];
    }

    size_t size() const
    {
        return m_count;
    }

    void clear()
    {
        m_head = 0;
        m_count = 0;
    }

    void setCapacity(size_t cap)
    {
        m_capacity = cap;
        m_data.assign(cap, T{});
        m_head = 0;
        m_count = 0;
    }

private:
    std::pmr::vector<T> m_data;
    size_t m_capacity;
    size_t m_head;
    size_t m_count;
};

struct ChannelData {
    QString portId;
    int colIdx = 0;
    QString signalName;
    bool digital = false;
    bool enabled = true;
    RingBuffer<float> samples;
};

struct PortData {
    QString id;
    float timestampDivisor = 1000.0;
    QString yLabel = QStringLiteral("y");
    RingBuffer<float> timestamps;
};

struct Graph {
    int id = 0;
    QList<int> channels; // indices into m_channels
    float sizeWeight = 1.0f;
};

constexpr float kMinGraphWeight = 1.0f;
constexpr float kSplitterHeight = 6.0f;
constexpr float kMinGraphHeight = 60.0f;

/**
 * Get a deterministic series color from a channel's name + column index.
 */
static ImVec4 colorForChannel(const QString &name, int colIdx)
{
    const uint h = qHash(QStringLiteral("%1#%2").arg(name).arg(colIdx));
    const float hue = (h % 360u) / 360.0f;
    const float sat = 0.55f + ((h >> 9) % 36u) / 100.0f;  // 0.55..0.90
    const float val = 0.80f + ((h >> 18) % 16u) / 100.0f; // 0.80..0.95
    ImVec4 col(0, 0, 0, 1);
    ImGui::ColorConvertHSVtoRGB(hue, sat, val, col.x, col.y, col.z);
    return col;
}

class PlotCanvas::Private
{
public:
    Private() = default;

    QTimer *updateTimer = nullptr;
    std::mutex dataMutex;
    ImVec4 clearColor = ImColor(38, 38, 42);
    QtImGui::RenderRef qigr = nullptr;
    ImPlotContext *impCtx = nullptr;

    bool isRunning = false;
    int runningIntervalMs = 1000 / 60;
    size_t bufferSize = 2080000;
    float historyLen = 2.0f;

    QHash<QString, PortData> ports;
    std::vector<ChannelData> channels;
    QHash<QPair<QString, int>, int> channelLookup; // (portId, colIdx) -> index into channels

    std::vector<Graph> graphs;
    int nextGraphId = 1;

    // linked X axis storage; refreshed each frame in running mode
    double xLinkMin = 0.0;
    double xLinkMax = 10.0;
};

PlotCanvas::PlotCanvas(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new Private)
{
    d->updateTimer = new QTimer(this);
    d->updateTimer->setInterval(1000 / 60);
}

PlotCanvas::~PlotCanvas()
{
    if (d->impCtx != nullptr)
        ImPlot::DestroyContext(d->impCtx);
}

void PlotCanvas::setUpdateInterval(int frequency)
{
    if (frequency < 1)
        frequency = 1;
    d->runningIntervalMs = 1000 / frequency;
    // Apply immediately only when running; idle rate stays slow.
    if (d->isRunning)
        d->updateTimer->setInterval(d->runningIntervalMs);
}

void PlotCanvas::setBufferSize(size_t size)
{
    if (size < 10)
        size = 10;
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->bufferSize = size;
    for (auto &p : d->ports)
        p.timestamps.setCapacity(size);
    for (auto &c : d->channels) {
        if (c.enabled)
            c.samples.setCapacity(size);
    }
}

void PlotCanvas::setRunning(bool running)
{
    d->isRunning = running;
    if (d->updateTimer) {
        // ImGui is immediate-mode and needs a steady frame stream for
        // interaction (zoom, hover, drag). While running, we redraw at the
        // user-configured rate to follow live data; while idle, drop to a
        // low rate so the UI stays responsive without burning CPU.
        d->updateTimer->setInterval(running ? d->runningIntervalMs : 1000 / 30);
        if (!d->updateTimer->isActive())
            d->updateTimer->start();
    }
}

void PlotCanvas::registerPort(const QString &portId, double timestampDivisor, const QString &yLabel)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto &p = d->ports[portId];
    p.id = portId;
    p.timestampDivisor = timestampDivisor;
    p.yLabel = yLabel.isEmpty() ? QStringLiteral("y") : yLabel;
    if (p.timestamps.size() == 0)
        p.timestamps.setCapacity(d->bufferSize);
}

void PlotCanvas::unregisterPort(const QString &portId)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->ports.remove(portId);

    // remove channels belonging to this port
    QSet<int> removed;
    for (int i = 0; i < (int)d->channels.size(); ++i) {
        if (d->channels[i].portId == portId)
            removed.insert(i);
    }
    if (removed.isEmpty())
        return;

    // drop from graphs
    for (auto &g : d->graphs) {
        QList<int> kept;
        for (int ci : g.channels)
            if (!removed.contains(ci))
                kept.append(ci);
        g.channels = kept;
    }
    // remove empty graphs
    d->graphs.erase(
        std::remove_if(
            d->graphs.begin(),
            d->graphs.end(),
            [](const Graph &g) {
                return g.channels.isEmpty();
            }),
        d->graphs.end());

    // we keep channels in the vector to preserve indices used by other graphs;
    // mark removed ones as disabled and clear their lookup entry so they
    // cannot be re-resolved by ensureChannel.
    for (int ci : removed) {
        auto &c = d->channels[ci];
        d->channelLookup.remove(qMakePair(c.portId, c.colIdx));
        c.enabled = false;
        c.samples = RingBuffer<float>(0);
        c.portId.clear();
        c.signalName.clear();
    }

    Q_EMIT layoutChanged();
}

void PlotCanvas::clearAll()
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->ports.clear();
    d->channels.clear();
    d->channelLookup.clear();
    d->graphs.clear();
    d->nextGraphId = 1;
    Q_EMIT layoutChanged();
}

void PlotCanvas::clearRuntimeData()
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    for (auto &p : d->ports)
        p.timestamps.clear();
    for (auto &c : d->channels)
        c.samples.clear();
}

int PlotCanvas::ensureChannel(const QString &portId, int colIdx, const QString &signalName)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto key = qMakePair(portId, colIdx);
    auto it = d->channelLookup.find(key);
    if (it != d->channelLookup.end()) {
        // refresh signalName if it changed
        if (!signalName.isEmpty() && d->channels[*it].signalName != signalName)
            d->channels[*it].signalName = signalName;
        return *it;
    }

    ChannelData c;
    c.portId = portId;
    c.colIdx = colIdx;
    c.signalName = signalName.isEmpty() ? QStringLiteral("ch%1").arg(colIdx) : signalName;
    c.digital = false;
    c.enabled = true;
    c.samples.setCapacity(d->bufferSize);

    int idx = (int)d->channels.size();
    d->channels.push_back(std::move(c));
    d->channelLookup.insert(key, idx);

    // give it its own default graph if it isn't already in one
    if (graphIdForChannel(idx) == 0) {
        Graph g;
        g.id = d->nextGraphId++;
        g.channels.append(idx);
        d->graphs.push_back(g);
    }

    Q_EMIT layoutChanged();
    return idx;
}

bool PlotCanvas::channelEnabled(int channelIndex) const
{
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return false;
    return d->channels[channelIndex].enabled;
}

void PlotCanvas::setChannelEnabled(int channelIndex, bool enabled)
{
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return;
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto &c = d->channels[channelIndex];
    if (c.enabled == enabled)
        return;
    c.enabled = enabled;
    if (enabled)
        c.samples.setCapacity(d->bufferSize);
    else
        c.samples = RingBuffer<float>(0);
}

void PlotCanvas::setChannelDigital(int channelIndex, bool digital)
{
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return;
    d->channels[channelIndex].digital = digital;
}

void PlotCanvas::appendTimestamps(const QString &portId, const VectorXul &vec)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto it = d->ports.find(portId);
    if (it == d->ports.end())
        return;
    const float div = it->timestampDivisor;
    auto &ts = it->timestamps;
    for (int i = 0; i < vec.rows(); ++i)
        ts.add(vec(i, 0) / div);
}

void PlotCanvas::appendSamplesF(int channelIndex, const Eigen::RowVectorXf &vec)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return;
    auto &c = d->channels[channelIndex];
    if (!c.enabled)
        return;
    for (int i = 0; i < vec.size(); ++i)
        c.samples.add(vec(i));
}

void PlotCanvas::appendSamplesI(int channelIndex, const Eigen::RowVectorXi &vec)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return;
    auto &c = d->channels[channelIndex];
    if (!c.enabled)
        return;
    for (int i = 0; i < vec.size(); ++i)
        c.samples.add(vec(i));
}

int PlotCanvas::graphIdForChannel(int channelIndex) const
{
    for (const auto &g : d->graphs) {
        if (g.channels.contains(channelIndex))
            return g.id;
    }
    return 0;
}

int PlotCanvas::graphIndexById(int graphId) const
{
    for (int i = 0; i < (int)d->graphs.size(); ++i)
        if (d->graphs[i].id == graphId)
            return i;
    return -1;
}

void PlotCanvas::moveChannelToGraph(int channelIndex, int destGraphId)
{
    const int destIdx = graphIndexById(destGraphId);
    if (destIdx < 0)
        return;
    if (d->graphs[destIdx].channels.contains(channelIndex))
        return; // already there

    // remove from current graph
    for (auto it = d->graphs.begin(); it != d->graphs.end();) {
        it->channels.removeAll(channelIndex);
        if (it->channels.isEmpty() && it->id != destGraphId)
            it = d->graphs.erase(it);
        else
            ++it;
    }

    // re-resolve dest index since erase may have shifted
    const int destIdx2 = graphIndexById(destGraphId);
    if (destIdx2 < 0)
        return;
    d->graphs[destIdx2].channels.append(channelIndex);

    Q_EMIT layoutChanged();
}

void PlotCanvas::createGraphWithChannel(int channelIndex)
{
    // remove from any current graph
    for (auto it = d->graphs.begin(); it != d->graphs.end();) {
        it->channels.removeAll(channelIndex);
        if (it->channels.isEmpty())
            it = d->graphs.erase(it);
        else
            ++it;
    }
    Graph g;
    g.id = d->nextGraphId++;
    g.channels.append(channelIndex);
    d->graphs.push_back(g);
    Q_EMIT layoutChanged();
}

void PlotCanvas::resetLayoutOneChannelPerGraph()
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->graphs.clear();
    for (int i = 0; i < (int)d->channels.size(); ++i) {
        if (d->channels[i].portId.isEmpty())
            continue;
        Graph g;
        g.id = d->nextGraphId++;
        g.channels.append(i);
        d->graphs.push_back(g);
    }
    Q_EMIT layoutChanged();
}

int PlotCanvas::channelCount() const
{
    return (int)d->channels.size();
}

PlotCanvas::ChannelInfo PlotCanvas::channelInfo(int channelIndex) const
{
    ChannelInfo info;
    if (channelIndex < 0 || channelIndex >= (int)d->channels.size())
        return info;
    const auto &c = d->channels[channelIndex];
    info.portId = c.portId;
    info.colIdx = c.colIdx;
    info.signalName = c.signalName;
    info.digital = c.digital;
    info.enabled = c.enabled;
    info.graphId = graphIdForChannel(channelIndex);
    return info;
}

QVariantList PlotCanvas::saveChannels() const
{
    QVariantList out;
    for (const auto &c : d->channels) {
        if (c.portId.isEmpty())
            continue;
        QVariantHash h;
        h.insert("port_id", c.portId);
        h.insert("col_idx", c.colIdx);
        h.insert("signal_name", c.signalName);
        h.insert("digital", c.digital);
        h.insert("enabled", c.enabled);
        out.append(h);
    }
    return out;
}

QVariantList PlotCanvas::saveGraphs() const
{
    QVariantList out;
    for (const auto &g : d->graphs) {
        QVariantHash gh;
        gh.insert("id", g.id);
        gh.insert("size_weight", g.sizeWeight);
        QVariantList ch;
        for (int ci : g.channels) {
            if (ci < 0 || ci >= (int)d->channels.size())
                continue;
            const auto &c = d->channels[ci];
            QVariantHash h;
            h.insert("port_id", c.portId);
            h.insert("col_idx", c.colIdx);
            ch.append(h);
        }
        gh.insert("channels", ch);
        out.append(gh);
    }
    return out;
}

void PlotCanvas::loadChannels(const QVariantList &v)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    // Re-create channel entries (must run before loadGraphs so indices match).
    d->channels.clear();
    d->channelLookup.clear();
    for (const auto &item : v) {
        const auto h = item.toHash();
        ChannelData c;
        c.portId = h.value("port_id").toString();
        c.colIdx = h.value("col_idx").toInt();
        c.signalName = h.value("signal_name").toString();
        c.digital = h.value("digital", false).toBool();
        c.enabled = h.value("enabled", true).toBool();
        if (c.portId.isEmpty())
            continue;
        if (c.enabled)
            c.samples.setCapacity(d->bufferSize);
        int idx = (int)d->channels.size();
        d->channels.push_back(std::move(c));
        d->channelLookup.insert(qMakePair(d->channels[idx].portId, d->channels[idx].colIdx), idx);
    }
}

void PlotCanvas::loadGraphs(const QVariantList &v)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    d->graphs.clear();
    int maxId = 0;
    for (const auto &item : v) {
        const auto h = item.toHash();
        Graph g;
        g.id = h.value("id").toInt();
        g.sizeWeight = h.value("size_weight", 1.0f).toFloat();
        if (g.sizeWeight < kMinGraphWeight)
            g.sizeWeight = 1.0f;
        for (const auto &ch : h.value("channels").toList()) {
            const auto chh = ch.toHash();
            const auto key = qMakePair(chh.value("port_id").toString(), chh.value("col_idx").toInt());
            auto it = d->channelLookup.find(key);
            if (it != d->channelLookup.end())
                g.channels.append(*it);
        }
        if (g.channels.isEmpty())
            continue;
        if (g.id > maxId)
            maxId = g.id;
        d->graphs.push_back(g);
    }
    d->nextGraphId = maxId + 1;
    if (d->nextGraphId < 1)
        d->nextGraphId = 1;
    Q_EMIT layoutChanged();
}

void PlotCanvas::initializeGL()
{
    initializeOpenGLFunctions();
    d->qigr = QtImGui::initialize(this, false);
    d->impCtx = ImPlot::CreateContext();

    QObject::connect(d->updateTimer, SIGNAL(timeout()), this, SLOT(update()));
    // Start at the idle rate; setRunning() bumps it up for live captures
    d->updateTimer->setInterval(d->isRunning ? d->runningIntervalMs : 1000 / 30);
    d->updateTimer->start();
}

void PlotCanvas::paintGL()
{
    QtImGui::newFrame(d->qigr);
    ImPlot::SetCurrentContext(d->impCtx);

    static ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                       | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
                                       | ImGuiWindowFlags_NoBringToFrontOnFocus;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::Begin("PlotCanvasMain", nullptr, winFlags);

    ImGui::BeginDisabled(!d->isRunning);
    ImGui::SliderFloat("History Length", &d->historyLen, 0.25, 15, "%.1f s");
    ImGui::EndDisabled();

    bool layoutDirty = false;

    {
        const std::lock_guard<std::mutex> lock(d->dataMutex);

        // Determine current X range from any port's most recent timestamp.
        float tNow = 0.0;
        bool haveTime = false;
        for (const auto &p : d->ports) {
            if (!p.timestamps.isEmpty()) {
                tNow = std::max(tNow, p.timestamps.last());
                haveTime = true;
            }
        }
        if (d->isRunning && haveTime) {
            d->xLinkMax = tNow;
            d->xLinkMin = tNow - d->historyLen;
        } else if (!haveTime) {
            d->xLinkMin = 0.0;
            d->xLinkMax = d->historyLen;
        }

        // Build the list of graphs that have at least one enabled channel.
        // Graphs whose channels are all hidden via "Show" are not rendered at all.
        std::vector<int> visIdx;
        visIdx.reserve(d->graphs.size());
        for (int i = 0; i < (int)d->graphs.size(); ++i) {
            for (int ci : d->graphs[i].channels) {
                if (ci >= 0 && ci < (int)d->channels.size() && d->channels[ci].enabled) {
                    visIdx.push_back(i);
                    break;
                }
            }
        }
        const int nVis = (int)visIdx.size();
        const float availH = ImGui::GetContentRegionAvail().y;
        const float newDropH = (nVis > 0 ? 14.0f : 0.0f);
        const float scrollAreaH = std::max(40.0f, availH - newDropH - 4.0f);
        const float splitterTotal = (nVis > 0 ? (nVis - 1) : 0) * kSplitterHeight;

        float weightSum = 0.0f;
        for (int i : visIdx)
            weightSum += d->graphs[i].sizeWeight;
        if (weightSum <= 0.0f)
            weightSum = 1.0f;

        // Per-graph height = sizeWeight * kMinGraphHeight (weight floored at
        // kMinGraphWeight). The user grows individual graphs by dragging their
        // splitter, which increases that graph's sizeWeight.
        // If the resulting total is shorter than the available area, scale all
        // heights up uniformly so the canvas fills the space (no empty area
        // below the last graph). If the total exceeds the area, the region
        // scrolls and no scaling is applied.
        std::vector<float> heights(nVis, 0.0f);
        float totalHeight = 0.0f;
        for (int idx = 0; idx < nVis; ++idx) {
            float w = d->graphs[visIdx[idx]].sizeWeight;
            if (w < kMinGraphWeight)
                w = kMinGraphWeight;
            const float h = w * kMinGraphHeight;
            heights[idx] = h;
            totalHeight += h;
        }
        const bool scrollable = (totalHeight + splitterTotal) > scrollAreaH;
        if (!scrollable && totalHeight > 0.0f) {
            const float scale = (scrollAreaH - splitterTotal) / totalHeight;
            if (scale > 1.0f) {
                for (auto &h : heights)
                    h *= scale;
            }
        }
        (void)weightSum;

        if (d->graphs.empty()) {
            ImGui::TextDisabled("No channels yet. Connect input ports and start a run.");
        } else if (nVis == 0) {
            ImGui::TextDisabled("All channels are hidden. Tick \"Show\" in the Channels panel to display them.");
        }

        ImGui::BeginChild(
            "graphs_scroll",
            ImVec2(0, scrollAreaH),
            false,
            scrollable ? ImGuiWindowFlags_AlwaysVerticalScrollbar : 0);

        // Tighten ImPlot padding so many small plots fit on screen. The
        // defaults waste a lot of pixels around each axis.
        ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(4, 2));
        ImPlot::PushStyleVar(ImPlotStyleVar_LabelPadding, ImVec2(2, 2));
        ImPlot::PushStyleVar(ImPlotStyleVar_LegendPadding, ImVec2(4, 2));
        ImPlot::PushStyleVar(ImPlotStyleVar_LegendInnerPadding, ImVec2(2, 2));

        for (int idx = 0; idx < nVis; ++idx) {
            const int gi = visIdx[idx];
            auto &g = d->graphs[gi];
            const float h = heights[idx];

            // Title derived from currently enabled channels in the graph.
            QStringList parts;
            int shown = 0;
            for (int ci : g.channels) {
                if (ci < 0 || ci >= (int)d->channels.size())
                    continue;
                if (!d->channels[ci].enabled)
                    continue;
                if (shown++ >= 4) {
                    parts << QStringLiteral("…");
                    break;
                }
                parts << d->channels[ci].signalName;
            }
            const QString title = parts.isEmpty() ? QStringLiteral("(empty)") : parts.join(QStringLiteral(" + "));
            const QByteArray plotIdUtf8 = QStringLiteral("%1##g%2").arg(title).arg(g.id).toUtf8();

            // Y label: from the port of the first enabled channel; "y" if mixed.
            QByteArray yLabelUtf8 = QByteArrayLiteral("y");
            QString firstPort;
            bool mixed = false;
            for (int ci : g.channels) {
                if (ci < 0 || ci >= (int)d->channels.size() || !d->channels[ci].enabled)
                    continue;
                if (firstPort.isEmpty())
                    firstPort = d->channels[ci].portId;
                else if (d->channels[ci].portId != firstPort) {
                    mixed = true;
                    break;
                }
            }
            if (!firstPort.isEmpty() && !mixed) {
                auto pit = d->ports.find(firstPort);
                if (pit != d->ports.end())
                    yLabelUtf8 = pit->yLabel.toUtf8();
            }

            // Hide redundant X tick labels and axis label on every plot except
            // the bottom-most visible one - they all share the same linked X.
            const bool isBottom = (idx == nVis - 1);
            ImPlotAxisFlags xFlags = ImPlotAxisFlags_None;
            if (!isBottom)
                xFlags |= ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoLabel;
            const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText;

            if (ImPlot::BeginPlot(plotIdUtf8.constData(), ImVec2(-1, h), plotFlags)) {
                ImPlot::SetupAxes(
                    isBottom ? "time [s]" : nullptr,
                    yLabelUtf8.constData(),
                    xFlags,
                    ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLinks(ImAxis_X1, &d->xLinkMin, &d->xLinkMax);

                for (int ci : g.channels) {
                    const auto &c = d->channels[ci];
                    if (!c.enabled || c.portId.isEmpty())
                        continue;
                    auto pit = d->ports.find(c.portId);
                    if (pit == d->ports.end())
                        continue;
                    const auto &ts = pit->timestamps;
                    if (ts.isEmpty() || c.samples.size() == 0)
                        continue;

                    const auto label = QStringLiteral("%1##c%2").arg(c.signalName).arg(ci).toUtf8();
                    const int n = (int)std::min(ts.size(), c.samples.size());
                    const int off = ts.offset();
                    const ImVec4 col = colorForChannel(c.signalName, c.colIdx);
                    ImPlot::SetNextLineStyle(col);
                    ImPlot::SetNextFillStyle(col, 0.5f);
                    if (c.digital)
                        ImPlot::PlotDigital(label.constData(), ts.data(), c.samples.data(), n, 0, off);
                    else
                        ImPlot::PlotLine(label.constData(), ts.data(), c.samples.data(), n, 0, off);

                    if (ImPlot::BeginDragDropSourceItem(label.constData())) {
                        int payload = ci;
                        ImGui::SetDragDropPayload("PLOTCANVAS_CHANNEL", &payload, sizeof(int));
                        ImGui::Text("%s", c.signalName.toUtf8().constData());
                        ImPlot::EndDragDropSource();
                    }
                }

                if (ImPlot::BeginDragDropTargetPlot()) {
                    if (auto p = ImGui::AcceptDragDropPayload("PLOTCANVAS_CHANNEL")) {
                        const int ci = *static_cast<const int *>(p->Data);
                        moveChannelToGraph(ci, g.id);
                        layoutDirty = true;
                    }
                    ImPlot::EndDragDropTarget();
                }

                ImPlot::EndPlot();
            }

            // Splitter strip between visible graphs.
            if (idx < nVis - 1) {
                const auto splitId = std::format("##split_{}", g.id);
                ImGui::InvisibleButton(splitId.c_str(), ImVec2(-1, kSplitterHeight));
                if (ImGui::IsItemActive()) {
                    const float dy = ImGui::GetIO().MouseDelta.y;
                    if (dy != 0.0f) {
                        // Grow/shrink only the upper graph; the lower one keeps
                        // its weight. Floor at kMinGraphWeight so it can never
                        // shrink below the base height.
                        auto &gA = d->graphs[visIdx[idx]];
                        const float newA = gA.sizeWeight + (dy / kMinGraphHeight);
                        gA.sizeWeight = std::max(kMinGraphWeight, newA);
                    }
                }
                if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

                // Render the splitter as a thin line over the invisible area.
                ImVec2 minP = ImGui::GetItemRectMin();
                ImVec2 maxP = ImGui::GetItemRectMax();
                const ImU32 col = ImGui::GetColorU32(
                    ImGui::IsItemHovered() || ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : ImGuiCol_Separator);
                const float midY = (minP.y + maxP.y) * 0.5f;
                ImGui::GetWindowDrawList()->AddLine(ImVec2(minP.x, midY), ImVec2(maxP.x, midY), col, 1.0f);
            }
        }

        ImPlot::PopStyleVar(4);
        ImGui::EndChild();

        // "Drop here to create a new graph" zone - kept outside the scroll area
        // so it stays reachable no matter how far the user has scrolled.
        if (nVis > 0) {
            ImGui::InvisibleButton("##new_graph_drop", ImVec2(-1, newDropH));
            ImVec2 minP = ImGui::GetItemRectMin();
            ImVec2 maxP = ImGui::GetItemRectMax();
            const bool dragActive = ImGui::GetDragDropPayload() != nullptr
                                    && ImGui::GetDragDropPayload()->IsDataType("PLOTCANVAS_CHANNEL");
            const ImU32 col = ImGui::GetColorU32(dragActive ? ImGuiCol_DragDropTarget : ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRect(minP, maxP, col, 2.0f, 0, dragActive ? 2.0f : 1.0f);
            if (dragActive) {
                ImVec2 ts = ImGui::CalcTextSize("Drop to create a new graph");
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2((minP.x + maxP.x) * 0.5f - ts.x * 0.5f, (minP.y + maxP.y) * 0.5f - ts.y * 0.5f),
                    col,
                    "Drop to create a new graph");
            }
            if (ImGui::BeginDragDropTarget()) {
                if (auto *p = ImGui::AcceptDragDropPayload("PLOTCANVAS_CHANNEL")) {
                    const int ci = *static_cast<const int *>(p->Data);
                    createGraphWithChannel(ci);
                    layoutDirty = true;
                }
                ImGui::EndDragDropTarget();
            }
        }
    } // end lock

    ImGui::End();

    glViewport(0, 0, width(), height());
    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    QtImGui::render(d->qigr);

    if (layoutDirty)
        Q_EMIT layoutChanged();
}
