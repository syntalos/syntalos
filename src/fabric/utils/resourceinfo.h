/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QString>
#include <QElapsedTimer>

namespace Syntalos
{

typedef struct {
    long long memTotalKiB;
    long long memAvailableKiB;
    long long memAvailableMiB;
    double memAvailablePercent;
    long long swapTotalKiB;
    long long swapFreeKiB;
} MemInfo;

/**
 * @brief Memory pressure stall information as reported by the kernel (PSI).
 *
 * The values are the percentage of wall-clock time (averaged over the last
 * 10 / 60 seconds) in which at least one task ("some") or all non-idle tasks
 * ("full") were stalled waiting for memory, e.g. due to reclaim or swapping.
 * Sustained "some" values above a few percent mean the system is thrashing.
 */
typedef struct {
    bool available; /// false if the kernel does not provide PSI data
    double someAvg10;
    double someAvg60;
    double fullAvg10;
    double fullAvg60;
} MemPressure;

MemInfo readMemInfo();
MemPressure readMemPressure();

/**
 * @brief Information about the storage a path is located on.
 */
struct DiskSpaceInfo {
    bool valid = false;         /// the path exists on an accessible filesystem
    quint64 deviceId = 0;       /// identifies the filesystem, same for all paths on it
    QString mountPoint;         /// where the filesystem is mounted
    qint64 bytesAvailable = -1; /// bytes available to the current user
    qint64 bytesTotal = -1;
};

/**
 * @brief Query the free space of the filesystem that @p path is located on.
 *
 * If @p path does not exist, the closest existing parent directory is used,
 * so the check also works for files that are yet to be created.
 */
DiskSpaceInfo diskSpaceInfo(const QString &path);

/**
 * Determine the total size of all regular files below the given directory.
 */
qint64 directoryTotalSize(const QString &path);

/**
 * @brief Estimate how fast a finite resource is being used up.
 *
 * Periodic samples of the remaining amount of a resource (free disk space,
 * available memory, ...) are fed in, and the consumption rate is tracked as an
 * exponential moving average, so that the time until the resource is depleted
 * can be projected.
 *
 * Samples where the remaining amount grew count as a rate of zero, so the
 * estimate recovers quickly once consumption stops.
 */
class ResourceTrend
{
public:
    /**
     * @param minRelevantRate Consumption rates (units/sec) below this value are
     *        treated as "unknown", so no depletion time is projected from noise.
     * @param smoothing Weight of the newest sample in the moving average (0..1).
     */
    explicit ResourceTrend(double minRelevantRate = 0.0, double smoothing = 0.5);

    /**
     * @brief Forget all previous samples.
     */
    void reset();

    /**
     * @brief Add a sample of the remaining amount, timed against the previous sample.
     */
    void addSample(qint64 remaining);

    /**
     * @brief Add a sample of the remaining amount taken @p elapsedSec after the previous one.
     */
    void addSample(qint64 remaining, double elapsedSec);

    /**
     * @brief Whether enough samples were seen to give a meaningful rate.
     */
    bool hasRate() const;

    /**
     * @brief Smoothed consumption rate in units per second, negative if unknown.
     */
    double consumptionRate() const;

    /**
     * @brief Projected time in seconds until the resource is depleted at the current rate, negative if unknown.
     */
    double secondsUntilDepleted() const;

    /**
     * @brief The most recently sampled remaining amount, negative if no sample was seen.
     */
    qint64 lastRemaining() const;

private:
    double m_minRelevantRate;
    double m_smoothing;
    qint64 m_lastRemaining;
    double m_rate;
    QElapsedTimer m_timer;
};

} // namespace Syntalos
