/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <malloc.h>

namespace Syntalos
{

static void tuneGlibcMalloc()
{
    // Pin glibc's mmap threshold and disable its adaptive growth.
    //
    // By default, ptmalloc2 starts with M_MMAP_THRESHOLD = 128 KB and raises it
    // dynamically whenever a mmap'd block of that size is freed.
    // In some pipelines (e.g. when acquiring a ton of video data) large buffers
    // (~1-8 MB each, cv::Mat::clone) are freed at high rate, which quickly drives
    // the threshold above the frame size.
    // Once that happens, every subsequent buffer allocation lands in the brk() heap
    // instead of being mmap()'d.
    // The brk() heap never shrinks automatically (only malloc_trim() can force it),
    // so after even a short run with multiple modules (especially when IPC is involved
    // and we might copy a lot) the RSS can grow to several gigabytes that are never
    // returned to the OS between runs.
    //
    // Calling mallopt(M_MMAP_THRESHOLD, N) pins the threshold and disables the
    // adaptive algorithm.  With the threshold fixed at 512 KB, all larger allocations
    // continue to use mmap() and are automatically returned to the OS when they are destroyed.
    // M_TRIM_THRESHOLD is also tightened so the residual brk() heap shrinks promptly.
    mallopt(M_MMAP_THRESHOLD, 512 * 1024); // 512 KB – below any realistic frame size
    mallopt(M_TRIM_THRESHOLD, 128 * 1024); // trim the brk() top when ≥ 128 KB is free
}

}
