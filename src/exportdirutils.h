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

#include <optional>
#include <QStringList>

namespace Syntalos
{

/**
 * Components that can appear in the export directory path.
 */
enum class ExportPathComponent {
    SubjectId = 0,
    Time = 1,
    ExperimentId = 2,
};

const QList<ExportPathComponent> DefaultExportPathComponentOrder = {
    ExportPathComponent::SubjectId,
    ExportPathComponent::Time,
    ExportPathComponent::ExperimentId,
};

QString exportDirPathComponentToKey(ExportPathComponent component);
std::optional<ExportPathComponent> exportDirPathComponentFromKey(const QString &key);
QString exportDirPathComponentTitle(ExportPathComponent component);

QList<ExportPathComponent> normalizeExportDirLayout(const QList<ExportPathComponent> &layout);

} // namespace Syntalos
