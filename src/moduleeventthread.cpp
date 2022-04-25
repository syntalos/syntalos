/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleeventthread.h"

#include <glib.h>
#include <thread>

#include "utils/misc.h"

using namespace Syntalos;

class TimerEventPayload
{
public:
    uint interval;
    AbstractModule *module;
    intervalEventFunc_t fn;

    ModuleEventThread *self;
    GSource *source;
    GMainContext *context;
};

class RecvDataEventPayload
{
public:
    AbstractModule *module;
    recvDataEventFunc_t fn;

    ModuleEventThread *self;
    GSource *source;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class ModuleEventThread::Private
{
public:
    Private() { }
    ~Private() { }

    QString threadName;
    bool running;
    bool failed;

    bool threadActive;
    std::thread thread;
    std::atomic<GMainLoop *> activeLoop;
};
#pragma GCC diagnostic pop

ModuleEventThread::ModuleEventThread(const QString &threadName, QObject *parent)
    : QObject(parent),
      d(new ModuleEventThread::Private)
{
    d->activeLoop = nullptr;
    d->running = false;
    d->failed = false;
    d->threadActive = false;
    if (threadName.isEmpty())
        d->threadName = QStringLiteral("ev:%1").arg(createRandomString(9));
    else
        d->threadName = QStringLiteral("ev:%1").arg(threadName);
}

ModuleEventThread::~ModuleEventThread()
{
    shutdownThread();
}

bool ModuleEventThread::isRunning() const
{
    return d->running;
}

bool ModuleEventThread::isFailed() const
{
    return d->failed;
}

QString ModuleEventThread::threadName() const
{
    return d->threadName;
}

void ModuleEventThread::setFailed(bool failed)
{
    d->failed = failed;
}

static gboolean timerEventDispatch(gpointer udata)
{
    const auto pl = static_cast<TimerEventPayload*>(udata);
    int interval = pl->interval;
    std::invoke(pl->fn, pl->module, interval);

    if (pl->module->state() == ModuleState::ERROR) {
        // ewww, this module failed. suspend execution
        pl->self->setFailed(true);
        qDebug().noquote().nospace() << "Module '" << pl->module->name() << "' failed in event loop. Stopping.";
        return FALSE;
    }

    // interval wasn't changed, we continue as normal
    if (interval == (int)pl->interval)
        return TRUE;

    // interval < 0 means we should stop this event source
    if (interval < 0)
        return FALSE;

    // if we are here, the interval was adjusted, so we create a new event source to be called
    // at a different interval and relace the old one
    pl->interval = interval;

    g_source_destroy(pl->source);
    g_source_unref(pl->source);
    pl->source = g_timeout_source_new(pl->interval);
    g_source_set_callback (pl->source,
                           &timerEventDispatch,
                           pl,
                           NULL);
    g_source_attach(pl->source, pl->context);

    return FALSE;
}

static gboolean recvDataEventDispatch(gpointer udata)
{
    const auto pl = static_cast<RecvDataEventPayload*>(udata);
    std::invoke(pl->fn, pl->module);

    if (pl->module->state() == ModuleState::ERROR) {
        // ewww, this module failed. suspend execution
        pl->self->setFailed(true);
        qDebug().noquote().nospace() << "Module '" << pl->module->name() << "' failed in event loop. Stopping.";
        return FALSE;
    }

    return TRUE;
}

typedef struct {
    GSource source;
    int event_fd;
    gpointer event_fd_tag;
} EFDSignalSource;

gboolean efd_signal_source_prepare(GSource*, gint *timeout)
{
    *timeout = -1;
    return FALSE;
}

gboolean efd_signal_source_dispatch(GSource* source, GSourceFunc callback, gpointer user_data)
{
    EFDSignalSource* efd_source = (EFDSignalSource*) source;

    unsigned events = g_source_query_unix_fd(source, efd_source->event_fd_tag);
    if (events & G_IO_HUP || events & G_IO_ERR || events & G_IO_NVAL) {
        return G_SOURCE_REMOVE;
    }

    gboolean result_continue = G_SOURCE_CONTINUE;
    if (events & G_IO_IN) {
        uint64_t buffer;
        // just read the buffer count for now to empty it
        // (maybe we can do something useful with the element count later?)
        if (G_UNLIKELY (read(efd_source->event_fd, &buffer, sizeof(buffer)) == -1))
            qWarning().noquote() << "Failed to read from eventfd:" << g_strerror(errno);

        result_continue = callback(user_data);
    }
    g_source_set_ready_time(source, -1);
    return result_continue;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static GSourceFuncs efd_source_funcs = {
  .prepare = efd_signal_source_prepare,
  .check = NULL,
  .dispatch = efd_signal_source_dispatch,
  .finalize = NULL
};
#pragma GCC diagnostic pop

GSource *efd_signal_source_new(int event_fd)
{
    auto source = (EFDSignalSource*) g_source_new(&efd_source_funcs, sizeof(EFDSignalSource));
    source->event_fd = event_fd;
    source->event_fd_tag = g_source_add_unix_fd((GSource*) source,
                                                event_fd,
                                                (GIOCondition) (G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL));
    return (GSource*) source;
}

void ModuleEventThread::moduleEventThreadFunc(QList<AbstractModule*> mods, OptionalWaitCondition *waitCondition)
{
    pthread_setname_np(pthread_self(), qPrintable(d->threadName.mid(0, 15)));
    g_autoptr(GMainContext) context = g_main_context_new();
    g_main_context_push_thread_default(context);
    g_autoptr(GMainLoop) loop = g_main_loop_new(context, FALSE);
    d->activeLoop = loop;

    // add event sources
    std::vector<std::unique_ptr<TimerEventPayload>> intervalPayloads;
    std::vector<std::unique_ptr<RecvDataEventPayload>> recvDataPayloads;
    for (const auto &mod : mods) {
        // add "timer" event sources
        for (const auto &ev : mod->intervalEventCallbacks()) {
            if (ev.second < 0)
                continue;

            auto pl = std::make_unique<TimerEventPayload>();
            pl->interval = ev.second;
            pl->module = mod;
            pl->fn = ev.first;
            pl->self = this;
            pl->context = context;
            pl->source = g_timeout_source_new(pl->interval);
            g_source_set_callback (pl->source,
                                   &timerEventDispatch,
                                   pl.get(),
                                   NULL);
            g_source_attach(pl->source, context);
            intervalPayloads.push_back(std::move(pl));
        }

        // add "received data in subscription" event sources
        for (const auto &ev : mod->recvDataEventCallbacks()) {
            auto sub = ev.second;
            int eventfd = sub->enableNotify();

            auto pl = std::make_unique<RecvDataEventPayload>();
            pl->module = mod;
            pl->fn = ev.first;
            pl->self = this;
            pl->source = efd_signal_source_new(eventfd);
            g_source_set_callback (pl->source,
                                   &recvDataEventDispatch,
                                   pl.get(),
                                   NULL);
            g_source_attach(pl->source, context);
            recvDataPayloads.push_back(std::move(pl));
        }
    }

    // wait for us to start
    waitCondition->wait();

    // check if any module signals that it will actually not be doing anything
    // (if so, we don't need to call it and can maybe even terminate this thread)
    QMutableListIterator<AbstractModule*> i(mods);
    while (i.hasNext()) {
        if (i.next()->state() == ModuleState::IDLE)
            i.remove();
    }

    // used later to measure wait time on shutdown
    symaster_timepoint tpWaitStart;

    if (mods.isEmpty()) {
        qDebug().noquote() << "All evented modules are idle, shutting down their thread.";
        d->activeLoop = nullptr;
        goto out;
    }

    // immediately return in case other modules have already failed
    if (d->failed) {
        d->activeLoop = nullptr;
        goto out;
    }

    // if we are already stopped, do nothing
    if (!d->running)
        goto out;

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

    // clean up sources (shouldn't be necessary, but we do it anyway)
    for (const auto &pl : intervalPayloads) {
        g_source_destroy(pl->source);
        g_source_unref(pl->source);
    }
    for (const auto &pl : recvDataPayloads) {
        g_source_destroy(pl->source);
        g_source_unref(pl->source);
    }
}

void ModuleEventThread::run(QList<AbstractModule*> mods, OptionalWaitCondition *waitCondition)
{
    if (d->threadActive)
        return;

    d->running = true;
    d->threadActive = true;
    d->thread = std::thread(&ModuleEventThread::moduleEventThreadFunc, this,
                            mods,
                            waitCondition);
}

void ModuleEventThread::stop()
{
    shutdownThread();
}

void ModuleEventThread::shutdownThread()
{
    if (!d->threadActive)
        return;
    d->running = false;
    if (d->activeLoop != nullptr)
        g_main_loop_quit(d->activeLoop);
    d->thread.join();
    d->threadActive = false;
}
