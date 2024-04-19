/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "syntaloslink.h"

#include <QDebug>
#include <QBuffer>
#include <QCoreApplication>
#include <signal.h>
#include <sys/prctl.h>
#include <iceoryx_posh/runtime/posh_runtime.hpp>
#include <iceoryx_posh/popo/server.hpp>
#include <iceoryx_posh/popo/untyped_server.hpp>
#include <iceoryx_posh/popo/publisher.hpp>
#include <iceoryx_posh/popo/untyped_publisher.hpp>
#include <iceoryx_posh/popo/untyped_subscriber.hpp>
#include <iceoryx_hoofs/posix_wrapper/signal_watcher.hpp>
#include <iceoryx_hoofs/log/logmanager.hpp>

#include "ipc-types-private.h"
#include "rtkit.h"
#include "cpuaffinity.h"

using namespace Syntalos;

namespace Syntalos
{

std::unique_ptr<SyntalosLink> initSyntalosModuleLink()
{
    auto syModuleId = qgetenv("SYNTALOS_MODULE_ID");
    if (syModuleId.isEmpty() || syModuleId.length() < 2)
        throw std::runtime_error("This module was not run by Syntalos, can not continue!");

    // set up stream data type mapping, if it hasn't been initialized yet
    registerStreamMetaTypes();

    char rtName[100];
    const auto rtNameStr = QString::fromUtf8(syModuleId.right(100));
    strncpy(rtName, qPrintable(rtNameStr), sizeof(rtName) - 1);
    rtName[sizeof(rtName) - 1] = '\0';

    // set IOX log level
    auto verboseLevel = qgetenv("SY_VERBOSE");
    if (verboseLevel == "1")
        iox::log::LogManager::GetLogManager().SetDefaultLogLevel(iox::log::LogLevel::kVerbose);
    else
        iox::log::LogManager::GetLogManager().SetDefaultLogLevel(iox::log::LogLevel::kInfo);

    // connect to RouDi
    iox::runtime::PoshRuntime::initRuntime(rtName);

    // ensure we (try to) die if Syntalos, our parent, dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    return std::unique_ptr<SyntalosLink>(new SyntalosLink(rtNameStr));
}

/**
 * Reference for a module input port
 */
class InputPortInfo::Private
{
public:
    explicit Private(const InputPortChange &pc)
    {
        connected = false;
        id = pc.id;
        title = pc.title;
        dataTypeId = pc.dataTypeId;
        metadata = pc.metadata;
    }

    int index;
    bool connected;
    std::unique_ptr<iox::popo::UntypedSubscriber> ioxSub;

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;

    NewDataRawFn newDataCb;
    uint throttleItemsPerSec;
};

InputPortInfo::InputPortInfo(const InputPortChange &pc)
    : d(new InputPortInfo::Private(pc))
{
}

QString InputPortInfo::id() const
{
    return d->id;
}

int InputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

QString InputPortInfo::title() const
{
    return d->title;
}

void InputPortInfo::setNewDataRawCallback(NewDataRawFn callback)
{
    d->newDataCb = std::move(callback);
}

void InputPortInfo::setThrottleItemsPerSec(uint itemsPerSec)
{
    d->throttleItemsPerSec = itemsPerSec;
}

QVariantHash InputPortInfo::metadata() const
{
    return d->metadata;
}

/**
 * Reference for a module output port
 */
class OutputPortInfo::Private
{
public:
    explicit Private(const OutputPortChange &pc)
    {
        connected = false;
        id = pc.id;
        title = pc.title;
        dataTypeId = pc.dataTypeId;
        metadata = pc.metadata;
    }

    int index;
    bool connected;
    std::unique_ptr<iox::popo::UntypedPublisher> ioxPub;

    QString id;
    QString title;
    int dataTypeId;
    QVariantHash metadata;

    iox::capro::IdString_t publisherId() const
    {
        auto channelId = QStringLiteral("oport_%1").arg(id.mid(0, 80));
        return iox::capro::IdString_t(iox::cxx::TruncateToCapacity, channelId.toStdString());
    }
};

OutputPortInfo::OutputPortInfo(const OutputPortChange &pc)
    : d(new OutputPortInfo::Private(pc))
{
}

QString OutputPortInfo::id() const
{
    return d->id;
}

int OutputPortInfo::dataTypeId() const
{
    return d->dataTypeId;
}

void OutputPortInfo::setMetadataVar(const QString &key, const QVariant &value)
{
    d->metadata[key] = value;
}

class SyntalosLink::Private
{
public:
    Private(const QString &instanceId)
    {
        modId = iox::capro::IdString_t(iox::cxx::TruncateToCapacity, instanceId.toStdString());

        // interfaces
        pubError = makePublisher<ErrorEvent>(ERROR_CHANNEL_ID);
        pubState = makePublisher<StateChangeEvent>(STATE_CHANNEL_ID);
        pubStatusMessage = makePublisher<StatusMessageEvent>(STATUS_MESSAGE_CHANNEL_ID, false);
        pubInPortChange = makeUntypedPublisher(IN_PORT_CHANGE_CHANNEL_ID);
        pubOutPortChange = makeUntypedPublisher(OUT_PORT_CHANGE_CHANNEL_ID);

        reqSetNiceness = makeServer<SetNicenessRequest, DoneResponse>(SET_NICENESS_CALL_ID);
        reqSetMaxRTPriority = makeServer<SetMaxRealtimePriority, DoneResponse>(SET_MAX_RT_PRIORITY_CALL_ID);
        reqSetCPUAffinity = makeServer<SetCPUAffinityRequest, DoneResponse>(SET_CPU_AFFINITY_CALL_ID);
        reqLoadScript = makeUntypedServer(LOAD_SCRIPT_CALL_ID);
        reqSetPortsPreset = makeUntypedServer(SET_PORTS_PRESET_CALL_ID);
        reqUpdateIPortMetadata = makeUntypedServer(IN_PORT_UPDATE_METADATA_ID);
        reqConnectIPort = makeServer<ConnectInputRequest, DoneResponse>(CONNECT_INPUT_CALL_ID);
        reqPrepareStart = makeUntypedServer(PREPARE_START_CALL_ID);
        reqStart = makeServer<StartRequest, DoneResponse>(START_CALL_ID);
        reqStop = makeServer<StopRequest, DoneResponse>(STOP_CALL_ID);
        reqShutdown = makeServer<ShutdownRequest, DoneResponse>(SHUTDOWN_CALL_ID);
    }

    ~Private() {}

    template<typename T>
    std::unique_ptr<iox::popo::Publisher<T>> makePublisher(
        const iox::capro::IdString_t &channelName,
        bool waitForConsumer = true)
    {
        iox::popo::PublisherOptions publisherOptn;

        publisherOptn.historyCapacity = SY_IOX_HISTORY_SIZE;
        if (waitForConsumer) {
            // allow the subscriber to block us, to ensure we don't lose data
            publisherOptn.subscriberTooSlowPolicy = iox::popo::ConsumerTooSlowPolicy::WAIT_FOR_CONSUMER;
        }

        return std::make_unique<iox::popo::Publisher<T>>(
            iox::capro::ServiceDescription{"SyntalosModule", modId, channelName}, publisherOptn);
    }

    std::unique_ptr<iox::popo::UntypedPublisher> makeUntypedPublisher(
        const iox::capro::IdString_t &channelName,
        bool waitForConsumer = true)
    {
        iox::popo::PublisherOptions publisherOptn;

        publisherOptn.historyCapacity = SY_IOX_HISTORY_SIZE;
        if (waitForConsumer) {
            // allow the subscriber to block us, to ensure we don't lose data
            publisherOptn.subscriberTooSlowPolicy = iox::popo::ConsumerTooSlowPolicy::WAIT_FOR_CONSUMER;
        }

        return std::make_unique<iox::popo::UntypedPublisher>(
            iox::capro::ServiceDescription{"SyntalosModule", modId, channelName}, publisherOptn);
    }

    template<typename Req, typename Res>
    std::unique_ptr<iox::popo::Server<Req, Res>> makeServer(const iox::cxx::string<100> &callName)
    {
        auto srv = std::make_unique<iox::popo::Server<Req, Res>>(
            iox::capro::ServiceDescription{"SyntalosModule", modId, callName});

        waitSet.attachState(*srv, iox::popo::ServerState::HAS_REQUEST).or_else([](auto) {
            std::cerr << "Failed to attach watcher for " << typeid(Req).name() << " request responder." << std::endl;
            std::exit(EXIT_FAILURE);
        });

        return srv;
    }

    std::unique_ptr<iox::popo::UntypedServer> makeUntypedServer(const iox::cxx::string<100> &callName)
    {
        auto srv = std::make_unique<iox::popo::UntypedServer>(
            iox::capro::ServiceDescription{"SyntalosModule", modId, callName});

        waitSet.attachState(*srv, iox::popo::ServerState::HAS_REQUEST).or_else([](auto) {
            std::cerr << "Failed to attach watcher for untyped request responder." << std::endl;
            std::exit(EXIT_FAILURE);
        });

        return srv;
    }

    std::unique_ptr<iox::popo::UntypedSubscriber> makeUntypedSubscriber(
        const iox::capro::IdString_t &instanceId,
        const iox::capro::IdString_t &channelId)
    {
        iox::popo::SubscriberOptions subOptn;

        // number of elements held for processing by default
        subOptn.queueCapacity = SY_IOX_QUEUE_CAPACITY;

        // number of samples to get if for whatever reason we connected too late
        subOptn.historyRequest = SY_IOX_HISTORY_SIZE;

        // make producer wait for us
        subOptn.queueFullPolicy = iox::popo::QueueFullPolicy::BLOCK_PRODUCER;

        auto subscr = std::make_unique<iox::popo::UntypedSubscriber>(
            iox::capro::ServiceDescription{"SyntalosModule", instanceId, channelId}, subOptn);

        waitSet.attachState(*subscr, iox::popo::SubscriberState::HAS_DATA).or_else([](auto) {
            std::cerr << "Failed to attach watcher for untyped subscriber." << std::endl;
            std::exit(EXIT_FAILURE);
        });

        return subscr;
    }

    iox::capro::IdString_t modId;

    std::unique_ptr<iox::popo::Publisher<ErrorEvent>> pubError;
    std::unique_ptr<iox::popo::Publisher<StateChangeEvent>> pubState;
    std::unique_ptr<iox::popo::Publisher<StatusMessageEvent>> pubStatusMessage;
    std::unique_ptr<iox::popo::UntypedPublisher> pubInPortChange;
    std::unique_ptr<iox::popo::UntypedPublisher> pubOutPortChange;
    std::unique_ptr<iox::popo::Server<SetNicenessRequest, DoneResponse>> reqSetNiceness;
    std::unique_ptr<iox::popo::Server<SetMaxRealtimePriority, DoneResponse>> reqSetMaxRTPriority;
    std::unique_ptr<iox::popo::Server<SetCPUAffinityRequest, DoneResponse>> reqSetCPUAffinity;
    std::unique_ptr<iox::popo::UntypedServer> reqLoadScript;
    std::unique_ptr<iox::popo::UntypedServer> reqSetPortsPreset;
    std::unique_ptr<iox::popo::UntypedServer> reqUpdateIPortMetadata;
    std::unique_ptr<iox::popo::Server<ConnectInputRequest, DoneResponse>> reqConnectIPort;
    std::unique_ptr<iox::popo::UntypedServer> reqPrepareStart;
    std::unique_ptr<iox::popo::Server<StartRequest, DoneResponse>> reqStart;
    std::unique_ptr<iox::popo::Server<StopRequest, DoneResponse>> reqStop;
    std::unique_ptr<iox::popo::Server<ShutdownRequest, DoneResponse>> reqShutdown;

    iox::popo::WaitSet<> waitSet;

    ModuleState state;
    int maxRTPriority;
    std::vector<std::shared_ptr<InputPortInfo>> inPortInfo;
    std::vector<std::shared_ptr<OutputPortInfo>> outPortInfo;
    SyncTimer *syTimer;

    LoadScriptFn loadScriptCb;
    PrepareStartFn prepareStartCb;
    StartFn startCb;
    StopFn stopCb;
    ShutdownFn shutdownCb;
};

SyntalosLink::SyntalosLink(const QString &instanceId, QObject *parent)
    : QObject(parent),
      d(new SyntalosLink::Private(instanceId))
{
    d->syTimer = new SyncTimer;

    // immediately upon creation, we send a message that we are idle now
    setState(ModuleState::IDLE);
}

SyntalosLink::~SyntalosLink()
{
    delete d->syTimer;
}

void SyntalosLink::raiseError(const QString &title, const QString &message)
{
    d->pubError->loan().and_then([&](auto &error) {
        error->title = iox::cxx::string<128>(iox::cxx::TruncateToCapacity, title.toStdString());
        error->message = iox::cxx::string<2048>(iox::cxx::TruncateToCapacity, message.toStdString());
        error.publish();
    });
    setState(ModuleState::ERROR);
}

void SyntalosLink::raiseError(const QString &message)
{
    d->pubError->loan().and_then([&](auto &error) {
        error->message = iox::cxx::string<2048>(iox::cxx::TruncateToCapacity, message.toStdString());
        error.publish();
    });
    setState(ModuleState::ERROR);
}

void SyntalosLink::awaitData(int timeoutUsec)
{
    if (timeoutUsec < 0) {
        auto notificationVector = d->waitSet.wait();
        for (auto &notification : notificationVector) {
            processNotification(notification);
            qApp->processEvents();
        }
    } else {
        auto notificationVector = d->waitSet.timedWait(iox::units::Duration::fromMicroseconds(timeoutUsec));
        for (auto &notification : notificationVector) {
            processNotification(notification);
            qApp->processEvents();
        }
    }
}

void SyntalosLink::awaitDataForever()
{
    while (!iox::posix::hasTerminationRequested()) {
        auto notificationVector = d->waitSet.wait();
        for (auto &notification : notificationVector) {
            processNotification(notification);
            qApp->processEvents();
        }
    }
}

void SyntalosLink::processNotification(const iox::popo::NotificationInfo *notification)
{
    // SetNiceness
    if (notification->doesOriginateFrom(d->reqSetNiceness.get())) {
        d->reqSetNiceness->take().and_then([&](const auto &request) {
            d->reqSetNiceness->loan(request)
                .and_then([&](auto &response) {
                    // apply niceness request immediately to current thread
                    const auto success = setCurrentThreadNiceness(request->nice);
                    response->success = success;

                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to SetNiceness! Error: " << error << std::endl;
                    });
                    if (!success)
                        raiseError("Could not set niceness to " + QString::number(request->nice));
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
    }

    // SetMaxRealtimePriority
    if (notification->doesOriginateFrom(d->reqSetMaxRTPriority.get())) {
        d->reqSetMaxRTPriority->take().and_then([&](const auto &request) {
            d->reqSetMaxRTPriority->loan(request)
                .and_then([&](auto &response) {
                    d->maxRTPriority = request->priority;
                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to SetMaxRealtimePriority! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
    }

    // SetCPUAffinity
    if (notification->doesOriginateFrom(d->reqSetCPUAffinity.get())) {
        d->reqSetCPUAffinity->take().and_then([&](const auto &request) {
            d->reqSetCPUAffinity->loan(request)
                .and_then([&](auto &response) {
                    if (!request->cores.empty()) {
                        thread_set_affinity_from_vec(
                            pthread_self(), std::vector<uint>(request->cores.begin(), request->cores.end()));
                    }

                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to SetCPUAffinity! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
    }

    // Load script
    if (notification->doesOriginateFrom(d->reqLoadScript.get())) {
        LoadScriptRequest scriptReqData;
        d->reqLoadScript->take().and_then([&](auto &requestPayload) {
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
            const auto size = chunkHeader->usedSizeOfChunk();

            scriptReqData = LoadScriptRequest::fromMemory(requestPayload, size);

            auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
            d->reqLoadScript->loan(requestHeader, sizeof(DoneResponse), alignof(DoneResponse))
                .and_then([&](auto &responsePayload) {
                    auto response = static_cast<DoneResponse *>(responsePayload);
                    response->success = true;
                    d->reqLoadScript->send(response).or_else([&](auto &error) {
                        std::cout << "Could not send LoadScript response! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cout << "Could not allocate LoadScript response! Error: " << error << std::endl;
                });

            d->reqLoadScript->releaseRequest(requestPayload);
        });

        // load script after sending a reply if we had a valid request
        if (d->loadScriptCb && !scriptReqData.script.isEmpty())
            d->loadScriptCb(scriptReqData.script, scriptReqData.workingDir);
    }

    // Have the master set all preset ports
    if (notification->doesOriginateFrom(d->reqSetPortsPreset.get())) {
        d->reqSetPortsPreset->take().and_then([&](auto &requestPayload) {
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
            const auto size = chunkHeader->usedSizeOfChunk();

            const auto sppReq = SetPortsPresetRequest::fromMemory(requestPayload, size);

            // override our existing ports with the static ones Syntalos provided
            d->inPortInfo.clear();
            d->outPortInfo.clear();
            for (const auto &ipc : sppReq.inPorts)
                d->inPortInfo.push_back(std::shared_ptr<InputPortInfo>(new InputPortInfo(ipc)));
            for (const auto &opc : sppReq.outPorts) {
                auto oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));
                oport->d->ioxPub = d->makeUntypedPublisher(oport->d->publisherId());
                d->outPortInfo.push_back(oport);
            }

            auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
            d->reqSetPortsPreset->loan(requestHeader, sizeof(DoneResponse), alignof(DoneResponse))
                .and_then([&](auto &responsePayload) {
                    auto response = static_cast<DoneResponse *>(responsePayload);
                    response->success = true;
                    d->reqSetPortsPreset->send(response).or_else([&](auto &error) {
                        std::cout << "Could not send SetPortsPreset response! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cout << "Could not allocate SetPortsPreset response! Error: " << error << std::endl;
                });

            d->reqSetPortsPreset->releaseRequest(requestPayload);
        });
    }

    // ConnectInputPort
    if (notification->doesOriginateFrom(d->reqConnectIPort.get())) {
        d->reqConnectIPort->take().and_then([&](const auto &request) {
            d->reqConnectIPort->loan(request)
                .and_then([&](auto &response) {
                    // find the port
                    const auto portId = QString::fromUtf8(request->portId.c_str());
                    std::shared_ptr<InputPortInfo> iport;
                    for (const auto &ip : d->inPortInfo) {
                        if (ip->id() == portId) {
                            iport = ip;
                            break;
                        }
                    }

                    // return error if the port was not registered
                    if (!iport) {
                        response->success = false;
                        response.send().or_else([&](auto &error) {
                            std::cerr << "Could not respond to ConnectInputPort! Error: " << error << std::endl;
                        });
                        return;
                    }

                    // connect the port
                    iport->d->connected = true;
                    iport->d->ioxSub = d->makeUntypedSubscriber(request->instanceId, request->channelId);

                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to ConnectInputPort! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
    }

    // Update metadata
    if (notification->doesOriginateFrom(d->reqUpdateIPortMetadata.get())) {
        d->reqUpdateIPortMetadata->take().and_then([&](auto &requestPayload) {
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
            const auto size = chunkHeader->usedSizeOfChunk();

            const auto reqUpdateMD = UpdateInputPortMetadataRequest::fromMemory(requestPayload, size);

            // update metadata
            for (const auto &ip : d->inPortInfo) {
                if (ip->id() == reqUpdateMD.id) {
                    ip->d->metadata = reqUpdateMD.metadata;
                    break;
                }
            }

            auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
            d->reqUpdateIPortMetadata->loan(requestHeader, sizeof(DoneResponse), alignof(DoneResponse))
                .and_then([&](auto &responsePayload) {
                    auto response = static_cast<DoneResponse *>(responsePayload);
                    response->success = true;
                    d->reqUpdateIPortMetadata->send(response).or_else([&](auto &error) {
                        std::cout << "Could not send UpdateInputPortMetadata response! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cout << "Could not allocate UpdateInputPortMetadata response! Error: " << error << std::endl;
                });

            d->reqUpdateIPortMetadata->releaseRequest(requestPayload);
        });
    }

    // Prepare start
    if (notification->doesOriginateFrom(d->reqPrepareStart.get())) {
        bool runPrepareRequested = false;
        QByteArray prepareSettings;

        d->reqPrepareStart->take().and_then([&](auto &requestPayload) {
            const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(requestPayload);
            const auto size = chunkHeader->usedSizeOfChunk();

            const auto req = PrepareStartRequest::fromMemory(requestPayload, size);
            runPrepareRequested = true;
            prepareSettings = req.settings;

            auto requestHeader = iox::popo::RequestHeader::fromPayload(requestPayload);
            d->reqPrepareStart->loan(requestHeader, sizeof(DoneResponse), alignof(DoneResponse))
                .and_then([&](auto &responsePayload) {
                    auto response = static_cast<DoneResponse *>(responsePayload);
                    response->success = true;
                    d->reqPrepareStart->send(response).or_else([&](auto &error) {
                        std::cout << "Could not send PrepareStart response! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cout << "Could not allocate PrepareStart response! Error: " << error << std::endl;
                });

            d->reqPrepareStart->releaseRequest(requestPayload);
        });
        if (runPrepareRequested) {
            // call our preparation delegate
            if (d->prepareStartCb)
                d->prepareStartCb(prepareSettings);
        }
    }

    // Handle start request
    if (notification->doesOriginateFrom(d->reqStart.get())) {
        bool runStartRequested = false;
        d->reqStart->take().and_then([&](const auto &request) {
            d->reqStart->loan(request)
                .and_then([&](auto &response) {
                    // NOTE: We reply immediately here and defer processing of the call,
                    // so the master will not wait for us. Errors are reported exclusively
                    // via the error channel.

                    const auto timePoint = symaster_timepoint(microseconds_t(request->startTimestampUsec));
                    d->syTimer->startAt(timePoint);
                    runStartRequested = true;

                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to Start! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
        if (runStartRequested) {
            // execute start action after replying to master
            if (d->startCb)
                d->startCb();
        }
    }

    // Handle stop request
    if (notification->doesOriginateFrom(d->reqStop.get())) {
        d->reqStop->take().and_then([&](const auto &request) {
            d->reqStop->loan(request)
                .and_then([&](auto &response) {
                    if (d->stopCb)
                        d->stopCb();

                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to Stop! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
    }

    // Handle shutdown
    if (notification->doesOriginateFrom(d->reqShutdown.get())) {
        bool shutdownRequested = false;
        d->reqShutdown->take().and_then([&](const auto &request) {
            d->reqShutdown->loan(request)
                .and_then([&](auto &response) {
                    // NOTE: We reply immediately here and defer processing of the call,
                    // because otherwise the master would never get a response if we
                    // tear down the process too quickly.

                    shutdownRequested = true;

                    response->success = true;
                    response.send().or_else([&](auto &error) {
                        std::cerr << "Could not respond to Start! Error: " << error << std::endl;
                    });
                })
                .or_else([&](auto &error) {
                    std::cerr << "Could not allocate response! Error: " << error << std::endl;
                });
        });
        if (shutdownRequested) {
            // execute shutdown action after replying to master
            // if no callback is defined, we just exit()
            if (d->shutdownCb)
                d->shutdownCb();
            else
                qApp->quit();
        }
    }

    // Data received
    for (auto &iport : d->inPortInfo) {
        if (!iport->d->connected)
            continue;

        iport->d->ioxSub->take()
            .and_then([&](const void *payload) {
                const auto chunkHeader = iox::mepoo::ChunkHeader::fromUserPayload(payload);
                const auto size = chunkHeader->usedSizeOfChunk();

                // call raw data received callback
                if (iport->d->newDataCb)
                    iport->d->newDataCb(payload, size);

                // release memory chunk
                iport->d->ioxSub->release(payload);
            })
            .or_else([](auto &result) {
                if (result != iox::popo::ChunkReceiveResult::NO_CHUNK_AVAILABLE) {
                    qWarning().noquote() << "Failed to receive new input port info!";
                }
            });
    }
}

ModuleState SyntalosLink::state() const
{
    return d->state;
}

void SyntalosLink::setState(ModuleState state)
{
    d->pubState->loan().and_then([&](auto &sample) {
        sample->state = state;
        sample.publish();
    });

    d->state = state;
}

void SyntalosLink::setStatusMessage(const QString &message)
{
    d->pubStatusMessage->loan().and_then([&](auto &sample) {
        sample->text = iox::cxx::string<512>(iox::cxx::TruncateToCapacity, message.toStdString());
        sample.publish();
    });
}

int SyntalosLink::maxRealtimePriority() const
{
    return d->maxRTPriority;
}

void SyntalosLink::setLoadScriptCallback(LoadScriptFn callback)
{
    d->loadScriptCb = std::move(callback);
}

void SyntalosLink::setPrepareStartCallback(PrepareStartFn callback)
{
    d->prepareStartCb = std::move(callback);
}

void SyntalosLink::setStartCallback(StartFn callback)
{
    d->startCb = std::move(callback);
}

void SyntalosLink::setStopCallback(StopFn callback)
{
    d->stopCb = std::move(callback);
}

void SyntalosLink::setShutdownCallback(ShutdownFn callback)
{
    d->shutdownCb = std::move(callback);
}

SyncTimer *SyntalosLink::timer() const
{
    return d->syTimer;
}

std::vector<std::shared_ptr<InputPortInfo>> SyntalosLink::inputPorts() const
{
    return d->inPortInfo;
}

std::vector<std::shared_ptr<OutputPortInfo>> SyntalosLink::outputPorts() const
{
    return d->outPortInfo;
}

std::shared_ptr<InputPortInfo> SyntalosLink::registerInputPort(
    const QString &id,
    const QString &title,
    const QString &dataTypeName)
{
    // construct our reference for this port
    InputPortChange ipc(PortAction::ADD);
    ipc.id = id;
    ipc.title = title;
    ipc.dataTypeId = BaseDataType::typeIdFromString(dataTypeName);

    const auto iportData = ipc.toBytes();

    // announce the new port to master
    bool haveError = false;
    d->pubInPortChange->loan(iportData.size())
        .and_then([&](auto &payload) {
            // we copy twice here - but this is a low-volume event, so it should be fine
            memcpy(payload, iportData.data(), iportData.size());
            d->pubInPortChange->publish(payload);
        })
        .or_else([&](auto &error) {
            std::cerr << "Unable to loan sample. Error: " << error << std::endl;
            haveError = true;
        });

    if (haveError) {
        return nullptr;
    } else {
        auto iport = std::shared_ptr<InputPortInfo>(new InputPortInfo(ipc));
        d->inPortInfo.push_back(iport);
        return iport;
    }
}

std::shared_ptr<OutputPortInfo> SyntalosLink::registerOutputPort(
    const QString &id,
    const QString &title,
    const QString &dataTypeName,
    const QVariantHash &metadata)
{
    // construct our reference for this port
    OutputPortChange opc(PortAction::ADD);
    opc.id = id;
    opc.title = title;
    opc.dataTypeId = BaseDataType::typeIdFromString(dataTypeName);
    opc.metadata = metadata;

    const auto oportData = opc.toBytes();

    // announce the new port to master
    bool haveError = false;
    d->pubOutPortChange->loan(oportData.size())
        .and_then([&](auto &payload) {
            // we copy twice here - but this is a low-volume event, so it should be fine
            memcpy(payload, oportData.data(), oportData.size());
            d->pubOutPortChange->publish(payload);
        })
        .or_else([&](auto &error) {
            std::cerr << "Unable to loan sample. Error: " << error << std::endl;
            haveError = true;
        });

    if (haveError) {
        return nullptr;
    } else {
        auto oport = std::shared_ptr<OutputPortInfo>(new OutputPortInfo(opc));
        oport->d->ioxPub = d->makeUntypedPublisher(oport->d->publisherId());
        d->outPortInfo.push_back(oport);
        return oport;
    }
}

void SyntalosLink::updateOutputPort(const std::shared_ptr<OutputPortInfo> &oport)
{
    OutputPortChange opc(PortAction::CHANGE);
    opc.id = oport->id();
    opc.title = oport->d->title;
    opc.dataTypeId = oport->dataTypeId();
    opc.metadata = oport->d->metadata;

    const auto oportData = opc.toBytes();
    d->pubOutPortChange->loan(oportData.size())
        .and_then([&](auto &payload) {
            // we copy twice here - but this is a low-volume event, so it should be fine
            memcpy(payload, oportData.data(), oportData.size());
            d->pubOutPortChange->publish(payload);
        })
        .or_else([&](auto &error) {
            std::cerr << "Unable to loan sample. Error: " << error << std::endl;
        });
}

void SyntalosLink::updateInputPort(const std::shared_ptr<InputPortInfo> &iport)
{
    InputPortChange ipc(PortAction::CHANGE);
    ipc.id = iport->id();
    ipc.title = iport->d->title;
    ipc.dataTypeId = iport->d->dataTypeId;
    ipc.metadata = iport->d->metadata;
    ipc.throttleItemsPerSec = iport->d->throttleItemsPerSec;

    const auto iportData = ipc.toBytes();
    d->pubInPortChange->loan(iportData.size())
        .and_then([&](auto &payload) {
            // we copy twice here - but this is a low-volume event, so it should be fine
            memcpy(payload, iportData.data(), iportData.size());
            d->pubInPortChange->publish(payload);
        })
        .or_else([&](auto &error) {
            std::cerr << "Unable to loan sample. Error: " << error << std::endl;
        });
}

bool SyntalosLink::submitOutput(const std::shared_ptr<OutputPortInfo> &oport, const BaseDataType &data)
{
    auto memSize = data.memorySize();
    if (memSize < 0) {
        // we do not know the required memory size in advance, so we need to
        // perform a serialization and extra copy operation
        const auto bytes = data.toBytes();

        oport->d->ioxPub->loan(bytes.size())
            .and_then([&](auto &payload) {
                memcpy(payload, bytes.data(), bytes.size());
                oport->d->ioxPub->publish(payload);
            })
            .or_else([&](auto &error) {
                std::cerr << "Unable to loan sample. Error: " << error << std::endl;
            });
    } else {
        // Higher efficiency code-path since the size is known in advance
        oport->d->ioxPub->loan(memSize)
            .and_then([&](auto &payload) {
                if (!data.writeToMemory(payload, memSize))
                    std::cerr << "Failed to write data to shared memory!" << std::endl;
                oport->d->ioxPub->publish(payload);
            })
            .or_else([&](auto &error) {
                std::cerr << "Unable to loan sample. Error: " << error << std::endl;
            });
    }

    return true;
}

} // namespace Syntalos
