/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "datasstmodule.h"
#include "streams/frametype.h"

#include <opencv2/imgproc.hpp>
#include "utils/misc.h"

SYNTALOS_MODULE(DevelDataSSTModule)

class DataSSTModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_fpSignalIn;

public:
    explicit DataSSTModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_fpSignalIn = registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig-in"), QStringLiteral("FSignal In"));
    }

    ~DataSSTModule() override
    {

    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::NONE;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool prepare(const TestSubject &) override
    {
        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        StreamSubscription<FloatSignalBlock> *fpSigSub = nullptr;
        if (m_fpSignalIn->hasSubscription())
            fpSigSub = m_fpSignalIn->subscription().get();

        startWaitCondition->wait(this);

        if (!fpSigSub)
            return;

        while (m_running) {
            if (fpSigSub) {
                auto maybeSb = fpSigSub->next();
                if (!maybeSb.has_value())
                    return; // end of stream

                const auto sb = maybeSb.value();

                for (int i = 0; i < 16; i++)
                    qDebug() << i << sb.data[i];
            }
        }
    }

private:

};

QString DevelDataSSTModuleInfo::id() const
{
    return QStringLiteral("devel.datasst");
}

QString DevelDataSSTModuleInfo::name() const
{
    return QStringLiteral("Devel: DataSST");
}

QString DevelDataSSTModuleInfo::description() const
{
    return QStringLiteral("Developer module representing a source, sink and transformer for debug logging.");
}

QIcon DevelDataSSTModuleInfo::icon() const
{
    return QIcon(":/module/devel");
}

bool DevelDataSSTModuleInfo::devel() const
{
    return true;
}

AbstractModule *DevelDataSSTModuleInfo::createModule(QObject *parent)
{
    return new DataSSTModule(parent);
}

#include "datasstmodule.moc"
