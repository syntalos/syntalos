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

#include "oopmodule.h"

#include <QEventLoop>
#include <QRemoteObjectNode>

#include "oopworkerconnector.h"

class OOPModuleRunData
{
public:
    OOPModuleRunData()
    {
        repNode.reset(new QRemoteObjectNode);
    }
    ~OOPModuleRunData()
    {}

    std::unique_ptr<QRemoteObjectNode> repNode;
    QSharedPointer<OOPWorkerReplica> replica;
    QSharedPointer<OOPWorkerConnector> wc;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class OOPModule::Private
{
public:
    Private()
        : captureStdout(false),
          runData(new OOPModuleRunData)
    {}
    ~Private() {}

    QString pyScript;
    QString pyEnv;
    QString workerBinary;
    bool captureStdout;
    OOPWorkerReplica::Stage workerStage;

    bool failed;
    QSharedPointer<OOPModuleRunData> runData;
};
#pragma GCC diagnostic pop

OOPModule::OOPModule(QObject *parent)
    : AbstractModule(parent),
      d(new OOPModule::Private)
{
    // default to using the Python worker binary
    setWorkerBinaryPyWorker();
}

OOPModule::~OOPModule()
{
}

ModuleFeatures OOPModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY |
           ModuleFeature::SHOW_SETTINGS;
}

bool OOPModule::prepare(const TestSubject &testSubject)
{
    Q_UNUSED(testSubject)

    return true;
}

bool OOPModule::oopPrepare(QEventLoop *loop, const QVector<uint> &cpuAffinity)
{
    // We have to do all the setup stuff on thread creation, as moving QObject
    // instances between different threads is not ideal and the QRO connection
    // occasionally doesn't get established properly if we shift work between threads
    d->runData.reset(new OOPModuleRunData);

    d->runData->replica.reset(d->runData->repNode->acquire<OOPWorkerReplica>());
    d->runData->wc.reset(new OOPWorkerConnector(d->runData->replica, d->workerBinary));

    auto wc = d->runData->wc;
    d->failed = false;

    // connect some of the important signals of our replica
    connect(d->runData->replica.data(), &OOPWorkerReplica::stageChanged, this, [&](const OOPWorkerReplica::Stage &newStage) {
        d->workerStage = newStage;
    });

    connect(d->runData->replica.data(), &OOPWorkerReplica::error, this, [&](const QString &message) {
        d->failed = true;
        raiseError(message);
        m_running = false;
    });
    connect(d->runData->replica.data(), &OOPWorkerReplica::statusMessage, this, [&](const QString &text) {
        setStatusMessage(text);
    });

    wc->setCaptureStdout(d->captureStdout);
    if (!wc->connectAndRun(cpuAffinity)) {
        raiseError("Unable to start worker process!");
        return false;
    }

    wc->setInputPorts(inPorts());
    wc->setOutputPorts(outPorts());

    wc->initWithPythonScript(d->pyScript, d->pyEnv);

    // check if we already received messages from the worker,
    // such as errors or output port metadata updates
    // we need to wait for some time until the worker is ready
    statusMessage("Waiting for worker to get ready...");
    const auto waitStartTime = currentTimePoint();
    while (d->workerStage != OOPWorkerReplica::READY) {
        loop->processEvents();

        // if we are in a failed state, we have already emitted an error message
        if (wc->failed() || d->failed)
            return false;

        if (timeDiffMsec(currentTimePoint(), waitStartTime).count() > 20000) {
            // waiting 20sec is long enough, presumably the worker died and we can not
            // continue here
            raiseError("The worker did not signal readyness - maybe it crashed or is frozen?");
            return false;
        }
    }

    // set all outgoing streams as active (which propagates metadata)
    for (auto &port : outPorts())
        port->streamVar()->start();

    if (wc->failed())
        return false;

    statusMessage("Worker is ready.");
    setStateReady();
    return true;
}

void OOPModule::oopStart(QEventLoop *)
{
    statusMessage("");
    d->runData->wc->start(m_syTimer->startTime());
}

void OOPModule::oopRunEvent(QEventLoop *loop)
{
    // first thing to do: Look for possible (error) signals from our worker
    loop->processEvents();

    // forward incoming data to the worker
    d->runData->wc->forwardInputData(loop);

    if (d->captureStdout) {
        const auto data = d->runData->wc->readProcessStdout();
        if (!data.isEmpty())
            emit processStdoutReceived(data);
    }

    if (d->runData->wc->failed())
        m_running = false;
}

void OOPModule::oopFinalize(QEventLoop *loop)
{
    statusMessage("Waiting for worker to terminate...");
    d->runData->wc->terminate(loop);
    if (d->captureStdout) {
        const auto data = d->runData->wc->readProcessStdout();
        if (!data.isEmpty())
            emit processStdoutReceived(data);
    }

    d->runData.reset();
    statusMessage("");
}

void OOPModule::loadPythonScript(const QString &script, const QString &env)
{
    d->pyScript = script;
    d->pyEnv = env;
}

void OOPModule::loadPythonFile(const QString &fname, const QString &env)
{
    QFile f(fname);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        raiseError(QStringLiteral("Unable to open Python script file: %1").arg(fname));
        return;
    }
    QTextStream in(&f);
    loadPythonScript(in.readAll(), env);
}

QString OOPModule::workerBinary() const
{
    return d->workerBinary;
}

void OOPModule::setWorkerBinary(const QString &binPath)
{
    d->workerBinary = binPath;
}

void OOPModule::setWorkerBinaryPyWorker()
{
    d->workerBinary = QStringLiteral("%1/pyworker/pyworker").arg(QCoreApplication::applicationDirPath());
    QFileInfo checkBin(d->workerBinary);
    if (!checkBin.exists()) {
        d->workerBinary = QStringLiteral("%1/../lib/syntalos/pyworker").arg(QCoreApplication::applicationDirPath());
        QFileInfo fi(d->workerBinary);
        d->workerBinary = fi.canonicalFilePath();
    }
}

bool OOPModule::captureStdout() const
{
    return d->captureStdout;
}

void OOPModule::setCaptureStdout(bool capture)
{
    d->captureStdout = capture;
}
