/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "readtsync.h"

#include <iostream>
#include <memory>
#include "datactl/timesync.h"

using namespace Syntalos;

int displayTSyncMetadata(const QString &fname)
{
    auto tsr = std::make_unique<TimeSyncFileReader>();
    if (!tsr->open(fname.toStdString())) {
        std::cerr << "Unable to open file '" << fname.toStdString() << "': " << tsr->lastError() << std::endl;
        return 1;
    }

    std::cout << "File: "
              << "TimeSync"
              << "\n"
              << "Module: " << tsr->moduleName() << "\n"
              << "CollectionID: " << tsr->collectionId().toString(QUuid::WithoutBraces).toStdString() << "\n"
              << "CreationTimestampUnix: " << tsr->creationTime() << "\n"
              << "Mode: " << tsyncFileModeToString(tsr->syncMode()) << "\n"
              << "TimeDTypes: " << tsyncFileDataTypeToString(tsr->timeDTypes().first) << "; "
              << tsyncFileDataTypeToString(tsr->timeDTypes().second) << "\n"
              << "TimeUnits: " << tsyncFileTimeUnitToString(tsr->timeUnits().first) << "; "
              << tsyncFileTimeUnitToString(tsr->timeUnits().second) << "\n";
    if (tsr->tolerance().count() != 0)
        std::cout << "Tolerance: " << tsr->tolerance().count() << " µs\n";
    if (!tsr->userData().isEmpty()) {
        const auto userData = tsr->userData();

        std::cout << "User Metadata:\n";
        for (const auto &key : userData.keys())
            std::cout << "    " << key.toStdString() << ": " << userData[key].toString().toStdString() << "\n";
    }
    std::cout << std::endl;

    auto timeNames = tsr->timeNames();
    if (timeNames.first.empty())
        timeNames.first = "time-a";
    if (timeNames.second.empty())
        timeNames.second = "time-b";

    std::cout << timeNames.first << ";" << timeNames.second << "\n";
    for (const auto &pair : tsr->times()) {
        std::cout << pair.first << ";" << pair.second << "\n";
    }

    return 0;
}
