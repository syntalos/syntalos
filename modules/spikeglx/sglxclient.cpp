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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sglxclient.h"

#include <cerrno>
#include <clocale>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>

// The SDK headers pull `using namespace std;` into scope, so they are
// confined to this translation unit.
#include "SglxApi.h"
#include "SglxCppClient.h"

namespace Sglx
{

static_assert(std::is_same_v<int16_t, short>, "SDK sample type must match int16_t");

/**
 * Guards SDK handle creation/destruction process-wide: the SDK keeps
 * its socket table in a static map that is read by every command.
 */
static std::mutex g_sglxHandleMutex;

/**
 * The SDK parses server replies with std::stod(), which honors the process'
 * C locale. Under a locale with a different decimal separator (e.g. de_DE,
 * which Qt applications adopt) "2.34375e-06" would silently become 2.
 * Switch the calling thread to the "C" numeric locale while SDK code runs.
 */
class ScopedCNumericLocale
{
public:
    ScopedCNumericLocale()
        : m_locale(newlocale(LC_NUMERIC_MASK, "C", static_cast<locale_t>(0))),
          m_previous(static_cast<locale_t>(0))
    {
        if (m_locale != static_cast<locale_t>(0))
            m_previous = uselocale(m_locale);
    }

    ~ScopedCNumericLocale()
    {
        if (m_locale != static_cast<locale_t>(0)) {
            uselocale(m_previous);
            freelocale(m_locale);
        }
    }

    ScopedCNumericLocale(const ScopedCNumericLocale &) = delete;
    ScopedCNumericLocale &operator=(const ScopedCNumericLocale &) = delete;

private:
    locale_t m_locale;
    locale_t m_previous;
};

std::expected<void, std::string> probeTcpEndpoint(const std::string &host, int port, std::chrono::milliseconds timeout)
{
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    const auto portStr = std::to_string(port);
    const int gai = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
    if (gai != 0)
        return std::unexpected(std::string("Unable to resolve host: ") + gai_strerror(gai));

    std::string lastErr = "No usable address";
    for (auto *ai = res; ai != nullptr; ai = ai->ai_next) {
        const int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
        if (fd < 0) {
            lastErr = std::strerror(errno);
            continue;
        }

        int rc = ::connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc != 0 && errno != EINPROGRESS) {
            lastErr = std::strerror(errno);
            ::close(fd);
            continue;
        }

        if (rc != 0) {
            struct pollfd pfd{};
            pfd.fd = fd;
            pfd.events = POLLOUT;
            const int pr = poll(&pfd, 1, static_cast<int>(timeout.count()));
            if (pr == 0) {
                lastErr = "Connection timed out";
                ::close(fd);
                continue;
            }
            if (pr < 0) {
                lastErr = std::strerror(errno);
                ::close(fd);
                continue;
            }
            int soErr = 0;
            socklen_t len = sizeof(soErr);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soErr, &len) != 0 || soErr != 0) {
                lastErr = std::strerror(soErr != 0 ? soErr : errno);
                ::close(fd);
                continue;
            }
        }

        ::close(fd);
        freeaddrinfo(res);
        return {};
    }

    freeaddrinfo(res);
    return std::unexpected(lastErr);
}

Client::Client()
    : m_handle(nullptr),
      m_port(0),
      m_connected(false)
{
}

Client::~Client()
{
    close();
}

std::string Client::lastError(const char *context) const
{
    std::string err;
    if (m_handle != nullptr)
        err = sglx_getError(m_handle);
    if (err.empty())
        err = "Unknown error";
    if (context == nullptr)
        return err;
    return std::string(context) + ": " + err;
}

template<typename F>
Client::Result<void> Client::command(const char *context, F &&f)
{
    std::lock_guard lock(m_mutex);
    if (!m_connected)
        return std::unexpected(std::string(context) + ": Not connected");
    ScopedCNumericLocale cLocale;
    if (!f())
        return std::unexpected(lastError(context));
    return {};
}

Client::Result<void> Client::connect(const std::string &host, int port, std::chrono::milliseconds probeTimeout)
{
    // the SDK's connect blocks without timeout, so probe first
    if (auto probe = probeTcpEndpoint(host, port, probeTimeout); !probe)
        return std::unexpected("Unable to reach " + host + ":" + std::to_string(port) + ": " + probe.error());

    std::lock_guard lock(m_mutex);
    std::lock_guard handleLock(g_sglxHandleMutex);

    // sglx_connect() would leak the previous socket, so shut down explicitly first
    if (m_handle != nullptr) {
        sglx_close(m_handle);
        sglx_destroyHandle(m_handle);
        m_handle = nullptr;
    }
    m_connected = false;

    ScopedCNumericLocale cLocale;
    m_handle = sglx_createHandle_std();
    if (m_handle == nullptr)
        return std::unexpected("Unable to create SpikeGLX client handle");

    if (!sglx_connect(m_handle, host.c_str(), port)) {
        const auto err = lastError("Connect");
        sglx_close(m_handle);
        sglx_destroyHandle(m_handle);
        m_handle = nullptr;
        return std::unexpected(err);
    }

    m_host = host;
    m_port = port;
    m_connected = true;
    return {};
}

void Client::close()
{
    std::lock_guard lock(m_mutex);
    if (m_handle == nullptr)
        return;
    std::lock_guard handleLock(g_sglxHandleMutex);
    sglx_close(m_handle);
    sglx_destroyHandle(m_handle);
    m_handle = nullptr;
    m_connected = false;
}

bool Client::isConnected() const
{
    std::lock_guard lock(m_mutex);
    return m_connected;
}

std::string Client::host() const
{
    std::lock_guard lock(m_mutex);
    return m_host;
}

int Client::port() const
{
    std::lock_guard lock(m_mutex);
    return m_port;
}

std::string Client::version() const
{
    std::lock_guard lock(m_mutex);
    if (m_handle == nullptr)
        return {};
    const char *v = sglx_getVersion(m_handle);
    return v ? std::string(v) : std::string();
}

Client::Result<bool> Client::isInitialized()
{
    bool ready = false;
    if (auto r = command(
            "ISINITIALIZED",
            [&] {
                return sglx_isInitialized(ready, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return ready;
}

Client::Result<bool> Client::isRunning()
{
    bool running = false;
    if (auto r = command(
            "ISRUNNING",
            [&] {
                return sglx_isRunning(running, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return running;
}

Client::Result<bool> Client::isSaving()
{
    bool saving = false;
    if (auto r = command(
            "ISSAVING",
            [&] {
                return sglx_isSaving(saving, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return saving;
}

Client::Result<double> Client::serverTime()
{
    std::lock_guard lock(m_mutex);
    if (!m_connected)
        return std::unexpected(std::string("GETTIME: Not connected"));
    ScopedCNumericLocale cLocale;
    const double t = sglx_getTime(m_handle);
    if (t <= 0.0)
        return std::unexpected(lastError("GETTIME"));
    return t;
}

Client::Result<void> Client::setRunName(const std::string &name)
{
    return command("SETRUNNAME", [&] {
        return sglx_setRunName(m_handle, name);
    });
}

Client::Result<void> Client::startRun(const std::string &name)
{
    return command("STARTRUN", [&] {
        return sglx_startRun(m_handle, name);
    });
}

Client::Result<void> Client::stopRun()
{
    return command("STOPRUN", [&] {
        return sglx_stopRun(m_handle);
    });
}

Client::Result<void> Client::setRecordingEnable(bool enable)
{
    return command("SETRECORDENAB", [&] {
        return sglx_setRecordingEnable(m_handle, enable);
    });
}

Client::Result<void> Client::triggerGT(int g, int t)
{
    return command("TRIGGERGT", [&] {
        return sglx_triggerGT(m_handle, g, t);
    });
}

Client::Result<void> Client::setMetadata(const std::map<std::string, std::string> &kv)
{
    cClient_sglx_set_keyvals kvIo(kv);
    return command("SETMETADATA", [&] {
        return sglx_setMetadata(m_handle, kvIo);
    });
}

Client::Result<void> Client::selectDevices(const std::string &devString, int errLevel)
{
    return command("SELECTDEVS", [&] {
        return sglx_selectDevices(m_handle, devString, errLevel);
    });
}

Client::Result<std::string> Client::probeAddrs()
{
    std::string list;
    if (auto r = command(
            "GETPROBEADDRS",
            [&] {
                return sglx_getProbeAddrs(list, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return list;
}

Client::Result<std::string> Client::runName()
{
    std::string name;
    if (auto r = command(
            "GETRUNNAME",
            [&] {
                return sglx_getRunName(name, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return name;
}

Client::Result<std::string> Client::dataDir(int idir)
{
    std::string dir;
    if (auto r = command(
            "GETDATADIR",
            [&] {
                return sglx_getDataDir(dir, m_handle, idir);
            });
        !r)
        return std::unexpected(r.error());
    return dir;
}

Client::Result<std::pair<int, int>> Client::lastGT()
{
    int g = -1;
    int t = -1;
    if (auto r = command(
            "GETLASTGT",
            [&] {
                return sglx_getLastGT(g, t, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return std::make_pair(g, t);
}

Client::Result<std::vector<std::string>> Client::enumDataDir(int idir)
{
    std::vector<std::string> files;
    cClient_sglx_get_strs io(files);
    if (auto r = command(
            "ENUMDATADIR",
            [&] {
                return sglx_enumDataDir(io, m_handle, idir);
            });
        !r)
        return std::unexpected(r.error());
    return files;
}

Client::Result<std::map<std::string, std::string>> Client::params()
{
    cppClient_sglx_get_keyvals io;
    if (auto r = command(
            "GETPARAMS",
            [&] {
                return sglx_getParams(io, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return io.mstrstr;
}

Client::Result<std::string> Client::probeList()
{
    std::string list;
    if (auto r = command(
            "GETPROBELIST",
            [&] {
                return sglx_getProbeList(list, m_handle);
            });
        !r)
        return std::unexpected(r.error());
    return list;
}

Client::Result<int> Client::streamCount(int js)
{
    int np = 0;
    if (auto r = command(
            "GETSTREAMNP",
            [&] {
                return sglx_getStreamNP(np, m_handle, js);
            });
        !r)
        return std::unexpected(r.error());
    return np;
}

Client::Result<double> Client::sampleRate(StreamId sid)
{
    std::lock_guard lock(m_mutex);
    if (!m_connected)
        return std::unexpected(std::string("GETSTREAMSAMPLERATE: Not connected"));
    ScopedCNumericLocale cLocale;
    const double rate = sglx_getStreamSampleRate(m_handle, sid.js, sid.ip);
    if (rate <= 0.0)
        return std::unexpected(lastError("GETSTREAMSAMPLERATE"));
    return rate;
}

Client::Result<std::vector<int>> Client::acqChanCounts(StreamId sid)
{
    std::vector<int> counts;
    cClient_sglx_get_ints io(counts);
    if (auto r = command(
            "GETSTREAMACQCHANS",
            [&] {
                return sglx_getStreamAcqChans(io, m_handle, sid.js, sid.ip);
            });
        !r)
        return std::unexpected(r.error());
    return counts;
}

Client::Result<std::vector<int>> Client::saveChans(StreamId sid)
{
    std::vector<int> chans;
    cClient_sglx_get_ints io(chans);
    if (auto r = command(
            "GETSTREAMSAVECHANS",
            [&] {
                return sglx_getStreamSaveChans(io, m_handle, sid.js, sid.ip);
            });
        !r)
        return std::unexpected(r.error());
    return chans;
}

Client::Result<StreamSN> Client::streamSN(StreamId sid)
{
    StreamSN sn;
    if (auto r = command(
            "GETSTREAMSN",
            [&] {
                return sglx_getStreamSN(sn.slotOrType, sn.serial, m_handle, sid.js, sid.ip);
            });
        !r)
        return std::unexpected(r.error());
    return sn;
}

Client::Result<double> Client::i16ToVolts(StreamId sid, int chan)
{
    double mult = 0.0;
    if (auto r = command(
            "GETSTREAMI16TOVOLTS",
            [&] {
                return sglx_getStreamI16ToVolts(mult, m_handle, sid.js, sid.ip, chan);
            });
        !r)
        return std::unexpected(r.error());
    return mult;
}

Client::Result<int> Client::maxInt(StreamId sid)
{
    int maxInt = 0;
    if (auto r = command(
            "GETSTREAMMAXINT",
            [&] {
                return sglx_getStreamMaxInt(maxInt, m_handle, sid.js, sid.ip);
            });
        !r)
        return std::unexpected(r.error());
    return maxInt;
}

Client::Result<uint64_t> Client::sampleCount(StreamId sid)
{
    std::lock_guard lock(m_mutex);
    if (!m_connected)
        return std::unexpected(std::string("GETSTREAMSAMPLECOUNT: Not connected"));
    ScopedCNumericLocale cLocale;
    const t_ull count = sglx_getStreamSampleCount(m_handle, sid.js, sid.ip);
    if (count == 0)
        return std::unexpected(lastError("GETSTREAMSAMPLECOUNT"));
    return static_cast<uint64_t>(count);
}

Client::Result<FetchResult> Client::fetch(
    StreamId sid,
    uint64_t startSamp,
    int maxSamps,
    const std::vector<int> &chans,
    std::vector<int16_t> &buffer)
{
    // the SDK formats the FETCH command into a fixed 32-byte buffer
    if (maxSamps < 1 || maxSamps > 999999)
        return std::unexpected(std::string("FETCH: max_samps must be within [1, 999999]"));
    if (startSamp == 0)
        return std::unexpected(std::string("FETCH: start sample must be > 0"));

    cClient_sglx_fetch io(buffer);
    io.js = static_cast<short>(sid.js);
    io.ip = static_cast<short>(sid.ip);
    io.channel_subset = chans.empty() ? nullptr : chans.data();
    io.n_cs = static_cast<int>(chans.size());
    io.max_samps = maxSamps;
    io.downsample = 1;

    std::lock_guard lock(m_mutex);
    if (!m_connected)
        return std::unexpected(std::string("FETCH: Not connected"));
    ScopedCNumericLocale cLocale;

    const t_ull headCt = sglx_fetch(io, m_handle, startSamp);
    if (headCt == 0)
        return std::unexpected(lastError("FETCH"));

    FetchResult res;
    res.headCt = static_cast<uint64_t>(headCt);
    if (!chans.empty()) {
        res.nChans = static_cast<int>(chans.size());
    } else {
        // all acquired channels were requested; derive the width from the layout
        std::vector<int> counts;
        cClient_sglx_get_ints cio(counts);
        if (!sglx_getStreamAcqChans(cio, m_handle, sid.js, sid.ip))
            return std::unexpected(lastError("FETCH"));
        for (const auto c : counts)
            res.nChans += c;
    }
    if (res.nChans <= 0)
        return std::unexpected(std::string("FETCH: Stream has no channels"));
    res.nSamps = static_cast<int>(buffer.size()) / res.nChans;

    return res;
}

} // namespace Sglx
