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

#include "pyooptestmodule.h"
#include "mlinkmodule.h"
#include "streams/frametype.h"
#include "globalconfig.h"

#include <QMessageBox>

SYNTALOS_MODULE(PyOOPTestModule)

class PyOOPTestModule : public MLinkModule
{
    Q_OBJECT
private:
    std::shared_ptr<DataStream<Frame>> m_vOut;

public:
    explicit PyOOPTestModule(QObject *parent = nullptr)
        : MLinkModule(parent)
    {
        // we use the generic Python OOP worker process for this
        setModuleBinary(findSyntalosPyWorkerBinary());

        setScript(
            "import syio as sy\n"
            "import cv2 as cv\n"
            "\n"
            "iport = sy.get_input_port('nonexistent')\n"
            "print('IPort (nonexistent): ' + str(iport))\n"
            "iport = sy.get_input_port('video-in')\n"
            "oport = sy.get_output_port('video-out')\n"
            "print('IPort: ' + str(iport))\n"
            "print('OPort: ' + str(oport))\n"
            "oport.set_metadata_value('framerate', 200)\n"
            "oport.set_metadata_value_size('size', [960, 600])\n"
            "\n"
            "def prepare() -> bool:\n"
            "    iport.on_data = new_data_event\n"
            "    return True\n"
            "\n"
            "def run() -> bool:\n"
            "    while sy.is_running():\n"
            "        sy.await_data()\n"
            "    print('Quitting PyOOPTestModule Loop!')\n"
            "\n"
            "def new_data_event(frame) -> None:\n"
            "    blur = cv.blur(frame.mat, (5,5))\n"
            "    frame.mat = blur\n"
            "    oport.submit(frame)\n"
            "\n");

        registerInputPort<FirmataData>("firmata-in", "Pin Data");
        registerOutputPort<FirmataControl>("firmata-out", "Pin Control");
        registerOutputPort<TableRow>("table-out", "Table Rows");

        registerInputPort<Frame>("video-in", "Frames");
        m_vOut = registerOutputPort<Frame>("video-out", "Processed Frames");
    }

    ~PyOOPTestModule() override {}

    ModuleFeatures features() const override
    {
        return MLinkModule::features();
    }

    bool prepare(const TestSubject &subject) override
    {
        m_vOut->setMetadataValue("size", QSize(960, 600));
        m_vOut->setMetadataValue("framerate", (double)200);
        m_vOut->start();

        return MLinkModule::prepare(subject);
    }

    void stop() override {}
};

QString PyOOPTestModuleInfo::id() const
{
    return QStringLiteral("devel.pyooptest");
}

QString PyOOPTestModuleInfo::name() const
{
    return QStringLiteral("Devel: PyOOPTest");
}

QString PyOOPTestModuleInfo::description() const
{
    return QStringLiteral("Test module to test out-of-process and Python capabilities.");
}

QIcon PyOOPTestModuleInfo::icon() const
{
    return QIcon(":/module/devel");
}

bool PyOOPTestModuleInfo::devel() const
{
    return true;
}

AbstractModule *PyOOPTestModuleInfo::createModule(QObject *parent)
{
    return new PyOOPTestModule(parent);
}

#include "pyooptestmodule.moc"
