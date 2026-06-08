/*
 * Copyright (C) 2019-2025 Matthias Klumpp <matthias@tenstral.net>
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

#include "canvasmodule.h"

#include <QTime>

#include <algorithm>
#include <cmath>

#include "canvaswindow.h"
#include "datactl/frametype.h"
#include "utils/misc.h"

SYNTALOS_MODULE(CanvasModule)

class CanvasModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<Frame>> m_framesIn;
    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlIn;

    std::shared_ptr<StreamSubscription<Frame>> m_frameSub;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;

    CanvasWindow *m_cvView;

    static constexpr double DISPLAY_EMA_ALPHA = 0.05; // EMA smoothing factor for FPS updates for displayed frames
    static constexpr double STREAM_EMA_ALPHA = 0.5;   // EMA smoothing factor for FPS updates for streamed frames

    // Framerate tracking for display
    double m_targetDisplayFps{}; // display rate we expect (monitor refresh, capped by source rate)
    double m_currentDisplayFps{};
    symaster_timepoint m_lastDisplayTime;
    double m_avgDisplayTimeMs{}; // Moving average of display intervals in ms

    // Stream FPS tracking
    symaster_timepoint m_lastStreamCalcTime;
    uint32_t m_streamedFramesSinceLastCalc{};
    double m_expectedFps{};
    double m_currentFpsEma{}; // Exponential moving average of stream FPS

    // Status text update optimization
    QString m_cachedStatusText;
    symaster_timepoint m_lastStatusUpdateTime;
    static constexpr double STATUS_UPDATE_INTERVAL_MS = 125.0; // Update status text only every 125ms

    bool m_active{};
    bool m_paused{};

public:
    explicit CanvasModule(CanvasModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_ctlIn = registerInputPort<ControlCommand>(QStringLiteral("control"), QStringLiteral("Control"));

        m_cvView = new CanvasWindow(m_log);
        addDisplayWindow(m_cvView);
        m_cvView->setWindowIcon(modInfo->icon());
    }

    bool initialize() override
    {
        m_cvView->setLogger(m_log);
        return AbstractModule::initialize();
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::CALL_UI_EVENTS | ModuleFeature::SHOW_DISPLAY;
    }

    bool prepare(const TestSubject &) override
    {
        m_frameSub.reset();
        m_ctlSub.reset();
        if (m_framesIn->hasSubscription())
            m_frameSub = m_framesIn->subscription();
        if (m_ctlIn->hasSubscription())
            m_ctlSub = m_ctlIn->subscription();

        // set some default values, these will be overridden immediately
        // with real values once we are displaying an image
        m_lastDisplayTime = currentTimePoint();
        m_lastStatusUpdateTime = currentTimePoint();
        m_currentDisplayFps = 60.0;
        m_paused = false;

        return true;
    }

    void start() override
    {
        if (m_frameSub == nullptr) {
            m_active = false;
            return;
        }
        m_active = true;

        // the source's true frame rate (0 means "unknown / static images")
        m_expectedFps = m_frameSub->metadataValue<double>("framerate", 60);

        // initialize stream FPS tracking
        m_lastStreamCalcTime = currentTimePoint();
        m_streamedFramesSinceLastCalc = 0;
        m_currentFpsEma = -1; // Signal that we begin a new measurement

        // We pace the display to the monitor's refresh rate, so 120 Hz screens get
        // 120 fps while 60 Hz screens are not over-served, but never faster than the
        // source actually produces frames.
        const double refreshRate = m_cvView->displayRefreshRate();
        m_targetDisplayFps = (m_expectedFps > 0) ? std::min(m_expectedFps, refreshRate) : refreshRate;
        m_currentDisplayFps = m_targetDisplayFps;
        m_avgDisplayTimeMs = 1000.0 / m_targetDisplayFps;

        // Stable, coarse cap on the producer to trim extreme upstream rates and avoid
        // pointless enqueue churn. The precise rate limiting is done on our side by
        // draining the queue down to the most recent frame on every pass, so this
        // value never needs to adapt - the headroom just keeps a fresh frame ready
        // for each display tick.
        m_frameSub->setThrottleItemsPerSec(static_cast<uint>(std::ceil(m_targetDisplayFps * 2)));

        auto imgWinTitle = qstr(m_frameSub->metadataValue<std::string>(CommonMetadataKey::SrcModName, {}));
        if (imgWinTitle.isEmpty())
            imgWinTitle = "Canvas";
        const auto portTitle = qstr(m_frameSub->metadataValue<std::string>(CommonMetadataKey::SrcModPortTitle, {}));
        if (!portTitle.isEmpty())
            imgWinTitle = QStringLiteral("%1 - %2").arg(imgWinTitle, portTitle);
        m_cvView->setWindowTitle(imgWinTitle);
    }

    void processUiEvents() override
    {
        if (!m_active)
            return;

        if (m_ctlSub) {
            auto maybeCtl = m_ctlSub->peekNext();
            if (maybeCtl.has_value()) {
                const auto &ctlValue = *maybeCtl;
                m_paused = ctlValue.kind == ControlCommandKind::STOP || ctlValue.kind == ControlCommandKind::PAUSE;
            }

            if (m_paused) {
                // keep the queue empty while paused, so we don't accumulate a backlog
                m_frameSub->clearPending();
                return;
            }
        }

        // Drain the whole queue, keeping only the most recent frame. This exists
        // so we never build up a backlog: no matter how fast the source runs,
        // the queue is emptied on every pass and we only ever display the freshest
        // frame. Every drained frame still counts towards the measured stream rate.
        std::optional<Frame> latest;
        uint drainedCount = 0;
        while (auto maybeFrame = m_frameSub->peekNext()) {
            latest = std::move(maybeFrame);
            ++drainedCount;
        }

        // Count ALL frames that arrived from the source: the ones we just drained
        // plus the ones the producer-side throttle already dropped. This gives the
        // true source rate.
        const auto skippedFrames = m_frameSub->retrieveApproxSkippedElements();
        m_streamedFramesSinceLastCalc += skippedFrames + drainedCount;

        if (!latest.has_value())
            return;

        // Show the freshest frame. We deliberately do NOT add a software rate gate
        // here: the display is already paced by the compositor's vsync. showImage()
        // calls update(), which coalesces into a single repaint whose buffer swap
        // blocks until the next vertical refresh.
        // Paints land at the monitor's refresh rate no matter how often the
        // engine loop wakes us.
        const auto timeNow = currentTimePoint();
        const auto &frame = *latest;
        m_cvView->showImage(frame.mat);
        const auto frameTimeUsec = frame.time.count();

        if (m_expectedFps == 0) {
            m_cvView->setStatusText(
                QTime::fromMSecsSinceStartOfDay(static_cast<int>(frameTimeUsec / 1000.0)).toString("hh:mm:ss.zzz"));

            // skip all further calculation, we don't know a framerate and are probably not displaying a video,
            // but just static images instead.
            return;
        }

        if (m_currentFpsEma < 0) {
            // The run has (very likely) just started!
            // We need to set the timepoint right here for accurate measurements of initial framerates
            m_lastStreamCalcTime = timeNow;
            m_currentFpsEma = m_expectedFps;
        }

        // calculate display FPS from actual display intervals
        const double displayIntervalMs = timeDiffMsec(timeNow, m_lastDisplayTime).count();
        if (displayIntervalMs > 0) {
            m_avgDisplayTimeMs = (DISPLAY_EMA_ALPHA * displayIntervalMs)
                                 + ((1 - DISPLAY_EMA_ALPHA) * m_avgDisplayTimeMs);
            m_currentDisplayFps = 1000.0 / m_avgDisplayTimeMs;
        }

        // stream FPS calculation
        if (m_streamedFramesSinceLastCalc >= 24) {
            const double streamElapsedSec = timeDiffMsec(timeNow, m_lastStreamCalcTime).count() / 1000.0;
            const double streamRate = static_cast<double>(m_streamedFramesSinceLastCalc) / streamElapsedSec;

            // EMA smoothing
            m_currentFpsEma = (STREAM_EMA_ALPHA * streamRate) + ((1 - STREAM_EMA_ALPHA) * m_currentFpsEma);

            // reset stream counters and timing
            m_streamedFramesSinceLastCalc = 0;
            m_lastStreamCalcTime = timeNow;
        }

        // update last time we calculated speeds and displayed a frame
        m_lastDisplayTime = timeNow;

        // update status text at a reduced rate (nicer for humans, and more performant)
        const double timeSinceLastUpdate = timeDiffMsec(timeNow, m_lastStatusUpdateTime).count();
        if (timeSinceLastUpdate >= STATUS_UPDATE_INTERVAL_MS) {
            // Format status text efficiently using QStringLiteral for compile-time string construction
            m_cachedStatusText = QStringLiteral("%1 | Stream: %2fps (of %3fps) | Display: %4fps")
                                     .arg(
                                         QTime::fromMSecsSinceStartOfDay(static_cast<int>(frameTimeUsec / 1000.0))
                                             .toString(QStringLiteral("hh:mm:ss.zzz")))
                                     .arg(m_currentFpsEma, 0, 'f', 1)
                                     .arg(m_expectedFps, 0, 'f', 1)
                                     .arg(m_currentDisplayFps, 0, 'f', 0);

            m_cvView->setStatusText(m_cachedStatusText);

            m_lastStatusUpdateTime = timeNow;
        }
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("highlight_saturation", m_cvView->highlightSaturation());
        settings.insert("histogram_visible", m_cvView->histogramVisible());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_cvView->setHighlightSaturation(settings.value("highlight_saturation", false).toBool());
        m_cvView->setHistogramVisible(settings.value("histogram_visible", false).toBool());

        return true;
    }
};

QString CanvasModuleInfo::id() const
{
    return QStringLiteral("canvas");
}

QString CanvasModuleInfo::name() const
{
    return QStringLiteral("Canvas");
}

QString CanvasModuleInfo::description() const
{
    return QStringLiteral("Display any image or video sequence.");
}

ModuleCategories CanvasModuleInfo::categories() const
{
    return ModuleCategory::DISPLAY;
}

AbstractModule *CanvasModuleInfo::createModule(QObject *parent)
{
    return new CanvasModule(this, parent);
}

#include "canvasmodule.moc"
