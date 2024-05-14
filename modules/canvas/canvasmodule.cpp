/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "canvassettingsdialog.h"
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
    CanvasSettingsDialog *m_cvSettings;

    double m_expectedFps;
    double m_currentFps;
    long m_lastFrameTime;
    double m_avgFrameTimeDiffMsec;

    double m_expectedDisplayFps;
    double m_currentDisplayFps;
    symaster_timepoint m_lastDisplayTime;

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

        m_cvSettings = new CanvasSettingsDialog(m_cvView);
        addSettingsWindow(m_cvSettings);
        m_cvSettings->setWindowIcon(modInfo->icon());
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::CALL_UI_EVENTS | ModuleFeature::SHOW_DISPLAY | ModuleFeature::SHOW_SETTINGS;
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
        m_currentDisplayFps = 60.0;
        m_blackOutCount = 0;
        m_paused = false;

        return true;
    }

    void start() override
    {
        m_lastFrameTime = m_syTimer->timeSinceStartMsec().count();
        if (m_frameSub.get() == nullptr) {
            m_active = false;
            return;
        }
        m_active = true;

        // check framerate and throttle it, showing a remark in the latter
        // case so the user is aware that they're not seeing every single frame
        m_expectedFps = m_frameSub->metadata().value("framerate", 60).toDouble();

        // never ever try to display more than 144fps by default
        // the module will lower this on its own if too much data is queued
        m_throttleCount = 144;
        m_frameSub->setThrottleItemsPerSec(m_throttleCount);
        m_expectedDisplayFps = (m_expectedFps < m_throttleCount) ? m_expectedFps : m_throttleCount;

        // assume perfect frame diff for now
        m_avgFrameTimeDiffMsec = 1000.0 / m_expectedFps;

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

        if (m_ctlSub.get() != nullptr) {
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
            m_currentFps = m_expectedFps;

            // we don't show the current image, so the fps values of display and stream can
            // be updated properly in the next run.
            // since we are already skipping frames at this point, not showing the current one
            // is no loss
            return;
        }

        // get all timing info and show the image
        const auto frame = maybeFrame.value();
        m_cvView->showImage(frame.mat.copy_memory());
        const auto frameTime = frame.time.count();

        if (m_expectedFps == 0) {
            m_cvView->setStatusText(QTime::fromMSecsSinceStartOfDay(frameTime).toString("hh:mm:ss.zzz"));
        } else {
            // we use a moving average of the inter-frame-time over two seconds, as the framerate occasionally
            // fluctuates (especially when throttling the subscription) and we want to display a more steady (but
            // accurate) info to the user instead, without twitching around too much
            m_avgFrameTimeDiffMsec = ((m_avgFrameTimeDiffMsec * m_expectedFps)
                                      + ((frameTime - m_lastFrameTime) / (skippedFrames + 1)))
                                     / (m_expectedFps + 1);
            m_lastFrameTime = frameTime;
            m_currentFps = (m_avgFrameTimeDiffMsec > 0) ? (1000.0 / m_avgFrameTimeDiffMsec) : m_expectedFps;

            const auto frameDisplayTime = currentTimePoint();
            if (std::isinf(m_currentDisplayFps))
                m_currentDisplayFps = 1000.0 / timeDiffMsec(frameDisplayTime, m_lastDisplayTime).count();
            else
                m_currentDisplayFps = ((m_currentDisplayFps * m_expectedDisplayFps)
                                       + (1000.0 / timeDiffMsec(frameDisplayTime, m_lastDisplayTime).count()))
                                      / (m_expectedDisplayFps + 1);
            m_lastDisplayTime = frameDisplayTime;

            m_cvView->setStatusText(QStringLiteral("%1 | Stream: %2fps (of %3fps) | Display: %4fps")
                                        .arg(QTime::fromMSecsSinceStartOfDay(frameTime).toString("hh:mm:ss.zzz"))
                                        .arg(m_currentFps, 0, 'f', 1)
                                        .arg(m_expectedFps, 0, 'f', 1)
                                        .arg(m_currentDisplayFps, 0, 'f', 0));
        }
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("highlight_saturation", m_cvView->highlightSaturation());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_cvView->setHighlightSaturation(settings.value("highlight_saturation", false).toBool());
        m_cvSettings->updateUi();
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
