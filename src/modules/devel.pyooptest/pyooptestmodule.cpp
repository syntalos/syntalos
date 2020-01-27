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

#include "pyooptestmodule.h"
#include "oopmodule.h"
#include "streams/frametype.h"

#include <QMessageBox>

class PyOOPTestModule : public OOPModule
{
public:
    explicit PyOOPTestModule(QObject *parent = nullptr)
        : OOPModule(parent)
    {
        loadPythonScript("import maio as io\n"
                         "import cv2 as cv\n"
                         "\n"
                         "i = 6\n"
                         "iport = io.get_input_port('nonexistent')\n"
                         "print('IPort (nonexistent): ' + str(iport))\n"
                         "iport = io.get_input_port('video-in')\n"
                         "print('IPort: ' + str(iport))\n"
                         "def loop():\n"
                         "    r = io.await_new_input()\n"
                         "    if r == io.InputWaitResult.CANCELLED:\n"
                         "        print('Quitting PyOOPTestModule Loop!', r)\n"
                         "        return False\n"
                         "    while True:\n"
                         "        frame = iport.next()\n"
                         "        if frame is None:\n"
                         "            break\n"
                         "        cv.imshow('Frame Display', frame)\n"
                         "        cv.waitKey(1)\n"
                         "    return True\n"
                         "");

        registerInputPort<FirmataData>("firmata-in", "Pin Data");
        registerOutputPort<FirmataControl>("firmata-out", "Pin Control");
        registerOutputPort<TableRow>("table-out", "Table Rows");

        registerInputPort<Frame>("video-in", "Frames");
        registerOutputPort<Frame>("video-out", "Processed Frames");
    }

    ~PyOOPTestModule() override
    {

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

QPixmap PyOOPTestModuleInfo::pixmap() const
{
    return QPixmap(":/module/devel");
}

AbstractModule *PyOOPTestModuleInfo::createModule(QObject *parent)
{
    return new PyOOPTestModule(parent);
}
