/*
    ------------------------------------------------------------------
    Copyright (C) 2024 Open Ephys
    Copyright (C) 2026 Syntalos Project

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

/**
 * Result of an impedance scan: parallel arrays indexed by sample number.
 */
struct Impedances {
    std::vector<int> streams;
    std::vector<int> channels;
    std::vector<float> magnitudes; // Ohms
    std::vector<float> phases;     // degrees
    bool valid = false;
};

/**
 * Abstract impedance meter. The actual scan runs synchronously on the calling
 * thread (typically the GUI thread, with a modal progress dialog). Long-running
 * implementations should poll @c shouldStop() periodically and post updates via
 * the optional progress callbacks.
 */
class ImpedanceMeter
{
public:
    using ProgressCallback = std::function<void(double progress, const std::string &status)>;

    ImpedanceMeter() = default;
    virtual ~ImpedanceMeter() = default;

    ImpedanceMeter(const ImpedanceMeter &) = delete;
    ImpedanceMeter &operator=(const ImpedanceMeter &) = delete;

    /** Calculate impedance values for all channels (synchronous). */
    virtual void runImpedanceMeasurement(Impedances &impedances) = 0;

    /** Request an in-progress measurement to abort at the next convenient point. */
    void requestStop() noexcept
    {
        m_stopRequested.store(true, std::memory_order_relaxed);
    }

    void resetStopRequest() noexcept
    {
        m_stopRequested.store(false, std::memory_order_relaxed);
    }

    void setProgressCallback(ProgressCallback cb)
    {
        m_progressCb = std::move(cb);
    }

protected:
    [[nodiscard]] bool shouldStop() const noexcept
    {
        return m_stopRequested.load(std::memory_order_relaxed);
    }

    void reportProgress(double fraction, const std::string &status = {}) const
    {
        if (m_progressCb)
            m_progressCb(fraction, status);
    }

    /** Allocate a 3-D array of doubles (helper). */
    static void allocateDoubleArray3D(
        std::vector<std::vector<std::vector<double>>> &array3D,
        int xSize,
        int ySize,
        int zSize)
    {
        if (xSize == 0)
            return;
        array3D.resize(xSize);
        for (int i = 0; i < xSize; ++i) {
            array3D[i].resize(ySize);
            for (int j = 0; j < ySize; ++j)
                array3D[i][j].resize(zSize);
        }
    }

private:
    std::atomic<bool> m_stopRequested{false};
    ProgressCallback m_progressCb;
};
