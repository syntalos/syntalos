/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef HRCLOCK_H
#define HRCLOCK_H

#include <chrono>

using steady_hr_clock =
    std::conditional<std::chrono::high_resolution_clock::is_steady,
                     std::chrono::high_resolution_clock,
                     std::chrono::steady_clock
                    >::type;
using steady_hr_timepoint = std::chrono::time_point<steady_hr_clock>;

using milliseconds_t = std::chrono::milliseconds;

inline std::chrono::milliseconds timeDiffMsec(const steady_hr_timepoint& timePoint1, const steady_hr_timepoint& timePoint2) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(timePoint1 - timePoint2);
}

inline std::chrono::milliseconds timeDiffToNowMsec(const std::chrono::time_point<steady_hr_clock>& timePoint) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(steady_hr_clock::now() - timePoint);
}

inline steady_hr_timepoint currentTimePoint() noexcept
{
    return steady_hr_clock::now();
}

class HRTimer
{
public:
    explicit HRTimer();

    void start();
    void startAt(steady_hr_timepoint startTimePoint);

    inline milliseconds_t timeSinceStartMsec()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(steady_hr_clock::now() - m_startTime);
    }

    inline std::chrono::nanoseconds timeSinceStartNsec()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(steady_hr_clock::now() - m_startTime);
    }

    inline steady_hr_timepoint currentTimerPoint()
    {
        return steady_hr_clock::now();
    }

    steady_hr_timepoint startTime()
    {
        return m_startTime;
    }

private:
    steady_hr_timepoint m_startTime;
};

/**
 * Compute a timestamp for "when this function acquired a value".
 * This is assumed to be the mean between function start and end time,
 * rounded to milliseconds.
 * For example, if the function F acquires a timestamp'ed value,
 * this macro should return the equivalent timestamp on our timer T.
 * This should balance out context switches if they are not too bad,
 * and produce a reasonably accurate result in milliseconds.
 * It is superior to measuring our timestamp for alignment after the other
 * timestamping function was run.
 */
#define TIMER_FUNC_TIMESTAMP(T, F) ({ \
    auto __stime = T->timeSinceStartNsec(); \
    F; \
    std::chrono::round<milliseconds_t>((__stime + T->timeSinceStartNsec()) / 2.0); \
    })
#define MTIMER_FUNC_TIMESTAMP(F) ({TIMER_TIMESTAMP_FUNC(m_timer, F)})

/**
 * Compute a timestamp for "when this function acquired a value".
 * This function is equivalent to TIMER_FUNC_TIMESTAMP(), but takes
 * a starting timepoint instead of a timer as first parameter, and
 * also captures the function result in FR.
 */
#define FUNC_EXEC_TIMESTAMP_RET(INIT_TIME, FR, F) ({ \
    auto __stime = std::chrono::duration_cast<std::chrono::nanoseconds>(steady_hr_clock::now() - (INIT_TIME)); \
    FR = F; \
    std::chrono::round<milliseconds_t>((__stime + std::chrono::duration_cast<std::chrono::nanoseconds>(steady_hr_clock::now() - (INIT_TIME))) / 2.0); \
    })

#endif // HRCLOCK_H
