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

#include "symemopt.h"

#include <malloc.h>
#include <mimalloc.h>
#include <opencv2/core/core_c.h>
#include <opencv2/core/mat.hpp>

using namespace Syntalos;

void Syntalos::configureGlibcAllocator()
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
    mallopt(M_MMAP_THRESHOLD, 512 * 1024); // 512 KB - below any realistic frame size
    mallopt(M_TRIM_THRESHOLD, 128 * 1024); // trim the brk() top when >= 128 KB is free
}

/**
 * @brief OpenCV Mat allocator backed by mimalloc.
 *
 * Structurally identical to OpenCV's internal StdAllocator, with two
 * targeted improvements:
 *
 *  1. **Aligned allocation** – `mi_malloc_aligned(..., kAlignment)` gives every
 *     buffer 64-byte alignment, matching the CPU cache-line size and the
 *     AVX-512 vector width so OpenCV's IPP/SIMD paths never fall back to a
 *     scalar tail.
 *
 *  2. **Aligned row strides** – when the allocator owns the buffer
 *     (`data0 == nullptr`) each intermediate stride is padded to `kAlignment`,
 *     so every scanline (and each inner hyperplane of an N-D array) begins at
 *     an aligned address, letting vectorised kernels skip per-row masking.
 *
 * mimalloc's thread-local arenas also dramatically cut lock contention in
 * Syntalos' heavily-threaded, burst-allocation video pipelines compared with
 * the system allocator.
 */
class MiMatAllocator CV_FINAL : public cv::MatAllocator
{
public:
    /// Alignment in bytes: matches the cache-line size and AVX-512 requirements.
    /// This works well on amd64 and arm64, where Syntalos is most likely to run
    /// (amd64 uses 64-byte cache lines, most arm64 CPUs do to, Apple seems to use
    /// 128 byte, but even there 64 will work well, and we are unlikely to run on
    /// Apple silicon without a Linux port).
    static constexpr size_t kAlignment = 64;

    cv::UMatData *allocate(
        int dims,
        const int *sizes,
        int type,
        void *data0,
        size_t *step,
        cv::AccessFlag /*flags*/,
        cv::UMatUsageFlags /*usageFlags*/) const CV_OVERRIDE
    {
        size_t total = CV_ELEM_SIZE(type);
        for (int i = dims - 1; i >= 0; i--) {
            if (step) {
                if (data0 && step[i] != CV_AUTOSTEP) {
                    CV_Assert(total <= step[i]);
                    total = step[i];
                } else {
                    // Pad intermediate strides to kAlignment so every scanline
                    // starts at an aligned address when we own the buffer.
                    if (!data0 && i < dims - 1)
                        total = (total + kAlignment - 1) & ~(kAlignment - 1);
                    step[i] = total;
                }
            }
            total *= sizes[i];
        }
        uchar *data = data0 ? (uchar *)data0 : (uchar *)mi_malloc_aligned(total, kAlignment);
        auto *u = new cv::UMatData(this);
        u->data = u->origdata = data;
        u->size = total;
        if (data0)
            u->flags |= cv::UMatData::USER_ALLOCATED;

        return u;
    }

    bool allocate(cv::UMatData *u, cv::AccessFlag /*accessFlags*/, cv::UMatUsageFlags /*usageFlags*/) const CV_OVERRIDE
    {
        if (!u)
            return false;
        return true;
    }

    void deallocate(cv::UMatData *u) const CV_OVERRIDE
    {
        if (!u)
            return;

        CV_Assert(u->urefcount == 0);
        CV_Assert(u->refcount == 0);
        if (!(u->flags & cv::UMatData::USER_ALLOCATED)) {
            // Provide size and alignment hints so mimalloc can skip the
            // size-class lookup and return the segment to its arena faster.
            mi_free_size_aligned(u->origdata, u->size, kAlignment);
            u->origdata = nullptr;
        }
        delete u;
    }
};

/// Global instance of the mimalloc-backed OpenCV Mat allocator
static MiMatAllocator g_mi_mat_allocator;

void Syntalos::setCvMiMatAllocator()
{
    cv::Mat::setDefaultAllocator(&g_mi_mat_allocator);
}
