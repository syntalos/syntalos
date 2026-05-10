/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "oeacqmodule.h"

SYNTALOS_MODULE(OpenEphysAcqModule)

class OpenEphysAcqModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<IntSignalBlock>> m_exIn;
    std::shared_ptr<DataStream<IntSignalBlock>> m_exOut;

public:
    explicit OpenEphysAcqModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        // Register all input- and output ports
        m_exIn = registerInputPort<IntSignalBlock>(QStringLiteral("frames-in"), QStringLiteral("Frames In"));
        m_exOut = registerOutputPort<IntSignalBlock>(QStringLiteral("frames-out"), QStringLiteral("Frames Out"));
    }

    ~OpenEphysAcqModule() override = default;

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
        if (m_exIn->hasSubscription()) {
            auto framesSub = m_exIn->subscription();

            // just copy the framerate from input to output port
            m_exOut->setMetadataValue("framerate", framesSub->metadataValue<double>("framerate", 0.0));

            // do not forget to start active output channels
            m_exOut->start();
        }

        // success
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        StreamSubscription<IntSignalBlock> *exSub = nullptr;
        if (m_exIn->hasSubscription())
            exSub = m_exIn->subscription().get();

        startWaitCondition->wait(this);

        if (!exSub)
            return;

        while (m_running) {
            auto maybeFrame = exSub->next();
            if (!maybeFrame.has_value())
                return; // end of stream

            // just move input to output
            auto frame = std::move(*maybeFrame);
            m_exOut->push(frame);
        }
    }

private:
};

QString OpenEphysAcqModuleInfo::id() const
{
    return QStringLiteral("open-ephys-acq");
}

QString OpenEphysAcqModuleInfo::name() const
{
    return QStringLiteral("Open Ephys AcqBoard");
}

QString OpenEphysAcqModuleInfo::description() const
{
    return QStringLiteral("Streams data from any generation of the Open Ephys Acquisition Board.");
}

QString OpenEphysAcqModuleInfo::authors() const
{
    return QStringLiteral(
        "Josh Siegle\n"
        "Aarón Cuevas López\n"
        "Brandon Parks\n"
        "Matthias Klumpp");
}

ModuleCategories OpenEphysAcqModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *OpenEphysAcqModuleInfo::createModule(QObject *parent)
{
    return new OpenEphysAcqModule(parent);
}

#include "oeacqmodule.moc"
