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

#include "imageviewwidget.h"
#include "streams/frametype.h"

class CanvasModule : public AbstractModule
{
private:
    std::shared_ptr<StreamInputPort> m_framesIn;
    std::shared_ptr<StreamInputPort> m_ctlIn;

    std::shared_ptr<StreamSubscription<Frame>> m_frameSub;
    std::shared_ptr<StreamSubscription<ControlCommand>> m_ctlSub;

    ImageViewWidget *m_imgView;
    QString m_imgWinTitle;
    QTimer *m_evTimer;

public:
    explicit CanvasModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_ctlIn = registerInputPort<ControlCommand>(QStringLiteral("control"), QStringLiteral("Control"));

        m_imgView = new ImageViewWidget;
        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &CanvasModule::updateImage);
    }

    ~CanvasModule() override
    {
        delete m_imgView;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    void showDisplayUi() override
    {
        m_imgView->show();
    }

    void hideDisplayUi() override
    {
        m_imgView->hide();
    }

    bool prepare(const QString &, const TestSubject &) override
    {
        if (m_framesIn->hasSubscription())
            m_frameSub = m_framesIn->subscription<Frame>();
        if (m_ctlIn->hasSubscription())
            m_ctlSub = m_ctlIn->subscription<ControlCommand>();

        return true;
    }

    void start() override
    {
        if (m_frameSub.get() != nullptr) {
            m_evTimer->start();
            m_frameSub->setThrottleItemsPerSec(60); // never try to display more than 60fps
            m_imgWinTitle = m_frameSub->metadata().value("src_mod_name").toString();
            if (m_imgWinTitle.isEmpty())
                m_imgWinTitle = "Canvas";
        }
    }

    void stop() override
    {
        m_evTimer->stop();
    }

    void updateImage()
    {
        auto maybeFrame = m_frameSub->peekNext();
        if (maybeFrame.has_value()) {
            const auto frame = maybeFrame.value();
            m_imgView->showImage(frame.mat);
            m_imgView->setWindowTitle(QStringLiteral("%1 / %2").arg(m_imgWinTitle).arg(QTime::fromMSecsSinceStartOfDay(frame.time.count()).toString("hh:mm:ss")));
        }
    }

private:

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
