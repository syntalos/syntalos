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

    ~ExampleCppModule() override = default;

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

    bool prepare(const RunInfo &) override
    {
        // we have nothing to do if nothing is connected to our input
        if (!m_frameIn->hasSubscription()) {
            setStateDormant();
            return true;
        }
        auto frameSub = m_frameIn->subscription();

        // just copy the framerate from input to output port
        m_frameOut->setMetadataValue("framerate", frameSub->metadataValue<double>("framerate", 0.0));

        // do not forget to start active output channels
        m_frameOut->start();

        // hand everything our thread needs to the worker that will run in it
        setWorker(
            Worker{
                .frameSub = frameSub,
                .frameOut = m_frameOut,
            });

        // success
        return true;
    }

    /**
     * Everything that runs in the module's thread goes into a worker.
     *
     * The worker is filled by the module in prepare() and then handed over to Syntalos,
     * so it only has access to what is listed here - and not to the module itself,
     * which lives in the main thread together with any user interface it may have.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<StreamSubscription<Frame>> frameSub;
        std::shared_ptr<DataStream<Frame>> frameOut;

        void run()
        {
            // tell Syntalos that we are ready, and wait until all modules are started together
            mod.waitForStart();

            while (mod.running()) {
                auto maybeFrame = frameSub->next();
                if (!maybeFrame.has_value())
                    return; // end of stream

                // just move input to output
                frameOut->push(std::move(*maybeFrame));
            }
        }
    };
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
    return ModuleCategory::SYNTALOS_DEV | ModuleCategory::EXAMPLES;
}

AbstractModule *ExampleCppModuleInfo::createModule(QObject *parent)
{
    return new ExampleCppModule(parent);
}

#include "examplemodule.moc"
