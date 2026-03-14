/**
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

extern "C" {
#include <libavutil/cpu.h>
}

/**
 * Get the required buffer alignment for AVFrame.
 *
 * Thanks to Blender for figuring out the AVX512 bug, we are using
 * their code & comment here.
 */
static size_t ffmpeg_get_buffer_alignment()
{
    /* NOTE: even if av_frame_get_buffer suggests to pass 0 for alignment,
     * as of ffmpeg 6.1/7.0 it does not use correct alignment for AVX512
     * CPU (frame.c get_video_buffer ends up always using 32 alignment,
     * whereas it should have used 64). Reported upstream:
     * https://trac.ffmpeg.org/ticket/11116 and the fix on their code
     * side is to use 64 byte alignment as soon as AVX512 is compiled
     * in (even if CPU might not support it). So play safe and
     * use at least 64 byte alignment here too. Currently larger than
     * 64 alignment would not happen anywhere, but keep on querying
     * av_cpu_max_align just in case some future platform might. */
    size_t align = av_cpu_max_align();
    if (align < 64) {
        align = 64;
    }
    return align;
}
