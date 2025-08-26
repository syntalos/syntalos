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
#include <QTimer>

#include "canvaswindow.h"
#include "datactl/frametype.h"

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
    static constexpr double STREAM_EMA_ALPHA = 0.25;  // EMA smoothing factor for FPS updates for streamed frames

    // Framerate tracking for display
    double m_expectedDisplayFps;
    double m_currentDisplayFps;
    symaster_timepoint m_lastDisplayTime;
    double m_avgDisplayTimeMs; // Moving average of display intervals in ms

    // Stream FPS tracking
    symaster_timepoint m_lastStreamCalcTime;
    uint32_t m_streamedFramesSinceLastCalc;
    double m_expectedFps;
    double m_currentFpsEma; // Exponential moving average of stream FPS

    // Status text update optimization
    QString m_cachedStatusText;
    symaster_timepoint m_lastStatusUpdateTime;
    static constexpr double STATUS_UPDATE_INTERVAL_MS = 125.0; // Update status text only every 125ms

    // Stream speed throttling
    uint m_throttleCount;
    uint m_blackOutCount;

    bool m_active;
    bool m_paused;

public:
    explicit CanvasModule(CanvasModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_ctlIn = registerInputPort<ControlCommand>(QStringLiteral("control"), QStringLiteral("Control"));

        m_cvView = new CanvasWindow;
        addDisplayWindow(m_cvView);
        m_cvView->setWindowIcon(modInfo->icon());
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
        m_blackOutCount = 0;
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

        // check framerate and throttle it, showing a remark in the latter
        // case so the user is aware that they're not seeing every single frame
        m_expectedFps = m_frameSub->metadata().value("framerate", 60).toDouble();

        // initialize stream FPS tracking
        m_lastStreamCalcTime = currentTimePoint();
        m_streamedFramesSinceLastCalc = 0;
        m_currentFpsEma = -1; // Signal that we begin a new measurement

        // initialize display tracking
        m_avgDisplayTimeMs = (m_expectedFps > 0) ? (1000.0 / m_expectedFps) : (1000.0 / 60.0);

        // never ever try to display more than 240 fps by default
        // the module will lower this on its own if too much data is queued
        m_throttleCount = 240;
        m_frameSub->setThrottleItemsPerSec(m_throttleCount);
        m_expectedDisplayFps = (m_expectedFps < m_throttleCount) ? m_expectedFps : m_throttleCount;
        m_currentDisplayFps = m_expectedDisplayFps;

        auto imgWinTitle = m_frameSub->metadataValue(CommonMetadataKey::SrcModName).toString();
        if (imgWinTitle.isEmpty())
            imgWinTitle = "Canvas";
        const auto portTitle = m_frameSub->metadataValue(CommonMetadataKey::SrcModPortTitle).toString();
        if (!portTitle.isEmpty())
            imgWinTitle = QStringLiteral("%1 - %2").arg(imgWinTitle).arg(portTitle);
        m_cvView->setWindowTitle(imgWinTitle);
    }

    void processUiEvents() override
    {
        if (!m_active)
            return;
        auto maybeFrame = m_frameSub->peekNext();

        if (m_ctlSub) {
            auto maybeCtl = m_ctlSub->peekNext();
            if (maybeCtl.has_value()) {
                const auto ctlValue = maybeCtl.value();
                m_paused = ctlValue.kind == ControlCommandKind::STOP || ctlValue.kind == ControlCommandKind::PAUSE;
            }

            if (m_paused)
                return;
        }

        if (!maybeFrame.has_value())
            return;

        const auto skippedFrames = m_frameSub->retrieveApproxSkippedElements();
        const auto framesPendingCount = m_frameSub->approxPendingCount();

        // Count ALL frames that arrive from the source (both processed and skipped)
        // This gives us the true source rate
        // (we add one to account for the frame we are about to process)
        m_streamedFramesSinceLastCalc += skippedFrames + 1;

        if (framesPendingCount > (m_expectedDisplayFps * 2)) {
            // we have too many frames pending in the queue, we may have to throttle
            // the subscription more

            uint throttleAdjust = 1;
            if (m_throttleCount > 60) {
                throttleAdjust = framesPendingCount / 8;
                if (throttleAdjust == 0)
                    throttleAdjust = 1;
            }
            if (throttleAdjust >= m_throttleCount)
                m_throttleCount = 1;
            else
                m_throttleCount -= throttleAdjust;

            if ((m_throttleCount <= 2) && (m_expectedFps != 0)) {
                // throttle to less then 2fps? This looks suspicious, terminate.
                m_frameSub->suspend();
                m_blackOutCount++;

                if (m_blackOutCount >= 3) {
                    raiseError(
                        QStringLiteral("Dropped below 2fps in display render speed multiple times. Even when "
                                       "discarding most frames we still "
                                       "can not display images fast enough to empty the pending data queue.\n"
                                       "Either the displayed frames are excessively large, something is wrong with the "
                                       "display hardware, "
                                       "or there is a bug in the display code."));
                    return;
                }

                // resume operation, maybe we have better luck this time?
                // (if we can not manage to display at reasonable speed,
                // we will give up after a few tries)
                m_throttleCount = 144;
                m_expectedDisplayFps = (m_expectedFps < m_throttleCount) ? m_expectedFps : m_throttleCount;
                m_frameSub->setThrottleItemsPerSec(m_throttleCount);
                m_frameSub->resume();
                return;
            }

            m_frameSub->setThrottleItemsPerSec(m_throttleCount);
            m_expectedDisplayFps = m_throttleCount;

            // we don't show the current image, so the fps values of display and stream can
            // be updated properly in the next run.
            // since we are already skipping frames at this point, not showing the current one
            // is no loss
            return;
        }

        // get all timing info and show the image
        const auto frame = maybeFrame.value();
        m_cvView->showImage(frame.mat);
        const auto frameTimeUsec = frame.time.count();

        if (m_expectedFps == 0) {
            m_cvView->setStatusText(
                QTime::fromMSecsSinceStartOfDay(static_cast<int>(frameTimeUsec / 1000.0)).toString("hh:mm:ss.zzz"));

            // skip all further calculation, we don't know a framerate and are probably not displaying a video,
            // but just static images instead.
            return;
        }

        const auto timeNow = currentTimePoint();
        if (m_currentFpsEma < 0) {
            // The run has (very likely) just started!
            // We need to set the timepoint right here for accurate measurements of initial framerates
            m_lastStreamCalcTime = timeNow;
            m_currentFpsEma = m_expectedFps;
        }

        // Calculate display FPS from actual display intervals
        const double displayIntervalMs = timeDiffMsec(timeNow, m_lastDisplayTime).count();
        if (displayIntervalMs > 0) {
            m_avgDisplayTimeMs = (DISPLAY_EMA_ALPHA * displayIntervalMs)
                                 + ((1 - DISPLAY_EMA_ALPHA) * m_avgDisplayTimeMs);
            m_currentDisplayFps = 1000.0 / m_avgDisplayTimeMs;
        }

        // Stream FPS calculation
        if (m_streamedFramesSinceLastCalc >= 24) {
            const double streamElapsedSec = timeDiffMsec(timeNow, m_lastStreamCalcTime).count() / 1000.0;
            const double streamRate = static_cast<double>(m_streamedFramesSinceLastCalc) / streamElapsedSec;

            // EMA smoothing
            m_currentFpsEma = (STREAM_EMA_ALPHA * streamRate) + ((1 - STREAM_EMA_ALPHA) * m_currentFpsEma);

            // Reset stream counters and timing
            m_streamedFramesSinceLastCalc = 0;
            m_lastStreamCalcTime = timeNow;
        }

        // update last time we calculated speeds and displayed a frame
        m_lastDisplayTime = timeNow;

        // Update status text at a reduced rate to optimize performance
        const double timeSinceLastUpdate = timeDiffMsec(timeNow, m_lastStatusUpdateTime).count();
        if (timeSinceLastUpdate >= STATUS_UPDATE_INTERVAL_MS) {
            // Format status text efficiently using QStringLiteral for compile-time string construction
            m_cachedStatusText = QStringLiteral("%1 | Stream: %2fps (of %3fps) | Display: %4fps")
                                     .arg(QTime::fromMSecsSinceStartOfDay(static_cast<int>(frameTimeUsec / 1000.0))
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
