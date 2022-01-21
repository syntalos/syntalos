/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include "oopmodule.h"
#include "streams/frametype.h"

#include <QMessageBox>

SYNTALOS_MODULE(PyOOPTestModule)

class PyOOPTestModule : public OOPModule
{
    Q_OBJECT
private:
    std::shared_ptr<DataStream<Frame>> m_vOut;
public:
    explicit PyOOPTestModule(QObject *parent = nullptr)
        : OOPModule(parent)
    {
        setPythonScript("import syio as sy\n"
                        "import cv2 as cv\n"
                        "\n"
                        "iport = sy.get_input_port('nonexistent')\n"
                        "print('IPort (nonexistent): ' + str(iport))\n"
                        "iport = sy.get_input_port('video-in')\n"
                        "oport = sy.get_output_port('video-out')\n"
                        "print('IPort: ' + str(iport))\n"
                        "print('OPort: ' + str(oport))\n"
                        "oport.set_metadata_value('framerate', 200)\n"
                        "oport.set_metadata_value_size('size', [800, 600])\n"
                        "def loop() -> bool:\n"
                        "    r = sy.await_new_input()\n"
                        "    if r == sy.InputWaitResult.CANCELLED:\n"
                        "        print('Quitting PyOOPTestModule Loop!', r)\n"
                        "        return False\n"
                        "    while True:\n"
                        "        frame = iport.next()\n"
                        "        if frame is None:\n"
                        "            break\n"
                        "        blur = cv.blur(frame.mat, (5,5))\n"
                        "        frame.mat = blur\n"
                        "        oport.submit(frame)\n"
                        "    return True\n"
                        "");

        registerInputPort<FirmataData>("firmata-in", "Pin Data");
        registerOutputPort<FirmataControl>("firmata-out", "Pin Control");
        registerOutputPort<TableRow>("table-out", "Table Rows");

        registerInputPort<Frame>("video-in", "Frames");
        m_vOut = registerOutputPort<Frame>("video-out", "Processed Frames");
    }

    ~PyOOPTestModule() override
    {

    }

    ModuleFeatures features() const override
    {
        return OOPModule::features();
    }

    bool prepare(const TestSubject &) override
    {
        m_vOut->setMetadataValue("size", QSize(800, 600));
        m_vOut->setMetadataValue("framerate", (double) 200);
        m_vOut->start();

        return true;
    }

    void stop() override
    {

    }
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
