/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "examplemodule.h"
#include "datactl/frametype.h"

SYNTALOS_MODULE(ExampleCppModule)

class ExampleCppModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<Frame>> m_frameIn;
    std::shared_ptr<DataStream<Frame>> m_frameOut;

public:
    explicit ExampleCppModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        // Register all input- and output ports
        m_frameIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames In"));
        m_frameOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Frames Out"));
    }

    ~ExampleCppModule() override {}

    ModuleFeatures features() const override
    {
        // This module has no specific features (like a settings UI) yet.
        return ModuleFeature::NONE;
    }

    ModuleDriverKind driver() const override
    {
        // This module shall be run in a dedicated thread.
        // This is for illustration purposes only, EVENTS_SHARED
        // would otherwise be more than sufficient.
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool prepare(const TestSubject &) override
    {
        if (m_frameIn->hasSubscription()) {
            auto framesSub = m_frameIn->subscription();

            // just copy the framerate from input to output port
            m_frameOut->setMetadataValue("framerate", framesSub->metadataValue("framerate").toDouble());

            // do not forget to start active output channels
            m_frameOut->start();
        }

        // success
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        StreamSubscription<Frame> *frameSub = nullptr;
        if (m_frameIn->hasSubscription())
            frameSub = m_frameIn->subscription().get();

        startWaitCondition->wait(this);

        if (!frameSub)
            return;

        while (m_running) {
            auto maybeFrame = frameSub->next();
            if (!maybeFrame.has_value())
                return; // end of stream

            // just copy input to output
            const auto frame = maybeFrame.value();
            m_frameOut->push(frame);
        }
    }

private:
};

QString ExampleCppModuleInfo::id() const
{
    return QStringLiteral("example-cpp");
}

QString ExampleCppModuleInfo::name() const
{
    return QStringLiteral("C++ Module Example");
}

QString ExampleCppModuleInfo::description() const
{
    return QStringLiteral("Most basic module, a starting place to develop a new C++ module.");
}

ModuleCategories ExampleCppModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV | ModuleCategory::EXAMPLE;
}

AbstractModule *ExampleCppModuleInfo::createModule(QObject *parent)
{
    return new ExampleCppModule(parent);
}

#include "examplemodule.moc"
