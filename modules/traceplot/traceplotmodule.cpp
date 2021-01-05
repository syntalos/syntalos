/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "traceplotmodule.h"

#include <QTimer>

#include "tracedisplay.h"

class TracePlotModule : public AbstractModule
{
    Q_OBJECT
private:
    TraceDisplay *m_traceDisplay;
    QTimer *m_evTimer;

    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_fpSig1In;
    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_fpSig2In;
    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_fpSig3In;

    std::shared_ptr<StreamInputPort<IntSignalBlock>> m_intSig1In;

public:
    explicit TracePlotModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_traceDisplay(nullptr)
    {
        setName(QStringLiteral("TracePlot"));

        // create trace display and fetch plot controller reference
        m_traceDisplay = new TraceDisplay();
        addDisplayWindow(m_traceDisplay);

        m_fpSig1In = registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig1-in"), QStringLiteral("Float In 1"));
        m_fpSig2In = registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig2-in"), QStringLiteral("Float In 2"));
        m_fpSig3In = registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig3-in"), QStringLiteral("Float In 3"));
        m_intSig1In = registerInputPort<IntSignalBlock>(QStringLiteral("intsig1-in"), QStringLiteral("Integer In 3"));
        m_traceDisplay->addFloatPort(m_fpSig1In);
        m_traceDisplay->addFloatPort(m_fpSig2In);
        m_traceDisplay->addFloatPort(m_fpSig3In);
        m_traceDisplay->addIntPort(m_intSig1In);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &TracePlotModule::checkNewData);
    }


    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    void inputPortConnected(VarStreamInputPort *) override
    {
        // update our list of channel details, as stream subscriptions have changed
        m_traceDisplay->updatePortChannels();
    }

    bool prepare(const TestSubject&) override
    {
        // reset trace plot data and ensure active subscriptions
        // are recognized
        m_traceDisplay->resetPlotConfig();

        return true;
    }

    void start() override
    {
        m_evTimer->start();
        AbstractModule::start();
    }

    void stop() override
    {
        m_evTimer->stop();
        AbstractModule::stop();
    }

    void checkNewData()
    {
        m_traceDisplay->updatePlotData();
    }
};

QString TracePlotModuleInfo::id() const
{
    return QStringLiteral("traceplot");
}

QString TracePlotModuleInfo::name() const
{
    return QStringLiteral("TracePlot");
}

QString TracePlotModuleInfo::description() const
{
    return QStringLiteral("Display signal data (usually from an electrophysiology data source) and plot it instantly.");
}

QPixmap TracePlotModuleInfo::pixmap() const
{
    return QPixmap(":/module/traceplot");
}

bool TracePlotModuleInfo::singleton() const
{
    return true;
}

AbstractModule *TracePlotModuleInfo::createModule(QObject *parent)
{
    return new TracePlotModule(parent);
}

#include "traceplotmodule.moc"
