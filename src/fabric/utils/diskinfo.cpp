/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "diskinfo.h"

#include <filesystem>

#include "fabric/logging.h"

namespace fs = std::filesystem;

namespace Syntalos
{

qint64 directoryTotalSize(const QString &path)
{
    qint64 total = 0;
    std::error_code ec;
    for (auto it =
             fs::recursive_directory_iterator(path.toStdString(), fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec)
            break;
        if (it->is_regular_file(ec) && !it->is_symlink(ec))
            total += static_cast<qint64>(it->file_size(ec));
    }
    return total;
}

} // namespace Syntalos
