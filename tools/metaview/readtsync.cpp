/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "readtsync.h"

#include <memory>
#include <iostream>
#include "timesync.h"

using namespace Syntalos;

int displayTSyncMetadata(const QString &fname)
{
    auto tsr = std::make_unique<TimeSyncFileReader>();
    if (!tsr->open(fname)) {
        std::cerr << "Unable to open file '" << fname.toStdString() << "': " << tsr->lastError().toStdString() << std::endl;
        return 1;
    }

    std::cout << "File: " << "TimeSync" << "\n"
              << "Module: " << tsr->moduleName().toStdString() << "\n"
              << "CreationTimestampUnix: " << tsr->creationTime() << "\n"
              << "TimeResolution: " << "microseconds" << "\n"
              << "CheckInterval: " << tsr->checkInterval().count() << "\n"
              << "Tolerance: " << tsr->tolerance().count() << "\n"
              << std::endl;

    std::cout << "DeviceTime;Offset\n";
    for (const auto &pair : tsr->offsets()) {
        std::cout << pair.first.count() << ";" << pair.second.count() << "\n";
    }

    return 0;
}
