/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

using milliseconds_t = std::chrono::milliseconds;

inline std::chrono::milliseconds timeDiffToNowMsec(const std::chrono::time_point<steady_hr_clock>& timePoint) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(steady_hr_clock::now() - timePoint);
}

inline std::chrono::time_point<steady_hr_clock> currentTimePoint() noexcept
{
    return steady_hr_clock::now();
}

class HRTimer
{
public:
    explicit HRTimer();

    void start();

    inline std::chrono::milliseconds timeSinceStartMsec()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(steady_hr_clock::now() - m_startTime);
    }

    inline std::chrono::time_point<steady_hr_clock> currentTimerPoint()
    {
        return steady_hr_clock::now();
    }

    std::chrono::time_point<steady_hr_clock> startTime()
    {
        return m_startTime;
    }

private:
    std::chrono::time_point<steady_hr_clock> m_startTime;
};

#endif // HRCLOCK_H
