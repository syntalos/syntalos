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
#include <algorithm>
#include <cstring>
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

    void add(const T *src, size_t n)
    {
        if (n == 0 || m_capacity == 0)
            return;
        if (n >= m_capacity) {
            src += (n - m_capacity);
            n = m_capacity;
        }
        const size_t tail = m_capacity - m_head;
        if (n <= tail) {
            std::memcpy(m_data.data() + m_head, src, n * sizeof(T));
        } else {
            std::memcpy(m_data.data() + m_head, src, tail * sizeof(T));
            std::memcpy(m_data.data(), src + tail, (n - tail) * sizeof(T));
        }
        m_head = (m_head + n) % m_capacity;
        m_count = std::min(m_count + n, m_capacity);
    }

    // Copy logical elements [from, from+len) in oldest-first order into dst.
    void copyRange(size_t from, size_t len, T *dst) const
    {
        if (len == 0 || from >= m_count)
            return;
        len = std::min(len, m_count - from);
        const size_t off = (m_count == m_capacity) ? m_head : 0;
        const size_t start = (off + from) % m_capacity;
        const size_t part1 = std::min(len, m_capacity - start);
        std::memcpy(dst, m_data.data() + start, part1 * sizeof(T));
        if (len > part1)
            std::memcpy(dst + part1, m_data.data(), (len - part1) * sizeof(T));
    }

    // First logical index where value >= threshold (ring must be sorted ascending).
    size_t lowerBoundLogical(T threshold, size_t startIdx = 0) const
    {
        if (m_count == 0 || startIdx >= m_count)
            return startIdx;
        const size_t off = (m_count == m_capacity) ? m_head : 0;
        size_t lo = startIdx, hi = m_count;
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (m_data[(off + mid) % m_capacity] < threshold)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
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

    // Affine transform applied to incoming samples before display:
    //   displayed = dataScale * raw + dataOffset
    float dataScale = 1.0f;
    float dataOffset = 0.0f;

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
 * Get a deterministic series color from a channel's stable identity: its signal
 * name when it has one, or its column index otherwise.
 */
static ImVec4 colorForChannel(const QString &name, int colIdx)
{
    const uint h = name.isEmpty() ? qHash(colIdx) : qHash(name);
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

    // Ingest scratch buffers - allocated once, reused per block (ingest thread)
    std::vector<float> tsConvBuf;
    std::vector<float> sampleConvBuf;

    // Per-frame render snapshot built under brief lock, rendered without lock
    struct PortSnap {
        QString yLabel;
        size_t visFrom = 0;    // logical ring index where visible window starts
        std::vector<float> ts; // timestamps[visFrom .. end)
    };
    struct ChannelSnap {
        int channelIdx = 0;
        QString portId;
        QString signalName;
        int colIdx = 0;
        bool digital = false;
        size_t tsOffset = 0; // offset into portSnaps[portId].ts for this channel
        size_t visLen = 0;
        std::vector<float> samples;
    };
    struct GraphSnap {
        int id = 0;
        int origIdx = 0; // index into d->graphs (for splitter drag)
        float sizeWeight = 1.0f;
        std::vector<int> chanSnaps; // indices into channelSnaps
    };
    QHash<QString, PortSnap> portSnaps;
    std::vector<ChannelSnap> channelSnaps;
    std::vector<GraphSnap> graphSnaps;

    // Decimation scratch (reused each frame, overwritten per channel)
    std::vector<float> decTs;
    std::vector<float> decSamples;
    float lastFrameWidth = 800.0f;
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

float PlotCanvas::historyLength() const
{
    return d->historyLen;
}

void PlotCanvas::setHistoryLength(float seconds)
{
    if (seconds < 0.25f)
        seconds = 0.25f;
    if (seconds > 15.0f)
        seconds = 15.0f;
    d->historyLen = seconds;
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

void PlotCanvas::registerPort(
    const QString &portId,
    double timestampDivisor,
    const QString &yLabel,
    double dataScale,
    double dataOffset)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto &p = d->ports[portId];
    p.id = portId;
    p.timestampDivisor = timestampDivisor;
    p.yLabel = yLabel.isEmpty() ? QStringLiteral("y") : yLabel;
    p.dataScale = static_cast<float>(dataScale);
    p.dataOffset = static_cast<float>(dataOffset);
    if (p.timestamps.size() == 0)
        p.timestamps.setCapacity(d->bufferSize);
}

void PlotCanvas::tombstoneChannels(const QSet<int> &removed)
{
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
        // Only drop the lookup entry if it still points at this channel.
        // updatePortChannels() may have reassigned (portId, colIdx) to a surviving
        // channel after column positions shifted; we must not clobber that entry.
        const auto key = qMakePair(c.portId, c.colIdx);
        const auto it = d->channelLookup.find(key);
        if (it != d->channelLookup.end() && it.value() == ci)
            d->channelLookup.erase(it);
        c.enabled = false;
        c.samples = RingBuffer<float>(0);
        c.portId.clear();
        c.signalName.clear();
    }
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

    tombstoneChannels(removed);
    Q_EMIT layoutChanged();
}

int PlotCanvas::appendChannel(const QString &portId, int colIdx, const QString &signalName)
{
    // caller must hold d->dataMutex. Creates a fresh channel in its own default
    // graph, registers its lookup entry, and returns its index. Does not emit
    // layoutChanged (callers do so once they are done mutating).
    ChannelData c;
    c.portId = portId;
    c.colIdx = colIdx;
    c.signalName = signalName.isEmpty() ? QStringLiteral("ch%1").arg(colIdx) : signalName;
    c.samples.setCapacity(d->bufferSize);

    const int idx = (int)d->channels.size();
    d->channels.push_back(std::move(c));
    d->channelLookup.insert(qMakePair(portId, colIdx), idx);

    Graph g;
    g.id = d->nextGraphId++;
    g.channels.append(idx);
    d->graphs.push_back(g);

    return idx;
}

void PlotCanvas::updatePortChannels(const QString &portId, const QStringList &signalNames)
{
    const std::lock_guard<std::mutex> lock(d->dataMutex);

    // Channel identity within a port is the signal name:
    // the source may add or remove signals, shifting the remaining ones' columns.
    // Matching the new signal list to existing channels by name lets user settings
    // (visibility, digital flag, graph assignment) follow a signal across such
    // shifts; signals no longer present are removed.

    // Index this port's live channels by name and drop their (portId, colIdx)
    // lookup entries, as columns are about to be reassigned. Tombstoned channels
    // have an empty portId and are naturally excluded.
    QHash<QString, int> existingByName;
    QList<int> portChannels;
    for (int i = 0; i < (int)d->channels.size(); ++i) {
        const auto &c = d->channels[i];
        if (c.portId != portId)
            continue;
        existingByName.insert(c.signalName, i);
        portChannels.append(i);
        d->channelLookup.remove(qMakePair(portId, c.colIdx));
    }

    QSet<int> reused;
    int colIdx = 0;
    for (const QString &name : signalNames) {
        auto it = existingByName.find(name);
        if (it != existingByName.end()) {
            // existing signal: keep it (and its settings), move it to the new column
            const int idx = it.value();
            existingByName.erase(it); // consume, so duplicate names create new channels
            d->channels[idx].colIdx = colIdx;
            d->channelLookup.insert(qMakePair(portId, colIdx), idx);
            reused.insert(idx);
        } else {
            // new signal: fresh channel in its own graph
            appendChannel(portId, colIdx, name);
        }
        ++colIdx;
    }

    // Channels not reused have vanished from the source: remove them.
    QSet<int> removed;
    for (int i : portChannels)
        if (!reused.contains(i))
            removed.insert(i);
    if (!removed.isEmpty())
        tombstoneChannels(removed);

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
    auto it = d->channelLookup.find(qMakePair(portId, colIdx));
    if (it != d->channelLookup.end()) {
        // refresh signalName if it changed
        if (!signalName.isEmpty() && d->channels[*it].signalName != signalName)
            d->channels[*it].signalName = signalName;
        return *it;
    }

    const int idx = appendChannel(portId, colIdx, signalName);
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

void PlotCanvas::appendBlockF(
    const QString &portId,
    const VectorXu64 &timestamps,
    const Eigen::Ref<const MatrixXf> &data,
    const int *channelIdx,
    int nCols)
{
    const int n = (int)timestamps.rows();
    if (n == 0)
        return;
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto it = d->ports.find(portId);
    if (it == d->ports.end())
        return;

    const float div = (float)it->timestampDivisor;
    d->tsConvBuf.resize(n);
    for (int i = 0; i < n; ++i)
        d->tsConvBuf[i] = (float)timestamps(i) / div;
    it->timestamps.add(d->tsConvBuf.data(), n);

    const float scale = it->dataScale;
    const float offset = it->dataOffset;
    d->sampleConvBuf.resize(n);
    for (int c = 0; c < nCols; ++c) {
        const int ci = channelIdx[c];
        if (ci < 0 || ci >= (int)d->channels.size())
            continue;
        auto &ch = d->channels[ci];
        if (!ch.enabled)
            continue;
        for (int i = 0; i < n; ++i)
            d->sampleConvBuf[i] = scale * data(i, c) + offset;
        ch.samples.add(d->sampleConvBuf.data(), n);
    }
}

void PlotCanvas::appendBlockI(
    const QString &portId,
    const VectorXu64 &timestamps,
    const Eigen::Ref<const MatrixXi32> &data,
    const int *channelIdx,
    int nCols)
{
    const int n = (int)timestamps.rows();
    if (n == 0)
        return;
    const std::lock_guard<std::mutex> lock(d->dataMutex);
    auto it = d->ports.find(portId);
    if (it == d->ports.end())
        return;

    const float div = (float)it->timestampDivisor;
    d->tsConvBuf.resize(n);
    for (int i = 0; i < n; ++i)
        d->tsConvBuf[i] = (float)timestamps(i) / div;
    it->timestamps.add(d->tsConvBuf.data(), n);

    const float scale = it->dataScale;
    const float offset = it->dataOffset;
    d->sampleConvBuf.resize(n);
    for (int c = 0; c < nCols; ++c) {
        const int ci = channelIdx[c];
        if (ci < 0 || ci >= (int)d->channels.size())
            continue;
        auto &ch = d->channels[ci];
        if (!ch.enabled)
            continue;
        for (int i = 0; i < n; ++i)
            d->sampleConvBuf[i] = scale * (float)data(i, c) + offset;
        ch.samples.add(d->sampleConvBuf.data(), n);
    }
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

// Build a min/max envelope: for each of nBins time-ordered bins across [ts,samples)(length n),
// emit the sample at the min and the sample at the max (1 or 2 points per bin, time-ordered).
// Reduces rendering cost while preserving spike visibility.
static void decimateMinMax(
    const float *ts,
    const float *samples,
    size_t n,
    std::vector<float> &outTs,
    std::vector<float> &outSamples,
    size_t nBins)
{
    outTs.clear();
    outSamples.clear();
    if (n == 0 || nBins == 0)
        return;
    outTs.reserve(nBins * 2);
    outSamples.reserve(nBins * 2);

    const double step = (double)n / nBins;
    for (size_t b = 0; b < nBins; ++b) {
        const size_t i0 = (size_t)(b * step);
        const size_t i1 = std::min((size_t)((b + 1) * step + 0.5), n);
        if (i0 >= i1)
            continue;

        float vMin = samples[i0], vMax = samples[i0];
        size_t iMin = i0, iMax = i0;
        for (size_t i = i0 + 1; i < i1; ++i) {
            if (samples[i] < vMin) {
                vMin = samples[i];
                iMin = i;
            }
            if (samples[i] > vMax) {
                vMax = samples[i];
                iMax = i;
            }
        }

        if (iMin == iMax) {
            outTs.push_back(ts[iMin]);
            outSamples.push_back(vMin);
        } else if (iMin < iMax) {
            outTs.push_back(ts[iMin]);
            outSamples.push_back(vMin);
            outTs.push_back(ts[iMax]);
            outSamples.push_back(vMax);
        } else {
            outTs.push_back(ts[iMax]);
            outSamples.push_back(vMax);
            outTs.push_back(ts[iMin]);
            outSamples.push_back(vMin);
        }
    }
}

/**
 * Mirrors ImPlot's NiceNum (vendor/implot/implot.cpp). Kept local so we can
 * derive a tick list identical in spirit to the default locator but shared
 * across all stacked plots.
 */
static double niceNum(double x, bool round)
{
    if (x <= 0.0)
        return 0.0;
    const int expv = (int)std::floor(std::log10(x));
    const double f = x / std::pow(10.0, (double)expv);
    double nf;
    if (round) {
        if (f < 1.5)
            nf = 1;
        else if (f < 3)
            nf = 2;
        else if (f < 7)
            nf = 5;
        else
            nf = 10;
    } else {
        if (f <= 1)
            nf = 1;
        else if (f <= 2)
            nf = 2;
        else if (f <= 5)
            nf = 5;
        else
            nf = 10;
    }
    return nf * std::pow(10.0, (double)expv);
}

/**
 * Compute major tick positions for the time axis in the same way ImPlot's
 * default locator does, but from a single reference pixel width so every
 * stacked plot gets identical ticks (and therefore aligned gridlines).
 */
static void computeTimeMajorTicks(double xMin, double xMax, float refPixels, std::vector<double> &out)
{
    out.clear();
    if (!(xMax > xMin))
        return;
    const int nMajor = std::max(2, (int)std::lround(refPixels / 400.0f));
    const double niceRange = niceNum((xMax - xMin) * 0.99, false);
    const double interval = niceNum(niceRange / (nMajor - 1), true);
    if (!(interval > 0.0))
        return;
    const double graphMin = std::floor(xMin / interval) * interval;
    const double graphMax = std::ceil(xMax / interval) * interval;
    out.reserve((size_t)((graphMax - graphMin) / interval) + 2);
    for (double major = graphMin; major < graphMax + 0.5 * interval; major += interval) {
        if (major >= xMin && major <= xMax)
            out.push_back(major);
    }
}

/**
 * Fixed-width y-axis tick formatter. ImGui's default font renders digits and
 * space with the same advance width (tabular figures), so right-aligning every
 * tick label into the same character count makes the y-axis gutter — and thus
 * each plot's inner rectangle left edge - identical across stacked plots.
 * Without this, plots with different auto-fit y-ranges end up with different
 * label widths, shifting their inner rects horizontally and misaligning the
 * shared x-axis gridlines.
 */
static int yTickFormatter(double value, char *buff, int size, void * /*user*/)
{
    char tmp[64];
    const int n = std::snprintf(tmp, sizeof(tmp), "%.4g", value);
    constexpr int width = 7;
    if (n >= width)
        return std::snprintf(buff, size, "%s", tmp);
    return std::snprintf(buff, size, "%*s", width, tmp);
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

    // Snapshot phase (mutex held only for the memcpy pass)
    {
        const std::lock_guard<std::mutex> lock(d->dataMutex);

        float tNow = 0.0f;
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
        const auto tMin = (float)d->xLinkMin;

        // Per-port: copy the visible timestamp window into portSnaps.
        d->portSnaps.clear();
        for (auto it = d->ports.begin(); it != d->ports.end(); ++it) {
            const auto &pd = it.value();
            const size_t tsTotal = pd.timestamps.size();
            if (tsTotal == 0)
                continue;
            Private::PortSnap ps;
            ps.yLabel = pd.yLabel;
            ps.visFrom = pd.timestamps.lowerBoundLogical(tMin);
            const size_t visLen = tsTotal - ps.visFrom;
            ps.ts.resize(visLen);
            if (visLen > 0)
                pd.timestamps.copyRange(ps.visFrom, visLen, ps.ts.data());
            d->portSnaps.insert(it.key(), std::move(ps));
        }

        // Per-channel: copy matching samples, aligned to the port's ts window.
        d->channelSnaps.clear();
        for (int ci = 0; ci < (int)d->channels.size(); ++ci) {
            const auto &c = d->channels[ci];
            if (!c.enabled || c.portId.isEmpty())
                continue;
            auto psIt = d->portSnaps.find(c.portId);
            if (psIt == d->portSnaps.end())
                continue;
            const auto &ps = psIt.value();

            // tsTotal - ring's current count; reconstruct from snapshot fields
            const size_t tsTotal = ps.visFrom + ps.ts.size();
            const size_t sampTotal = c.samples.size();
            // Samples correspond to the last sampTotal entries of the ts ring.
            const size_t tsStart = tsTotal >= sampTotal ? tsTotal - sampTotal : 0;
            const size_t adjVisFrom = std::max(ps.visFrom, tsStart);
            if (adjVisFrom >= tsTotal)
                continue;

            Private::ChannelSnap cs;
            cs.channelIdx = ci;
            cs.portId = c.portId;
            cs.signalName = c.signalName;
            cs.colIdx = c.colIdx;
            cs.digital = c.digital;
            cs.tsOffset = adjVisFrom - ps.visFrom;
            cs.visLen = tsTotal - adjVisFrom;
            const size_t sampFrom = adjVisFrom - tsStart;
            cs.samples.resize(cs.visLen);
            c.samples.copyRange(sampFrom, cs.visLen, cs.samples.data());
            d->channelSnaps.push_back(std::move(cs));
        }

        // Map original channel index -> channelSnaps index.
        QHash<int, int> chanToSnap;
        chanToSnap.reserve((int)d->channelSnaps.size());
        for (int i = 0; i < (int)d->channelSnaps.size(); ++i)
            chanToSnap.insert(d->channelSnaps[i].channelIdx, i);

        // Snapshot graph layout (metadata only; sample data is in channelSnaps).
        d->graphSnaps.clear();
        for (int gi = 0; gi < (int)d->graphs.size(); ++gi) {
            const auto &g = d->graphs[gi];
            Private::GraphSnap gs;
            gs.id = g.id;
            gs.origIdx = gi;
            gs.sizeWeight = g.sizeWeight;
            for (int ch : g.channels) {
                auto it2 = chanToSnap.find(ch);
                if (it2 != chanToSnap.end())
                    gs.chanSnaps.push_back(*it2);
            }
            d->graphSnaps.push_back(std::move(gs));
        }
    }

    // Mutex released; render from snapshots
    std::vector<int> visIdx;
    visIdx.reserve(d->graphSnaps.size());
    for (int i = 0; i < (int)d->graphSnaps.size(); ++i) {
        if (!d->graphSnaps[i].chanSnaps.empty())
            visIdx.push_back(i);
    }
    const int nVis = (int)visIdx.size();
    const float availH = ImGui::GetContentRegionAvail().y;
    const float newDropH = (nVis > 0 ? 14.0f : 0.0f);
    const float scrollAreaH = std::max(40.0f, availH - newDropH - 4.0f);
    const float splitterTotal = (nVis > 0 ? (nVis - 1) : 0) * kSplitterHeight;

    std::vector<float> heights(nVis, 0.0f);
    float totalHeight = 0.0f;
    for (int idx = 0; idx < nVis; ++idx) {
        float w = d->graphSnaps[visIdx[idx]].sizeWeight;
        if (w < kMinGraphWeight)
            w = kMinGraphWeight;
        heights[idx] = w * kMinGraphHeight;
        totalHeight += heights[idx];
    }
    const bool scrollable = (totalHeight + splitterTotal) > scrollAreaH;
    if (!scrollable && totalHeight > 0.0f) {
        const float scale = (scrollAreaH - splitterTotal) / totalHeight;
        if (scale > 1.0f) {
            for (auto &h : heights)
                h *= scale;
        }
    }

    if (d->graphSnaps.empty()) {
        ImGui::TextDisabled("No channels yet. Connect input ports and start a run.");
    } else if (nVis == 0) {
        ImGui::TextDisabled("All channels are hidden. Tick \"Show\" in the Channels panel to display them.");
    }

    ImGui::BeginChild(
        "graphs_scroll",
        ImVec2(0, scrollAreaH),
        false,
        scrollable ? ImGuiWindowFlags_AlwaysVerticalScrollbar : 0);

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(4, 2));
    ImPlot::PushStyleVar(ImPlotStyleVar_LabelPadding, ImVec2(2, 2));
    ImPlot::PushStyleVar(ImPlotStyleVar_LegendPadding, ImVec2(4, 2));
    ImPlot::PushStyleVar(ImPlotStyleVar_LegendInnerPadding, ImVec2(2, 2));

    // Compute shared x-axis ticks once per frame so that every stacked plot
    // draws gridlines at identical x positions. Without this, ImPlot's default
    // per-plot locator picks slightly different intervals when the inner plot
    // width varies (different y-label widths, bottom-axis label space, etc.).
    const float refPlotW = std::max(64.0f, d->lastFrameWidth);
    std::vector<double> xMajors;
    computeTimeMajorTicks(d->xLinkMin, d->xLinkMax, refPlotW, xMajors);

    for (int idx = 0; idx < nVis; ++idx) {
        auto &gs = d->graphSnaps[visIdx[idx]];
        const float h = heights[idx];

        QStringList parts;
        int shown = 0;
        for (int csi : gs.chanSnaps) {
            if (shown++ >= 4) {
                parts << QStringLiteral("…");
                break;
            }
            parts << d->channelSnaps[csi].signalName;
        }
        const QString title = parts.isEmpty() ? QStringLiteral("(empty)") : parts.join(QStringLiteral(" + "));
        const QByteArray plotIdUtf8 = QStringLiteral("%1##g%2").arg(title).arg(gs.id).toUtf8();

        QByteArray yLabelUtf8 = QByteArrayLiteral("y");
        if (!gs.chanSnaps.empty()) {
            const auto &firstCs = d->channelSnaps[gs.chanSnaps[0]];
            auto psIt = d->portSnaps.find(firstCs.portId);
            if (psIt != d->portSnaps.end())
                yLabelUtf8 = psIt->yLabel.toUtf8();
        }

        const bool isBottom = (idx == nVis - 1);
        ImPlotAxisFlags xFlags = ImPlotAxisFlags_None;
        if (!isBottom)
            xFlags |= ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoLabel;
        const ImPlotFlags plotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText;

        if (ImPlot::BeginPlot(plotIdUtf8.constData(), ImVec2(-1, h), plotFlags)) {
            ImPlot::SetupAxes(isBottom ? "time [s]" : nullptr, yLabelUtf8.constData(), xFlags, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLinks(ImAxis_X1, &d->xLinkMin, &d->xLinkMax);
            if (!xMajors.empty())
                ImPlot::SetupAxisTicks(ImAxis_X1, xMajors.data(), (int)xMajors.size(), nullptr, false);
            ImPlot::SetupAxisFormat(ImAxis_Y1, yTickFormatter, nullptr);

            // Cache the plot pixel width for the decimation threshold next frame.
            const float pw = ImPlot::GetPlotSize().x;
            if (pw > 1.0f)
                d->lastFrameWidth = pw;

            for (int csi : gs.chanSnaps) {
                const auto &cs = d->channelSnaps[csi];
                auto psIt = d->portSnaps.find(cs.portId);
                if (psIt == d->portSnaps.end() || cs.visLen == 0)
                    continue;
                const auto &ps = psIt.value();

                const float *tsPtr = ps.ts.data() + cs.tsOffset;
                const float *sampPtr = cs.samples.data();
                int n = (int)cs.visLen;

                // Min/max envelope decimation: reduce to at most 2 x pixelWidth
                // points so rendering cost is proportional to display pixels.
                // Digital channels are skipped - their step edges must not be lost.
                if (!cs.digital) {
                    const int pixW = (int)std::max(d->lastFrameWidth, 64.0f);
                    if (n > pixW * 4) {
                        decimateMinMax(tsPtr, sampPtr, n, d->decTs, d->decSamples, pixW * 2);
                        tsPtr = d->decTs.data();
                        sampPtr = d->decSamples.data();
                        n = (int)d->decTs.size();
                    }
                }

                const auto label = QStringLiteral("%1##c%2").arg(cs.signalName).arg(cs.channelIdx).toUtf8();
                const ImVec4 col = colorForChannel(cs.signalName, cs.colIdx);
                ImPlot::SetNextLineStyle(col);
                ImPlot::SetNextFillStyle(col, 0.5f);
                if (cs.digital)
                    ImPlot::PlotDigital(label.constData(), tsPtr, sampPtr, n);
                else
                    ImPlot::PlotLine(label.constData(), tsPtr, sampPtr, n);

                if (ImPlot::BeginDragDropSourceItem(label.constData())) {
                    int payload = cs.channelIdx;
                    ImGui::SetDragDropPayload("PLOTCANVAS_CHANNEL", &payload, sizeof(int));
                    ImGui::Text("%s", cs.signalName.toUtf8().constData());
                    ImPlot::EndDragDropSource();
                }
            }

            if (ImPlot::BeginDragDropTargetPlot()) {
                if (auto p = ImGui::AcceptDragDropPayload("PLOTCANVAS_CHANNEL")) {
                    const int ci = *static_cast<const int *>(p->Data);
                    moveChannelToGraph(ci, gs.id);
                    layoutDirty = true;
                }
                ImPlot::EndDragDropTarget();
            }

            ImPlot::EndPlot();
        }

        if (idx < nVis - 1) {
            const auto splitId = std::format("##split_{}", gs.id);
            ImGui::InvisibleButton(splitId.c_str(), ImVec2(-1, kSplitterHeight));
            if (ImGui::IsItemActive()) {
                const float dy = ImGui::GetIO().MouseDelta.y;
                if (dy != 0.0f) {
                    auto &gA = d->graphs[gs.origIdx];
                    gA.sizeWeight = std::max(kMinGraphWeight, gA.sizeWeight + (dy / kMinGraphHeight));
                    gs.sizeWeight = gA.sizeWeight;
                }
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

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

    // "Drop here to create a new graph" zone.
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

    ImGui::End();

    glViewport(0, 0, width(), height());
    glClearColor(d->clearColor.x, d->clearColor.y, d->clearColor.z, d->clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::Render();
    QtImGui::render(d->qigr);

    if (layoutDirty)
        Q_EMIT layoutChanged();
}
