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

#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Sglx
{

/// Stream-type selectors, as used by the SpikeGLX remote API ("js").
constexpr int JS_NI = 0; /// NI-DAQ stream
constexpr int JS_OB = 1; /// OneBox stream
constexpr int JS_IM = 2; /// IMEC probe stream

/**
 * @brief Identifies one SpikeGLX data stream: (stream type, stream index).
 */
struct StreamId {
    int js = JS_IM;
    int ip = 0;

    bool operator==(const StreamId &) const = default;
};

struct FetchResult {
    uint64_t headCt = 0; /// index of the first sample returned
    int nChans = 0;      /// channels per sample
    int nSamps = 0;      /// samples returned (0 = no new data yet)
};

struct StreamSN {
    std::string serial;
    int slotOrType = 0; /// OneBox: slot, IMEC: probe type
};

/**
 * @brief Thin RAII wrapper around one SpikeGLX-CPP-SDK connection handle.
 *
 * All calls are serialized by an internal mutex, so a single instance
 * may be shared between threads (the SDK itself is not thread-safe).
 * Handle creation and destruction are additionally guarded by a
 * process-wide lock, because the SDK keeps its socket table in a
 * static map that is also read by every command on every other handle.
 *
 * SDK headers are deliberately kept out of this header.
 */
class Client
{
public:
    template<typename T>
    using Result = std::expected<T, std::string>;

    explicit Client();
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    /**
     * @brief Connect to a SpikeGLX command server.
     *
     * The SDK's own connect has no timeout, so we first probe the endpoint with a
     * bounded non-blocking connect and only hand over to the SDK on success.
     */
    Result<void> connect(const std::string &host, int port, std::chrono::milliseconds probeTimeout);
    void close();
    bool isConnected() const;

    std::string host() const;
    int port() const;
    /// SpikeGLX version string as reported on connect.
    std::string version() const;

    // Application state
    Result<bool> isInitialized();
    Result<bool> isRunning();
    Result<bool> isSaving();
    Result<double> serverTime();

    // Run control
    Result<void> setRunName(const std::string &name);
    Result<void> startRun(const std::string &name = {});
    Result<void> stopRun();
    Result<void> setRecordingEnable(bool enable);
    Result<void> triggerGT(int g, int t);
    Result<void> setMetadata(const std::map<std::string, std::string> &kv);

    /**
     * @brief Remotely perform "Detect" and "Verify | Save" in SpikeGLX's configuration dialog.
     *
     * @param devString Devices to enable, e.g. "(40,1,1)" for probe slot 40, port 1, dock 1,
     *                  "(21,obx)" for a OneBox in slot 21, "(nidq)" for NI-DAQ.
     * @param errLevel 1 = report severe errors, 2 = also report warnings.
     */
    Result<void> selectDevices(const std::string &devString, int errLevel = 1);

    // Run information
    Result<std::string> runName();
    Result<std::string> dataDir(int idir = 0);
    Result<std::pair<int, int>> lastGT();
    Result<std::vector<std::string>> enumDataDir(int idir = 0);
    Result<std::map<std::string, std::string>> params();
    /// Raw "(ip,nShanks,partNumber)…" probe list string.
    Result<std::string> probeList();
    /// Raw "(ip,slot,port,dock)…" probe address string.
    Result<std::string> probeAddrs();

    // Stream layout
    Result<int> streamCount(int js);
    Result<double> sampleRate(StreamId sid);
    /// Channel counts per type: NI {MN,MA,XA,DW}, OB {XA,DW,SY}, IM {AP,LF,SY}.
    Result<std::vector<int>> acqChanCounts(StreamId sid);
    Result<std::vector<int>> saveChans(StreamId sid);
    Result<StreamSN> streamSN(StreamId sid);
    Result<double> i16ToVolts(StreamId sid, int chan);
    Result<int> maxInt(StreamId sid);

    // Live data
    /// Total samples acquired so far on this stream (0 if not running).
    Result<uint64_t> sampleCount(StreamId sid);

    /**
     * @brief Fetch samples [startSamp, startSamp + maxSamps) from a stream.
     *
     * @param chans Absolute channel indices to fetch, empty = all acquired channels.
     * @param buffer Receives nChans*nSamps int16 values, sample-major.
     *
     * A cursor that has fallen out of the server's ring buffer yields an error
     * containing "Too late".
     */
    Result<FetchResult> fetch(
        StreamId sid,
        uint64_t startSamp,
        int maxSamps,
        const std::vector<int> &chans,
        std::vector<int16_t> &buffer);

private:
    std::string lastError(const char *context) const;
    template<typename F>
    Result<void> command(const char *context, F &&f);

    mutable std::mutex m_mutex;
    void *m_handle;
    std::string m_host;
    int m_port;
    bool m_connected;
};

/**
 * @brief Test whether a TCP endpoint accepts connections, within a time limit.
 */
std::expected<void, std::string> probeTcpEndpoint(const std::string &host, int port, std::chrono::milliseconds timeout);

} // namespace Sglx
