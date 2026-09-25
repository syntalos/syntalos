/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "termsignalwatcher.h"

#include <QSocketNotifier>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "logging.h"
#include "mlink/ipc-iox-private.h"

namespace Syntalos
{

static int g_sigPipe[2] = {-1, -1};
static constexpr int kTermSignals[] = {SIGINT, SIGTERM};

static void termSignalHandler(int signum)
{
    const int savedErrno = errno;
    const auto sigByte = static_cast<unsigned char>(signum);
    [[maybe_unused]] const auto ret = ::write(g_sigPipe[1], &sigByte, 1);
    errno = savedErrno;
}

static bool installHandler(int signum)
{
    struct sigaction sa = {};
    sa.sa_handler = termSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    return sigaction(signum, &sa, nullptr) == 0;
}

TermSignalWatcher::TermSignalWatcher(QObject *parent)
    : QObject(parent)
{
    auto log = getLogger("main");
    Q_ASSERT(g_sigPipe[0] < 0);
    if (pipe2(g_sigPipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        LOG_WARNING(
            log,
            "Unable to create signal pipe, SIGINT/SIGTERM will not stop Syntalos cleanly: {}",
            std::strerror(errno));
        return;
    }

    // iceoryx2 would otherwise replace our handlers the first time it is used
    ipc::initIoxSignalHandlers();
    for (const auto signum : kTermSignals) {
        if (!installHandler(signum))
            LOG_WARNING(log, "Unable to install handler for SIG{}: {}", sigabbrev_np(signum), std::strerror(errno));
    }

    m_notifier = new QSocketNotifier(g_sigPipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this]() {
        unsigned char sigByte;
        while (::read(g_sigPipe[0], &sigByte, 1) == 1)
            emit terminationRequested(sigByte);
    });
}

TermSignalWatcher::~TermSignalWatcher()
{
    if (g_sigPipe[0] < 0)
        return;
    for (const auto signum : kTermSignals)
        std::signal(signum, SIG_DFL);
    delete m_notifier;
    ::close(g_sigPipe[0]);
    ::close(g_sigPipe[1]);
    g_sigPipe[0] = g_sigPipe[1] = -1;
}

bool TermSignalWatcher::ensureHandlersInstalled()
{
    if (g_sigPipe[0] < 0)
        return false;

    bool intact = true;
    for (const auto signum : kTermSignals) {
        struct sigaction current = {};
        sigaction(signum, nullptr, &current);
        if (current.sa_handler == termSignalHandler)
            continue;
        intact = false;
        LOG_WARNING(getLogger("main"), "Handler for SIG{} was replaced, reinstalling it.", sigabbrev_np(signum));
        installHandler(signum);
    }
    return intact;
}

} // namespace Syntalos
