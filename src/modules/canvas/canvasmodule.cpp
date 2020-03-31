/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "canvasmodule.h"

#include <QTimer>
#include <QTime>

#include "canvaswindow.h"
#include "streams/frametype.h"

class CanvasModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort<Frame>> m_framesIn;
    std::shared_ptr<StreamInputPort<ControlCommand>> m_ctlIn;

    std::shared_ptr<StreamSubscription<Frame>> m_frameSub;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;

    CanvasWindow *m_cvView;
    QTimer *m_evTimer;

    int m_fps;
    long m_lastFrameTime;
    int m_currentFps;
    double m_avgFrameTimeDiffMsec;
    QString m_throttleRemark;

public:
    explicit CanvasModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_ctlIn = registerInputPort<ControlCommand>(QStringLiteral("control"), QStringLiteral("Control"));

        m_cvView = new CanvasWindow;
        addDisplayWindow(m_cvView);
        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &CanvasModule::updateImage);
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    bool prepare(const TestSubject &) override
    {
        m_frameSub.reset();
        m_ctlSub.reset();
        if (m_framesIn->hasSubscription())
            m_frameSub = m_framesIn->subscription();
        if (m_ctlIn->hasSubscription())
            m_ctlSub = m_ctlIn->subscription();

        return true;
    }

    void start() override
    {
        m_lastFrameTime = m_syTimer->timeSinceStartMsec().count();
        if (m_frameSub.get() == nullptr)
            return;

        // check framerate and throttle it, showing a remark in the latter
        // case so the user is aware that they're not seeing every single frame
        m_fps = m_frameSub->metadata().value("framerate", 0).toInt();
        m_throttleRemark = (m_fps > 50)? QStringLiteral("rate lowered for display, original:") : QStringLiteral("req.");
        m_frameSub->setThrottleItemsPerSec(50); // never try to display more than 50fps

        // assume perfect frame diff for now
        m_avgFrameTimeDiffMsec = 1000.0 / m_fps;

        auto imgWinTitle = m_frameSub->metadata().value("srcModName").toString();
        if (imgWinTitle.isEmpty())
            imgWinTitle = "Canvas";
        const auto portTitle = m_frameSub->metadata().value("srcModPortTitle").toString();
        if (!portTitle.isEmpty())
            imgWinTitle = QStringLiteral("%1 - %2").arg(imgWinTitle).arg(portTitle);
        m_cvView->setWindowTitle(imgWinTitle);

        m_evTimer->start();
    }

    void stop() override
    {
        m_evTimer->stop();
    }

    void updateImage()
    {
        auto maybeFrame = m_frameSub->peekNext();
        if (!maybeFrame.has_value())
            return;

        const auto frame = maybeFrame.value();
        m_cvView->showImage(frame.mat);
        const auto frameTime = frame.time.count();

        if (m_fps == 0) {
            m_cvView->setStatusText(QTime::fromMSecsSinceStartOfDay(frameTime).toString("hh:mm:ss.zzz"));
        } else {
            // we use a moving average of the inter-frame-time over two seconds, as the framerate occasionally fluctuates
            // (especially when throttling the subscription) and we want to display a more steady (but accurate)
            // info to the user instead, without twitching around too much
            m_avgFrameTimeDiffMsec = ((m_avgFrameTimeDiffMsec * (m_fps * 2)) + (frameTime - m_lastFrameTime)) / ((m_fps * 2) + 1);
            m_lastFrameTime = frameTime;
            if (m_avgFrameTimeDiffMsec > 0)
                m_currentFps = std::round(1000.0 / m_avgFrameTimeDiffMsec);
            else
                m_currentFps = m_fps;

            m_cvView->setStatusText(QStringLiteral("%1 / %2fps (%3 %4fps)")
                                    .arg(QTime::fromMSecsSinceStartOfDay(frameTime).toString("hh:mm:ss.zzz"))
                                    .arg(m_currentFps)
                                    .arg(m_throttleRemark)
                                    .arg(m_fps));
        }
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

QPixmap CanvasModuleInfo::pixmap() const
{
    return QPixmap(":/module/canvas");
}

AbstractModule *CanvasModuleInfo::createModule(QObject *parent)
{
    return new CanvasModule(parent);
}
