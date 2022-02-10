//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.5
//
//  Copyright (c) 2020-2022 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <mutex>
#include <condition_variable>

#ifndef RHX_SEMAPHORE_H
#define RHX_SEMAPHORE_H

// Non-Qt version of QSemaphore
class Semaphore
{
public:
    Semaphore(int count_ = 0) :
        count(count_) {}

    inline void acquire(int n = 1)
    {
        std::unique_lock<std::mutex> lock(mtx);
        while (n > count) cv.wait(lock);
        count -= n;
    }

    inline bool tryAcquire(int n = 1)
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (n > count) {
            return false;
        } else {
            count -= n;
            return true;
        }
    }

    inline void release(int n = 1)
    {
        std::unique_lock<std::mutex> lock(mtx);
        count += n;
        cv.notify_all();
    }

    inline int available() const
    {
        std::unique_lock<std::mutex> lock(mtx);
        return count;
    }

private:
    mutable std::mutex mtx; // 'mutable' allows mutex to be used in const member functions (i.e., available()).
    std::condition_variable cv;
    int count;
};

#endif // RHX_SEMAPHORE_H
