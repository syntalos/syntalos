/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2014 Jakob Unterwurzacher
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

/*
 * readMemInfo() is also licensed under the MIT License:
 * Copyright (C) 2014 Jakob Unterwurzacher
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "resourceinfo.h"

#include <filesystem>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>

#include "fabric/logging.h"

namespace fs = std::filesystem;

namespace Syntalos
{

/* Parse the contents of /proc/meminfo (in buf), return value of "name"
 * (example: MemTotal)
 * Returns -errno if the entry cannot be found. */
static long long get_entry(const char *name, const char *buf)
{
    const char *hit = strstr(buf, name);
    if (hit == NULL) {
        return -ENODATA;
    }

    errno = 0;
    const long long val = strtoll(hit + strlen(name), NULL, 10);
    if (errno != 0) {
        int strtoll_errno = errno;
        perror("get_entry: strtol() failed");
        return -strtoll_errno;
    }
    return val;
}

/* Like get_entry(), but exit if the value cannot be found */
static long long get_entry_fatal(const char *name, const char *buf)
{
    const long long val = get_entry(name, buf);
    if (val < 0) {
        auto log = Syntalos::getLogger("meminfo");
        LOG_CRITICAL(log, "could not find entry '{}' in /proc/meminfo: {}", name, strerror((int)-val));
        log->flush_log();
        std::abort();
    }
    return val;
}

/* If the kernel does not provide MemAvailable (introduced in Linux 3.14),
 * approximate it using other data we can get */
static long long available_guesstimate(const char *buf)
{
    long long Cached = get_entry_fatal("Cached:", buf);
    long long MemFree = get_entry_fatal("MemFree:", buf);
    long long Buffers = get_entry_fatal("Buffers:", buf);
    long long Shmem = get_entry_fatal("Shmem:", buf);

    return MemFree + Cached + Buffers - Shmem;
}

/**
 * Read some data from meminfo that is relevant to Syntalos.
 */
MemInfo readMemInfo()
{
    auto log = Syntalos::getLogger("meminfo");

    // Note that we do not need to close static FDs that we ensure to
    // `fopen()` maximally once.
    static FILE *fd;

    // On Linux 5.3, "wc -c /proc/meminfo" counts 1391 bytes.
    // 8192 should be enough for the foreseeable future.
    char buf[8192] = {0};
    MemInfo m = {0, 0, 0, 0, 0, 0};

    if (fd == NULL)
        fd = fopen("/proc/meminfo", "r");
    if (fd == NULL) {
        LOG_CRITICAL(log, "could not open /proc/meminfo: {}", strerror(errno));
        return m;
    }
    rewind(fd);

    size_t len = fread(buf, 1, sizeof(buf) - 1, fd);
    if (ferror(fd)) {
        LOG_CRITICAL(log, "could not read /proc/meminfo: {}", strerror(errno));
        return m;
    }
    if (len == 0) {
        LOG_CRITICAL(log, "could not read /proc/meminfo: 0 bytes returned");
        return m;
    }

    m.memTotalKiB = get_entry_fatal("MemTotal:", buf);
    long long MemAvailable = get_entry("MemAvailable:", buf);
    if (MemAvailable < 0) {
        // kernel was too old
        MemAvailable = available_guesstimate(buf);
    }

    // Calculate percentages
    m.memAvailableKiB = MemAvailable;
    m.memAvailablePercent = (double)MemAvailable * 100 / (double)m.memTotalKiB;

    // Convert kiB to MiB
    m.memAvailableMiB = MemAvailable / 1024;

    // swap is optional, the entries are 0 if none is configured
    m.swapTotalKiB = std::max(get_entry("SwapTotal:", buf), 0LL);
    m.swapFreeKiB = std::max(get_entry("SwapFree:", buf), 0LL);

    return m;
}

/**
 * Read memory pressure stall information from the kernel.
 * This is unavailable on kernels without PSI support (or with psi=0),
 * in which case the returned structure is flagged as unavailable.
 */
MemPressure readMemPressure()
{
    MemPressure p = {false, 0, 0, 0, 0};

    // Do not keep the file open: unlike /proc/meminfo, a PSI file that was opened
    // with psi=0 keeps failing, so we just retry cheaply each time.
    FILE *f = fopen("/proc/pressure/memory", "r");
    if (f == NULL)
        return p;

    char line[256];
    bool haveSome = false;
    while (fgets(line, sizeof(line), f) != NULL) {
        double avg10 = 0, avg60 = 0, avg300 = 0;
        if (sscanf(line, "some avg10=%lf avg60=%lf avg300=%lf", &avg10, &avg60, &avg300) == 3) {
            p.someAvg10 = avg10;
            p.someAvg60 = avg60;
            haveSome = true;
        } else if (sscanf(line, "full avg10=%lf avg60=%lf avg300=%lf", &avg10, &avg60, &avg300) == 3) {
            p.fullAvg10 = avg10;
            p.fullAvg60 = avg60;
        }
    }
    fclose(f);

    p.available = haveSome;
    return p;
}

DiskSpaceInfo diskSpaceInfo(const QString &path)
{
    DiskSpaceInfo info;
    std::error_code ec;

    // find the closest existing directory, so we can also check locations that will be created
    fs::path p = fs::absolute(path.toStdString(), ec);
    if (ec)
        return info;
    while (!p.empty() && !fs::exists(p, ec)) {
        const auto parent = p.parent_path();
        if (parent == p)
            break;
        p = parent;
    }

    struct stat st;
    if (::stat(p.c_str(), &st) != 0)
        return info;
    const auto space = fs::space(p, ec);
    if (ec)
        return info;

    info.valid = true;
    info.deviceId = st.st_dev;
    info.bytesAvailable = static_cast<qint64>(space.available);
    info.bytesTotal = static_cast<qint64>(space.capacity);

    // walk up until we cross a filesystem boundary to find the mount point
    auto mount = fs::canonical(p, ec);
    if (ec)
        mount = p;
    while (mount.has_parent_path() && mount.parent_path() != mount) {
        struct stat pst;
        if (::stat(mount.parent_path().c_str(), &pst) != 0 || pst.st_dev != st.st_dev)
            break;
        mount = mount.parent_path();
    }
    info.mountPoint = QString::fromStdString(mount.string());

    return info;
}

qint64 directoryTotalSize(const QString &path)
{
    qint64 total = 0;
    std::error_code ec;
    for (auto it =
             fs::recursive_directory_iterator(path.toStdString(), fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec)
            break;
        if (it->is_regular_file(ec) && !it->is_symlink(ec))
            total += static_cast<qint64>(it->file_size(ec));
    }
    return total;
}

ResourceTrend::ResourceTrend(double minRelevantRate, double smoothing)
    : m_minRelevantRate(std::max(minRelevantRate, 0.0)),
      m_smoothing(std::clamp(smoothing, 0.0, 1.0)),
      m_lastRemaining(-1),
      m_rate(-1.0)
{
}

void ResourceTrend::reset()
{
    m_lastRemaining = -1;
    m_rate = -1.0;
    m_timer.invalidate();
}

void ResourceTrend::addSample(qint64 remaining)
{
    const double elapsedSec = m_timer.isValid() ? m_timer.nsecsElapsed() / 1.0e9 : 0.0;
    m_timer.start();
    addSample(remaining, elapsedSec);
}

void ResourceTrend::addSample(qint64 remaining, double elapsedSec)
{
    // ignore samples taken (almost) at the same time as the previous one, they only add noise
    constexpr double minSampleIntervalSec = 0.1;

    if (m_lastRemaining >= 0 && elapsedSec >= minSampleIntervalSec) {
        const double rate = std::max(0.0, static_cast<double>(m_lastRemaining - remaining) / elapsedSec);
        m_rate = (m_rate < 0.0) ? rate : (m_smoothing * rate + (1.0 - m_smoothing) * m_rate);
    }
    if (m_lastRemaining < 0 || elapsedSec >= minSampleIntervalSec)
        m_lastRemaining = remaining;
}

bool ResourceTrend::hasRate() const
{
    return m_rate >= 0.0 && m_rate >= m_minRelevantRate && m_rate > 0.0;
}

double ResourceTrend::consumptionRate() const
{
    return m_rate;
}

double ResourceTrend::secondsUntilDepleted() const
{
    if (!hasRate() || m_lastRemaining < 0)
        return -1.0;
    return static_cast<double>(m_lastRemaining) / m_rate;
}

qint64 ResourceTrend::lastRemaining() const
{
    return m_lastRemaining;
}

} // namespace Syntalos
