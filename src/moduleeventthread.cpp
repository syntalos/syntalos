/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "moduleeventthread.h"

#include <glib.h>
#include <thread>

#include "utils.h"

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

ModuleEventThread::ModuleEventThread(QObject *parent)
    : QObject(parent),
      d(new ModuleEventThread::Private)
{
    d->activeLoop = nullptr;
    d->running = false;
    d->failed = false;
    d->threadActive = false;
    d->threadName = QStringLiteral("evexec_%1").arg(createRandomString(4));
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
    shutdownThread();
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

void ModuleEventThread::moduleEventThreadFunc(QList<AbstractModule*> mods, OptionalWaitCondition *waitCondition)
{
    pthread_setname_np(pthread_self(), qPrintable(d->threadName.mid(0, 15)));
    g_autoptr(GMainContext) context = g_main_context_new();
    g_main_context_push_thread_default(context);
    g_autoptr(GMainLoop) loop = g_main_loop_new(context, FALSE);
    d->activeLoop = loop;

    // wait for us to start
    waitCondition->wait();

    // immediately return on early failures by other modules
    if (d->failed) {
        d->activeLoop = nullptr;
        return;
    }

    // check if any module signals that it will actually not be doing anything
    // (if so, we don't need to call it and can maybe even terminate this thread)
    QMutableListIterator<AbstractModule*> i(mods);
    while (i.hasNext()) {
        if (i.next()->state() == ModuleState::IDLE)
            i.remove();
    }

    if (mods.isEmpty()) {
        qDebug() << "All evented modules are idle, shutting down their thread.";
        d->activeLoop = nullptr;
        return;
    }

    // add timer event sources
    std::vector<std::unique_ptr<TimerEventPayload>> intervalPayloads;
    for (const auto &mod : mods) {
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
    }

    // if we are already stopped, do nothing
    if (!d->running)
        goto out;

    // run the event loop
    g_main_loop_run(loop);

out:
    d->activeLoop = nullptr;

    // clean up sources
    for (const auto &pl : intervalPayloads) {
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
