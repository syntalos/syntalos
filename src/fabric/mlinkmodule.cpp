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

#include "mlinkmodule.h"

#include "config.h"
#include <QProcess>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <iceoryx_hoofs/posix_wrapper/signal_watcher.hpp>
#include <iceoryx_posh/popo/subscriber.hpp>
#include <iceoryx_posh/popo/untyped_subscriber.hpp>
#include <iceoryx_posh/popo/client.hpp>
#include <iceoryx_posh/popo/untyped_client.hpp>
#include <iceoryx_posh/popo/wait_set.hpp>
#include <iceoryx_posh/popo/listener.hpp>
#include <iceoryx_posh/runtime/service_discovery.hpp>

#include "mlink/ipc-types-private.h"
#include "streams/datatype-utils.h"
#include "globalconfig.h"
#include "utils/misc.h"

namespace Syntalos
{
Q_LOGGING_CATEGORY(logMLinkMod, "mlink-master")
}

class MLinkModule::Private
{
public:
    Private() {}

    ~Private() {}

    QProcess *proc;
    bool outputCaptured;
    QString pyVenvDir;
    QString scriptWDir;
    QString scriptContent;

    QByteArray settingsData;

    bool portChangesAllowed;
    QHash<QString, std::shared_ptr<VarStreamInputPort>> inPortIdMap;
    QHash<QString, std::shared_ptr<VariantDataStream>> outPortIdMap;

    iox::capro::IdString_t clientId;
    std::unique_ptr<iox::popo::Subscriber<ErrorEvent>> subError;

    std::unique_ptr<iox::popo::UntypedSubscriber> subInPortChange;
    std::unique_ptr<iox::popo::UntypedSubscriber> subOutPortChange;

    std::vector<std::pair<std::unique_ptr<iox::popo::UntypedSubscriber>, std::shared_ptr<StreamOutputPort>>>
        outPortSubs;

    iox::popo::Listener ioxListener;
};

template<typename T>
std::unique_ptr<T> MLinkModule::makeSubscriber(const QString &eventName)
{
    iox::popo::SubscriberOptions subOptn;

    // hold 10 elements for processing by default
    subOptn.queueCapacity = 10U;

    // get the last 5 samples if for whatever reason we connected too late
    subOptn.historyRequest = 5U;

    const auto eventNameIox = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, eventName.toStdString());
    return std::make_unique<T>(iox::capro::ServiceDescription{"SyntalosModule", d->clientId, eventNameIox}, subOptn);
}

std::unique_ptr<iox::popo::UntypedSubscriber> MLinkModule::makeUntypedSubscriber(const QString &eventName)
{
    iox::popo::SubscriberOptions subOptn;

    // hold 10 elements for processing by default
    subOptn.queueCapacity = 10U;

    // get the last 5 samples if for whatever reason we connected too late
    subOptn.historyRequest = 5U;

    const auto eventNameIox = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, eventName.toStdString());
    return std::make_unique<iox::popo::UntypedSubscriber>(
        iox::capro::ServiceDescription{"SyntalosModule", d->clientId, eventNameIox}, subOptn);
}

template<typename T>
std::unique_ptr<T> MLinkModule::makeClient(const QString &callName)
{
    iox::popo::ClientOptions optn;
    optn.responseQueueCapacity = 2U;

    const auto callNameIox = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, callName.toStdString());
    return std::make_unique<T>(iox::capro::ServiceDescription{"SyntalosModule", d->clientId, callNameIox}, optn);
}

std::unique_ptr<iox::popo::UntypedClient> MLinkModule::makeUntypedClient(const QString &callName)
{
    iox::popo::ClientOptions optn;
    optn.responseQueueCapacity = 2U;

    const auto callNameIox = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, callName.toStdString());
    return std::make_unique<iox::popo::UntypedClient>(
        iox::capro::ServiceDescription{"SyntalosModule", d->clientId, callNameIox}, optn);
}

template<typename Client, typename Func>
bool MLinkModule::callClientSimple(const Client &client, Func func, int timeoutSec)
{
    int secsWaited = 0;
    iox::popo::WaitSet waitset;

    waitset.attachState(*client.get(), iox::popo::ClientState::HAS_RESPONSE).or_else([this](auto) {
        raiseError("Could not attach to module process!");
    });

    const auto eventIDString = client->getServiceDescription().getEventIDString();
    client->loan()
        .and_then([&](auto &request) {
            func(request);
            request.send().or_else([this, eventIDString, &client](auto &error) {
                if (state() != ModuleState::ERROR)
                    raiseError(
                        QStringLiteral("Unable to send %1 request to module process!").arg(eventIDString.c_str()));
            });
        })
        .or_else([this, eventIDString](auto &error) {
            raiseError(QStringLiteral("Unable to allocate %1 request!").arg(eventIDString.c_str()));
        });

    if (state() == ModuleState::ERROR)
        return false;

    while (true) {
        QCoreApplication::processEvents();
        auto notificationVector = waitset.timedWait(iox::units::Duration::fromSeconds(1));
        for (auto &notification : notificationVector) {
            if (notification->doesOriginateFrom(client.get())) {
                bool success;
                while (client->take().and_then([&](const auto &response) {
                    success = response->success;
                })) {
                }

                return success;
            }
        }

        if (secsWaited++ >= timeoutSec) {
            raiseError(QStringLiteral("Timeout while waiting for %1 response!").arg(eventIDString.c_str()));
            return false;
        }
    }
}

template<typename ReqData>
bool MLinkModule::callUntypedClientSimple(
    const std::unique_ptr<iox::popo::UntypedClient> &client,
    const ReqData &reqEntity,
    int timeoutSec)
{
    int secsWaited = 0;
    iox::popo::WaitSet waitset;

    waitset.attachState(*client, iox::popo::ClientState::HAS_RESPONSE).or_else([this](auto) {
        raiseError("Could not attach to module process!");
    });

    auto bytes = reqEntity.toBytes();
    const auto eventIDString = client->getServiceDescription().getEventIDString();
    client->loan(bytes.size(), 0)
        .and_then([&](auto &payload) {
            memcpy(payload, bytes.data(), bytes.size());

            client->send(payload).or_else([this, eventIDString](auto &error) {
                if (state() != ModuleState::ERROR)
                    raiseError(
                        QStringLiteral("Unable to send %1 request to module process!").arg(eventIDString.c_str()));
            });
        })
        .or_else([this, eventIDString](auto &error) {
            raiseError(QStringLiteral("Unable to allocate %1 request!").arg(eventIDString.c_str()));
        });

    if (state() == ModuleState::ERROR)
        return false;

    while (true) {
        QCoreApplication::processEvents();
        auto notificationVector = waitset.timedWait(iox::units::Duration::fromSeconds(1));
        for (auto &notification : notificationVector) {
            if (notification->doesOriginateFrom(client.get())) {
                bool success;
                while (client->take().and_then([&](const auto &responsePayload) {
                    auto response = static_cast<const DoneResponse *>(responsePayload);
                    success = response->success;
                    client->releaseResponse(responsePayload);
                })) {
                }

                return success;
            }
        }

        if (secsWaited++ >= timeoutSec) {
            raiseError(QStringLiteral("Timeout while waiting for %1 response!").arg(eventIDString.c_str()));
            return false;
        }
    }
}

MLinkModule::MLinkModule(QObject *parent)
    : AbstractModule(parent),
      d(new MLinkModule::Private)
{
    d->proc = new QProcess(this);
    d->portChangesAllowed = true;
    resetConnection();

    // merge stdout/stderr of external process with ours by default
    setOutputCaptured(false);

    d->proc->connect(d->proc, &QProcess::readyReadStandardOutput, this, [this]() {
        if (d->outputCaptured)
            emit processOutputReceived(readProcessOutput());
    });
    d->proc->connect(
        d->proc,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
            if (exitStatus == QProcess::CrashExit) {
                raiseError(QStringLiteral("Module process crashed with exit code %1! Check the log for details.")
                               .arg(exitCode));
            }
        });
}

MLinkModule::~MLinkModule() {}

void MLinkModule::onErrorReceivedCb(iox::popo::Subscriber<ErrorEvent> *subscriber, MLinkModule *self)
{
    subscriber->take().and_then([subscriber, self](auto &error) {
        if (error->title.empty())
            self->raiseError(error->message.c_str());
        else
            self->raiseError(
                QStringLiteral("<html><b>%1</b><br/>%2").arg(error->title.c_str(), error->message.c_str()));
        QCoreApplication::processEvents();
    });
}

void MLinkModule::onPortChangedCb(iox::popo::UntypedSubscriber *subscriber, MLinkModule *self)
{
    if (!self->d->portChangesAllowed) {
        qCDebug(logMLinkMod).noquote() << "Port change ignored: No changes are allowed.";
        return;
    }

    // process new input/output ports
    subscriber->take()
        .and_then([subscriber, self](const void *payload) {
            auto eventIdString = subscriber->getServiceDescription().getEventIDString();
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(payload);
            const auto size = chunkHeader->usedSizeOfChunk();

            if (eventIdString == IN_PORT_CHANGE_CHANNEL_ID) {
                // deserialize
                const auto ipc = InputPortChange::fromMemory(payload, size);
                const auto action = ipc.action;

                if (action == PortAction::ADD) {
                    const auto iport = self->registerInputPortByTypeId(ipc.dataTypeId, ipc.id, ipc.title);
                    self->d->inPortIdMap.insert(ipc.id, iport);
                } else if (action == PortAction::REMOVE) {
                    self->removeInPortById(ipc.id);
                    self->d->inPortIdMap.remove(ipc.id);
                }
            } else if (eventIdString == OUT_PORT_CHANGE_CHANNEL_ID) {
                // deserialize
                const auto opc = OutputPortChange::fromMemory(payload, size);
                const auto action = opc.action;

                if (action == PortAction::ADD) {
                    const auto oport = self->registerOutputPortByTypeId(opc.dataTypeId, opc.id, opc.title);
                    oport->setMetadata(opc.metadata);
                    self->d->outPortIdMap.insert(opc.id, oport);
                } else if (action == PortAction::REMOVE) {
                    self->removeOutPortById(opc.id);
                    self->d->outPortIdMap.remove(opc.id);
                } else if (action == PortAction::CHANGE) {
                    std::shared_ptr<VariantDataStream> ostream;
                    if (self->d->outPortIdMap.contains(opc.id)) {
                        ostream = self->d->outPortIdMap.value(opc.id);
                    } else {
                        auto oport = self->outPortById(opc.id);
                        if (oport)
                            ostream = oport->streamVar();
                    }
                    if (ostream)
                        ostream->setMetadata(opc.metadata);
                }
            }

            // release memory chunk
            subscriber->release(payload);
        })
        .or_else([](auto &result) {
            if (result != iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE) {
                qCWarning(logMLinkMod).noquote() << "Failed to receive new port info!";
            }
        });
}

void MLinkModule::resetConnection()
{
    d->clientId = iox::capro::IdString_t(
        iox::cxx::TruncateToCapacity, QStringLiteral("%1_%2").arg(id()).arg(index()).toStdString());

    // detach all events from the listener
    if (d->subError != nullptr)
        d->ioxListener.detachEvent(*d->subError, iox::popo::SubscriberEvent::DATA_RECEIVED);
    if (d->subInPortChange != nullptr)
        d->ioxListener.detachEvent(*d->subInPortChange, iox::popo::SubscriberEvent::DATA_RECEIVED);
    if (d->subOutPortChange != nullptr)
        d->ioxListener.detachEvent(*d->subOutPortChange, iox::popo::SubscriberEvent::DATA_RECEIVED);

    // (re)create subscribers
    d->subError = makeSubscriber<iox::popo::Subscriber<ErrorEvent>>(ERROR_CHANNEL_ID.c_str());
    d->subInPortChange = makeUntypedSubscriber(IN_PORT_CHANGE_CHANNEL_ID.c_str());
    d->subOutPortChange = makeUntypedSubscriber(OUT_PORT_CHANGE_CHANNEL_ID.c_str());

    // attach events again
    d->ioxListener
        .attachEvent(
            *d->subError,
            iox::popo::SubscriberEvent::DATA_RECEIVED,
            iox::popo::createNotificationCallback(onErrorReceivedCb, *this))
        .or_else([this](auto) {
            raiseError("Unable to attach to Error event! Communication with module is not possible.");
        });
    d->ioxListener
        .attachEvent(
            *d->subInPortChange,
            iox::popo::SubscriberEvent::DATA_RECEIVED,
            iox::popo::createNotificationCallback(onPortChangedCb, *this))
        .or_else([this](auto) {
            raiseError("Unable to attach event to NewInPort! Communication with module is not possible.");
        });
    d->ioxListener
        .attachEvent(
            *d->subOutPortChange,
            iox::popo::SubscriberEvent::DATA_RECEIVED,
            iox::popo::createNotificationCallback(onPortChangedCb, *this))
        .or_else([this](auto) {
            raiseError("Unable to attach event to NewOutPort! Communication with module is not possible.");
        });
}

ModuleDriverKind MLinkModule::driver() const
{
    return ModuleDriverKind::NONE;
}

ModuleFeatures MLinkModule::features() const
{
    return ModuleFeature::SHOW_DISPLAY | ModuleFeature::SHOW_SETTINGS;
}

QString MLinkModule::moduleBinary() const
{
    return d->proc->program();
}

void MLinkModule::setModuleBinary(const QString &binaryPath)
{
    d->proc->setProgram(binaryPath);
}

bool MLinkModule::outputCaptured() const
{
    return d->outputCaptured;
}

void MLinkModule::setOutputCaptured(bool capture)
{
    d->outputCaptured = capture;
    if (d->outputCaptured)
        d->proc->setProcessChannelMode(QProcess::MergedChannels);
    else
        d->proc->setProcessChannelMode(QProcess::ForwardedChannels);
}

void MLinkModule::setPythonVirtualEnv(const QString &venvDir)
{
    d->pyVenvDir = venvDir;
}

void MLinkModule::setScript(const QString &script, const QString &wdir)
{
    d->scriptWDir = wdir;
    d->scriptContent = script;
}

bool MLinkModule::setScriptFromFile(const QString &fname, const QString &wdir)
{
    QFile f(fname);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        return false;

    QTextStream in(&f);
    setScript(in.readAll(), wdir);

    return true;
}

QByteArray MLinkModule::settingsData() const
{
    return d->settingsData;
}

void MLinkModule::setSettingsData(const QByteArray &data)
{
    d->settingsData = data;
}

void MLinkModule::terminateProcess()
{
    auto callShutdown = makeClient<iox::popo::Client<ShutdownRequest, DoneResponse>>(SHUTDOWN_CALL_ID.c_str());
    if (!isProcessRunning())
        return;

    // request the module process to terminate itself
    callClientSimple(callShutdown, [&](auto &request) {});

    // give the process some time to terminate
    d->proc->waitForFinished(5000);

    // ask nicely
    d->proc->terminate();
    d->proc->waitForFinished(5000);

    // no response? kill it!
    d->proc->kill();
    d->proc->waitForFinished(5000);
}

bool MLinkModule::runProcess()
{
    // ensure any existing process does not exist
    terminateProcess();

    d->subError->releaseQueuedData();
    d->subInPortChange->releaseQueuedData();
    d->subOutPortChange->releaseQueuedData();

    if (d->proc->program().isEmpty()) {
        qCWarning(logMLinkMod).noquote() << "MLink module has not set a worker binary";
        return false;
    }

    // reset connection, just in case we changed our ID
    resetConnection();

    auto penv = QProcessEnvironment::systemEnvironment();
    penv.insert("SYNTALOS_VERSION", syntalosVersionFull());
    penv.insert("SYNTALOS_MODULE_ID", d->clientId.c_str());
    if (!d->pyVenvDir.isEmpty()) {
        penv.remove("PYTHONHOME");
        penv.insert("VIRTUAL_ENV", d->pyVenvDir);
        penv.insert("PATH", QStringLiteral("%1/bin/:%2").arg(d->pyVenvDir, penv.value("PATH", "")));
    }

    d->proc->setProcessEnvironment(penv);
    d->proc->start(d->proc->program(), QStringList());
    if (!d->proc->waitForStarted())
        return false;

    // wait for the service to show up
    bool workerFound = false;
    iox::runtime::ServiceDiscovery sd;
    iox::popo::WaitSet<1> waitset;
    waitset.attachEvent(sd, iox::runtime::ServiceDiscoveryEvent::SERVICE_REGISTRY_CHANGED).or_else([](auto &) {
        qCWarning(logMLinkMod).noquote() << "Failed to attach to service discovery waitset!";
    });

    sd.findService(
        iox::capro::IdString_t("SyntalosModule"),
        d->clientId,
        iox::capro::Wildcard,
        [&](const iox::capro::ServiceDescription &s) {
            workerFound = true;
        },
        iox::popo::MessagingPattern::PUB_SUB);

    QElapsedTimer timer;
    timer.start();
    do {
        auto notificationVector = waitset.timedWait(iox::units::Duration::fromSeconds(5));
        for (auto &notification : notificationVector) {
            if (notification->doesOriginateFrom(&sd))
                workerFound = true;
        }

        if (timer.elapsed() > 5000)
            break;
    } while (!workerFound);

    if (!workerFound) {
        raiseError(
            "Module communication interface did not show up in time! The module might have crashed or may not be "
            "configured correctly.");
        d->proc->kill();
        return false;
    }

    return true;
}

bool MLinkModule::isProcessRunning() const
{
    return d->proc->state() == QProcess::Running;
}

QString MLinkModule::readProcessOutput()
{
    if (!d->outputCaptured)
        return QString();
    return d->proc->readAllStandardOutput();
}

void MLinkModule::markIncomingForExport(StreamExporter *exporter)
{
    auto callConnectIPort = makeClient<iox::popo::Client<ConnectInputRequest, DoneResponse>>(
        CONNECT_INPUT_CALL_ID.c_str());

    for (auto &iport : inPorts()) {
        const auto details = exporter->publishStreamByPort(iport);
        if (!details.has_value())
            continue;

        bool ret = callClientSimple(callConnectIPort, [&](auto &request) {
            request->portId = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, iport->id().toStdString());
            request->instanceId = iox::capro::IdString_t(
                iox::cxx::TruncateToCapacity, details->instanceId.toStdString());
            request->channelId = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, details->channelId.toStdString());
        });
        if (!ret)
            qWarning().noquote() << "Failed to connect exported input port" << iport->title();
    }
}

void MLinkModule::onOutputDataReceivedCb(iox::popo::UntypedSubscriber *subscriber, VariantDataStream *stream)
{
    subscriber->take()
        .and_then([subscriber, stream](const void *payload) {
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(payload);
            const auto size = chunkHeader->usedSizeOfChunk();

            stream->pushRawData(stream->dataTypeId(), payload, size);

            // release memory chunk
            subscriber->release(payload);
        })
        .or_else([](auto &result) {
            if (result != iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE) {
                qCWarning(logMLinkMod).noquote() << "Failed to receive new output data to forward!";
            }
        });
}

void MLinkModule::registerOutPortForwarders()
{
    // ensure we are disconnected
    disconnectOutPortForwarders();

    // connect to external process streams
    for (auto &oport : outPorts()) {
        if (!oport->streamVar()->hasSubscribers())
            continue;
        auto sub = makeUntypedSubscriber(QStringLiteral("oport_%1").arg(oport->id().mid(0, 80)));
        d->ioxListener
            .attachEvent(
                *sub,
                iox::popo::SubscriberEvent::DATA_RECEIVED,
                iox::popo::createNotificationCallback(onOutputDataReceivedCb, *oport->streamVar().get()))
            .or_else([this](auto) {
                raiseError(
                    "Unable to attach event to listen for output data submissions! Communication with module is not "
                    "possible.");
            });

        d->outPortSubs.emplace_back(std::move(sub), oport);
        oport->startStream();
    }
}

void MLinkModule::disconnectOutPortForwarders()
{
    // stop listening to messages from external process
    for (auto &pair : d->outPortSubs) {
        pair.second->stopStream();
        d->ioxListener.detachEvent(*pair.first, iox::popo::SubscriberEvent::DATA_RECEIVED);
        pair.first->releaseQueuedData();
    }
}

bool MLinkModule::prepare(const TestSubject &subject)
{
    GlobalConfig gconf;
    bool ret;

    // at this point, ensure the module process is actually running
    if (!isProcessRunning()) {
        if (!runProcess())
            return false;
    }

    auto callSetNiceness = makeClient<iox::popo::Client<SetNicenessRequest, DoneResponse>>(
        SET_NICENESS_CALL_ID.c_str());
    auto callSetMaxRealtimePriority = makeClient<iox::popo::Client<SetMaxRealtimePriority, DoneResponse>>(
        SET_MAX_RT_PRIORITY_CALL_ID.c_str());
    auto callSetPortsPreset = makeUntypedClient(SET_PORTS_PRESET_CALL_ID.c_str());
    auto callLoadScript = makeUntypedClient(LOAD_SCRIPT_CALL_ID.c_str());
    auto callPrepare = makeClient<iox::popo::Client<PrepareStartRequest, DoneResponse>>(PREPARE_START_CALL_ID.c_str());

    // set module process niceness
    ret = callClientSimple(callSetNiceness, [&](auto &request) {
        request->nice = gconf.defaultThreadNice();
    });
    if (!ret)
        return false;

    // set module process realtime priority
    ret = callClientSimple(callSetMaxRealtimePriority, [&](auto &request) {
        request->priority = gconf.defaultRTThreadPriority();
    });
    if (!ret)
        return false;

    // set the ports that are selected on this module
    {
        SetPortsPresetRequest req;
        QList<InputPortChange> ipDef;
        QList<OutputPortChange> opDef;

        for (auto &iport : inPorts()) {
            InputPortChange ipc(PortAction::CHANGE);
            ipc.id = iport->id();
            ipc.dataTypeId = iport->dataTypeId();
            ipc.title = iport->title();
            ipDef << ipc;
        }

        for (auto &oport : outPorts()) {
            OutputPortChange opc(PortAction::CHANGE);
            opc.id = oport->id();
            opc.dataTypeId = oport->dataTypeId();
            opc.title = oport->title();
            opDef << opc;
        }

        req.inPorts = ipDef;
        req.outPorts = opDef;

        ret = callUntypedClientSimple(callSetPortsPreset, req);
        if (!ret)
            return false;
    }

    // set the script to be run, if any exists
    if (!d->scriptContent.isEmpty()) {
        LoadScriptRequest req;
        req.workingDir = d->scriptWDir;
        req.venvDir = d->pyVenvDir;
        req.script = d->scriptContent;
        ret = callUntypedClientSimple(callLoadScript, req);
        if (!ret)
            return false;
    }

    // call the module's own startup preparations
    ret = callClientSimple(callPrepare, [&](auto &request) {
        request->settings = d->settingsData;
    });
    if (!ret)
        return false;

    // register output port forwarding from exported data streams to internal data transmission
    registerOutPortForwarders();
    if (state() == ModuleState::ERROR)
        return false;

    return true;
}

void MLinkModule::start()
{
    d->portChangesAllowed = false;
    auto callStart = makeClient<iox::popo::Client<StartRequest, DoneResponse>>(START_CALL_ID.c_str());

    auto timestampUs =
        std::chrono::duration_cast<std::chrono::microseconds>(m_syTimer->currentTimePoint().time_since_epoch()).count();
    callClientSimple(callStart, [&](auto &request) {
        request->startTimestampUsec = timestampUs;
    });

    AbstractModule::start();
}

void MLinkModule::stop()
{
    disconnectOutPortForwarders();
    d->portChangesAllowed = true;
    AbstractModule::stop();
}
