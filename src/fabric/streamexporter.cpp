/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "streamexporter.h"

#include <glib.h>
#include <thread>

#include "streams/stream.h"
#include "utils/misc.h"
#include "mlinkmodule.h"
#include "mlink/ipc-types-private.h"
#include "mlink/ipc-iox-private.h"
#include "mlink/syntaloslink.h"

using namespace Syntalos;
using namespace Syntalos::ipc;

static QuillLogger *g_logSExport = nullptr;

struct StreamExportData {
    std::optional<SyPublisher> publisher;
    std::shared_ptr<VariantStreamSubscription> subscription;

    // reused memory segment to avoid memory fragmentation
    ByteVector buffer;

    StreamExporter *self{};
    GSource *source{};
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class StreamExporter::Private
{
public:
    Private() = default;
    ~Private() = default;

    std::optional<iox2::Node<iox2::ServiceType::Ipc>> node;

    QuillLogger *log;
    QString threadName;
    std::atomic_bool running;
    std::atomic_bool failed;

    std::atomic_bool threadActive;
    std::thread thread;
    std::atomic<GMainLoop *> activeLoop;

    std::vector<StreamExportData> exports;
    QSet<QString> exportedIds;
};
#pragma GCC diagnostic pop

StreamExporter::StreamExporter(const QString &threadName, QObject *parent)
    : QObject(parent),
      d(new StreamExporter::Private)
{
    d->log = getLogger("stream-exporter");
    d->activeLoop = nullptr;
    d->running = false;
    d->failed = false;
    d->threadActive = false;
    if (threadName.isEmpty())
        d->threadName = QStringLiteral("se:%1").arg(createRandomString(9));
    else
        d->threadName = QStringLiteral("se:%1").arg(threadName);

    d->node.emplace(makeIoxNode("syntalos-stream-exporter"));
}

StreamExporter::~StreamExporter()
{
    stop();
}

bool StreamExporter::isRunning() const
{
    return d->running;
}

bool StreamExporter::isFailed() const
{
    return d->failed;
}

QString StreamExporter::threadName() const
{
    return d->threadName;
}

void StreamExporter::setFailed(bool failed)
{
    d->failed = failed;
}

static void ipcLogDispatch(datactl::LogSeverity severity, const std::string &msg)
{
    LOG_DYNAMIC(g_logSExport, static_cast<quill::LogLevel>(severity), "{}", msg);
}

auto StreamExporter::publishStreamByPort(const std::shared_ptr<VarStreamInputPort> &iport)
    -> std::expected<std::optional<ExportedStreamInfo>, QString>
{
    // we don't export unsubscribed ports
    if (!iport->hasSubscription())
        return std::nullopt;

    // create unique ID for this output port
    auto modId = QStringLiteral("%1_%2")
                     .arg(iport->outPort()->owner()->id().mid(0, 100))
                     .arg(iport->outPort()->owner()->index());
    auto channelId = QStringLiteral("o/%1").arg(iport->outPort()->id());

    ExportedStreamInfo result;
    result.instanceId = modId;
    result.channelId = channelId;

    // if the emitter is an MLink module, the stream is already exported, and we just return its expected info
    if (dynamic_cast<MLinkModule *>(iport->outPort()->owner()) != nullptr)
        return result;

    // If this exact stream is already being exported, the IPC publisher is already
    // set up and IOX takes care of fan-out to multiple IPC subscribers.
    // Register a sink-only entry (no publisher): on first dispatch the subscription
    // will be suspended and the event source removed, so after one wakeup neither
    // the queue nor the event loop carry any overhead for this duplicate.
    // NOTE: We can't suspend here already, as modules may still reset() the stream
    if (d->exportedIds.contains(modId + channelId)) {
        StreamExportData drainEd;
        drainEd.self = this;
        drainEd.subscription = iport->subscriptionVar();
        drainEd.publisher = std::nullopt;
        d->exports.push_back(std::move(drainEd));
        return result;
    }

    StreamExportData edata;
    edata.self = this;
    edata.subscription = iport->subscriptionVar();

    // The output port may have multiple MLink consumers; size the IPC service
    // from the currently connected downstream subscribers.
    // This means we will likely overcommit on possible subscribers, since some subscribers may be in-process,
    // but this is an acceptable tradeoff for now. If we want to be more efficient in future, we can propagate
    // the subscriber's type through the system and count only MLink modules here.
    const auto topology = makeIpcServiceTopology(1, iport->outPort()->streamVar()->subscriberCount());

    try {
        edata.publisher.emplace(
            SyPublisher::create(*d->node, modId.toStdString(), channelId.toStdString(), topology, ipcLogDispatch));
    } catch (const std::exception &ex) {
        return std::unexpected("Failed to set up IPC export for " + modId + "/" + channelId + ": " + ex.what());
    }

    // register
    d->exports.push_back(std::move(edata));
    d->exportedIds.insert(modId + channelId);

    return result;
}

static gboolean recvStreamEventDispatch(gpointer udata)
{
    const auto ed = static_cast<StreamExportData *>(udata);

    // Duplicate-subscription entry: suspend to drain the queue and prevent future
    // enqueues, then evict this source from the event loop.
    if (!ed->publisher.has_value()) {
        ed->subscription->suspend();
        return G_SOURCE_REMOVE;
    }

    auto sendFn = [&ed](const BaseDataType &data) {
        try {
            auto memSize = data.memorySize();
            if (memSize < 0) {
                // we do not know the required memory size in advance, so we need to
                // perform a serialization and extra copy operation
                data.toBytes(ed->buffer);
                auto slice = ed->publisher->loanSlice(static_cast<size_t>(ed->buffer.size()));
                std::memcpy(slice.payload_mut().data(), ed->buffer.data(), ed->buffer.size());
                ed->publisher->sendSlice(std::move(slice));
            } else {
                // Higher efficiency code-path since the size is known in advance
                auto slice = ed->publisher->loanSlice(static_cast<size_t>(memSize));
                if (!data.writeToMemory(slice.payload_mut().data(), static_cast<ssize_t>(memSize))) {
                    LOG_ERROR(g_logSExport, "Failed to serialize data for export!");
                    return;
                }
                ed->publisher->sendSlice(std::move(slice));
            }
        } catch (const std::exception &e) {
            LOG_ERROR(g_logSExport, "Failed to transmit sample to other process: {}", e.what());
        }
    };

    // Send up to 20 samples in one go, but do not try this if we are shutting down,
    // as the client we want to communicate with may have crashed.
    // If we try to communicate with a crashed client, we will wait for a long time
    // and might run out of memory meanwhile.
    for (uint i = 0; i < 20; i++) {
        if (!ed->self->isRunning())
            break;
        if (!ed->subscription->callIfNextVar(sendFn))
            break;
    }

    // handle any auxiliary publisher events
    ed->publisher->handleEvents();

    return TRUE;
}

typedef struct {
    GSource source;
    int event_fd;
    gpointer event_fd_tag;
} EFDSignalSource;

static gboolean efd_signal_source_prepare(GSource *, gint *timeout)
{
    *timeout = -1;
    return FALSE;
}

static gboolean efd_signal_source_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    EFDSignalSource *efd_source = (EFDSignalSource *)source;

    unsigned events = g_source_query_unix_fd(source, efd_source->event_fd_tag);
    if (events & G_IO_HUP || events & G_IO_ERR || events & G_IO_NVAL) {
        return G_SOURCE_REMOVE;
    }

    gboolean result_continue = G_SOURCE_CONTINUE;
    if (events & G_IO_IN) {
        uint64_t buffer;
        // just read the buffer count for now to empty it
        // (maybe we can do something useful with the element count later?)
        if (G_UNLIKELY(read(efd_source->event_fd, &buffer, sizeof(buffer)) == -1 && errno != EAGAIN))
            LOG_WARNING(g_logSExport, "Failed to read from eventfd: {}", g_strerror(errno));

        result_continue = callback(user_data);
    }
    g_source_set_ready_time(source, -1);
    return result_continue;
}

static GSourceFuncs efd_source_funcs =
    {.prepare = efd_signal_source_prepare, .check = NULL, .dispatch = efd_signal_source_dispatch, .finalize = NULL};

static GSource *efd_signal_source_new(int event_fd)
{
    auto source = (EFDSignalSource *)g_source_new(&efd_source_funcs, sizeof(EFDSignalSource));
    source->event_fd = event_fd;
    source->event_fd_tag = g_source_add_unix_fd(
        (GSource *)source, event_fd, (GIOCondition)(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL));
    return (GSource *)source;
}

void StreamExporter::streamEventThreadFunc(OptionalWaitCondition *waitCondition)
{
    pthread_setname_np(pthread_self(), qPrintable(d->threadName.mid(0, 15)));
    g_autoptr(GMainContext) context = g_main_context_new();
    g_main_context_push_thread_default(context);
    g_autoptr(GMainLoop) loop = g_main_loop_new(context, FALSE);
    d->activeLoop = loop;

    // register events for all streams to be published
    for (auto &ed : d->exports) {
        int eventfd = ed.subscription->enableNotify();

        ed.source = efd_signal_source_new(eventfd);
        g_source_set_callback(ed.source, &recvStreamEventDispatch, &ed, NULL);
        g_source_attach(ed.source, context);
    }

    // wait for us to start
    waitCondition->wait();

    // used later to measure wait time on shutdown
    symaster_timepoint tpWaitStart;

    // immediately return in case other modules have already failed
    if (d->failed)
        goto out;

    // if we are already stopped, do nothing
    if (!d->running)
        goto out;

    // Process any SubscriberConnected events that arrived during module startup
    // before any data flows. This ensures update_connections() is called on all
    // publishers so late-joining subscribers are in the publisher's list before
    // the first data item is sent. Without this, history-delivered samples arrive
    // silently (no Sample notification) and the worker never wakes up.
    for (auto &ed : d->exports) {
        if (ed.publisher.has_value())
            ed.publisher->handleEvents();
    }

    // run the event loop
    g_main_loop_run(loop);

    // cleanup and process remaining events
    tpWaitStart = symaster_clock::now();
    while (timeDiffToNowMsec(tpWaitStart).count() < 1000) {
        if (!g_main_context_iteration(context, FALSE))
            break;
    }

out:
    d->activeLoop = nullptr;

    for (auto &ed : d->exports) {
        // clean up sources (shouldn't be necessary, but we do it anyway)
        g_source_destroy(ed.source);
        g_source_unref(ed.source);

        // drain any pending data
        ed.subscription->clearPending();
    }
}

void StreamExporter::run(OptionalWaitCondition *waitCondition)
{
    if (d->threadActive)
        return;

    d->running = true;
    d->threadActive = true;
    d->thread = std::thread(&StreamExporter::streamEventThreadFunc, this, waitCondition);
}

void StreamExporter::stop()
{
    shutdownThread();
    for (auto &ed : d->exports) {
        // clear any data that might be left in the subscription
        ed.subscription->clearPending();
    }
}

void StreamExporter::shutdownThread()
{
    if (!d->threadActive)
        return;
    d->running = false;
    if (d->activeLoop != nullptr)
        g_main_loop_quit(d->activeLoop);
    d->thread.join();
    d->threadActive = false;
}
