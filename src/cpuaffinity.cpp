/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "cpuaffinity.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sched.h>

int get_online_cores_count()
{
    return (int) sysconf(_SC_NPROCESSORS_ONLN);
}

int thread_set_affinity(pthread_t thread, unsigned core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    return (rc == 0 ? 0 : -1);
}

int thread_set_affinity_from_vec(pthread_t thread, const std::vector<unsigned> &cores)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (unsigned core : cores)
        CPU_SET(core, &cpuset);

    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    return (rc == 0 ? 0 : -1);
}

int thread_clear_affinity(pthread_t thread)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < get_online_cores_count(); ++i)
        CPU_SET(i, &cpuset);

    int rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    return (rc == 0 ? 0 : -1);
}
