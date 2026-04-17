/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMessageBox>
#include <filesystem>

#include "mlinkmodule.h"
#include "datactl/frametype.h"
#include "globalconfig.h"

SYNTALOS_MODULE(PyOOPTestModule)

static QString PYOOP_TEST_SCRIPT = QStringLiteral(R"%py(
import syntalos_mlink as syl
import cv2 as cv

iport = syl.get_input_port('nonexistent')
print('IPort (nonexistent): ' + str(iport))
iport_frames = syl.get_input_port('video-in')
oport_frames = syl.get_output_port('video-out')
oport_tab = syl.get_output_port('table-out')
print('IPort: ' + str(iport_frames))
print('OPorts: ' + str([oport_frames, oport_tab]))

def prepare() -> bool:
    iport_frames.on_data = new_data_event
    return True

def start():
    # copy the framerate from input to output
    oport_frames.set_metadata_value('framerate', iport_frames.metadata['framerate'])
    oport_frames.set_metadata_value_size('size', iport_frames.metadata['size'])

    # table metadata
    oport_tab.set_metadata_value('table_header', ['Index', 'Frame Time'])

def run() -> bool:
    while syl.is_running():
        syl.await_data()
    print('Quitting PyOOPTestModule Loop!')

def new_data_event(frame) -> None:
    text = 'pyOOPTest'
    font = cv.FONT_HERSHEY_SIMPLEX
    font_scale = 1.5
    thickness = 3
    color = (0, 255, 0)
    (text_w, text_h), _ = cv.getTextSize(text, font, font_scale, thickness)
    h, w = frame.mat.shape[:2]
    org = ((w - text_w) // 2, (h + text_h) // 2)
    cv.putText(frame.mat, text, org, font, font_scale, color, thickness, cv.LINE_AA)
    oport_frames.submit(frame)

    if frame.index % 100 == 0:
        oport_tab.submit([frame.index, frame.time_usec])
)%py");

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
        if (std::filesystem::exists("/usr/bin/gdb")) {
            setModuleBinary("/usr/bin/gdb");
            setModuleBinaryArgs(
                QStringList() << "-q" << "-batch" << "-ex" << "run" << "-ex" << "bt" << "--args"
                              << findSyntalosPyWorkerBinary());
        } else {
            setModuleBinary(findSyntalosPyWorkerBinary());
        }

        setScript(PYOOP_TEST_SCRIPT);
        setWorkerMode(ModuleWorkerMode::TRANSIENT);

        registerInputPort<FirmataData>("firmata-in", "Pin Data");
        registerOutputPort<FirmataControl>("firmata-out", "Pin Control");
        registerOutputPort<TableRow>("table-out", "Table Rows");

        registerInputPort<Frame>("video-in", "Frames");
        m_vOut = registerOutputPort<Frame>("video-out", "Processed Frames");
    }

    ~PyOOPTestModule() override = default;

    ModuleFeatures features() const override
    {
        return MLinkModule::features();
    }

    bool prepare(const TestSubject &subject) override
    {
        m_vOut->setMetadataValue("size", MetaSize(960, 600));
        m_vOut->setMetadataValue("framerate", (double)200.0);
        m_vOut->start();

        return MLinkModule::prepare(subject);
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

ModuleCategories PyOOPTestModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV;
}

AbstractModule *PyOOPTestModuleInfo::createModule(QObject *parent)
{
    return new PyOOPTestModule(parent);
}

#include "pyooptestmodule.moc"
