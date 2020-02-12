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

    inline std::chrono::milliseconds timeSinceStartMsec()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(steady_hr_clock::now() - m_startTime);
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

#endif // HRCLOCK_H
