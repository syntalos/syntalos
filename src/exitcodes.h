/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

// Exit codes of the syntalos executable, also relied upon by tools that launch it
constexpr int SY_EXIT_SUCCESS = 0;          /// success
constexpr int SY_EXIT_FAILURE = 1;          /// failure
constexpr int SY_EXIT_LOAD_ERROR = 2;       /// unable to load project/data
constexpr int SY_EXIT_PERMISSION_ERROR = 3; /// failed due to lack of permission
constexpr int SY_EXIT_NOT_FOUND = 4;        /// a resource or project or other data could not be found
constexpr int SY_EXIT_RUN_FAILED = 5;       /// failed to run the project
constexpr int SY_EXIT_TERMINATED = 6;       /// stopped cleanly after SIGINT/SIGTERM
constexpr int SY_EXIT_ALREADY_RUNNING = 7;  /// a previous instance was already running
