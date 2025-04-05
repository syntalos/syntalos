/*
 * Copyright (C) 2014 Jakob Unterwurzacher
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: MIT or LGPL-3.0-or-later
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

typedef struct {
    long long memTotalKiB;
    long long memAvailableMiB;
    double memAvailablePercent;
} MemInfo;

MemInfo readMemInfo();
