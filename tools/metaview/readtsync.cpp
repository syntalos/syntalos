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
              << "CollectionID: " << tsr->collectionId().toString(QUuid::WithoutBraces).toStdString() << "\n"
              << "CreationTimestampUnix: " << tsr->creationTime() << "\n"
              << "Mode: " << tsyncFileModeToString(tsr->syncMode()).toStdString() << "\n"
              << "TimeDTypes: " << tsyncFileDataTypeToString(tsr->timeDTypes().first).toStdString() << "; "
                                << tsyncFileDataTypeToString(tsr->timeDTypes().second).toStdString() << "\n"
              << "TimeUnits: " << tsyncFileTimeUnitToString(tsr->timeUnits().first).toStdString() << "; "
                               << tsyncFileTimeUnitToString(tsr->timeUnits().second).toStdString() << "\n";
    if (tsr->tolerance().count() != 0)
        std::cout << "Tolerance: " << tsr->tolerance().count() << " µs\n";
    if (!tsr->userData().isEmpty()) {
        const auto userData =  tsr->userData();

        std::cout << "User Metadata:\n";
        for (const auto &key : userData.keys())
            std::cout << "    " << key.toStdString() << ": " << userData[key].toString().toStdString() << "\n";
    }
    std::cout << std::endl;

    auto timeNames = tsr->timeNames();
    if (timeNames.first.isEmpty())
        timeNames.first = QStringLiteral("time-a");
    if (timeNames.second.isEmpty())
        timeNames.second = QStringLiteral("time-b");

    std::cout << timeNames.first.toStdString() << ";" << timeNames.second.toStdString() << "\n";
    for (const auto &pair : tsr->times()) {
        std::cout << pair.first << ";" << pair.second << "\n";
    }

    return 0;
}
