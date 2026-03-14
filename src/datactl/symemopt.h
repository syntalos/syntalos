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

namespace Syntalos
{

/**
 * Change settings of the default Glibc allocator to reduce fragmentation
 * and match Syntalos' allocation patterns better.
 * Not needed if mimalloc is used.
 */
void configureGlibcAllocator();

/**
 * Install the mimalloc-backed allocator as OpenCV's
 * default matrix allocator.
 *
 * Call this once early in main(), before any cv::Mat
 * is created.
 */
void setCvMiMatAllocator();

} // namespace Syntalos
