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

#include "exportdirutils.h"

namespace Syntalos
{

QString exportDirPathComponentToKey(ExportPathComponent component)
{
    if (component == ExportPathComponent::SubjectId)
        return QStringLiteral("subject-id");
    if (component == ExportPathComponent::Time)
        return QStringLiteral("time");
    return QStringLiteral("experiment-id");
}

std::optional<ExportPathComponent> exportDirPathComponentFromKey(const QString &key)
{
    if (key == QStringLiteral("subject-id"))
        return ExportPathComponent::SubjectId;
    if (key == QStringLiteral("time"))
        return ExportPathComponent::Time;
    if (key == QStringLiteral("experiment-id"))
        return ExportPathComponent::ExperimentId;
    return std::nullopt;
}

QString exportDirPathComponentTitle(ExportPathComponent component)
{
    if (component == ExportPathComponent::SubjectId)
        return QStringLiteral("Subject-ID");
    if (component == ExportPathComponent::Time)
        return QStringLiteral("Time");
    return QStringLiteral("Experiment-ID");
}

QList<ExportPathComponent> normalizeExportDirLayout(const QList<ExportPathComponent> &layout)
{
    const QList<ExportPathComponent> allParts = {
        ExportPathComponent::SubjectId,
        ExportPathComponent::Time,
        ExportPathComponent::ExperimentId,
    };

    QList<ExportPathComponent> normalized;
    for (const auto &part : layout) {
        if (!allParts.contains(part) || normalized.contains(part))
            continue;
        normalized.append(part);
    }
    for (const auto &part : allParts) {
        if (!normalized.contains(part))
            normalized.append(part);
    }

    return normalized;
}

ExportDirNameResult arrangeExportDirName(
    const QList<ExportPathComponent> &layout,
    const QString &subjectId,
    const QString &time,
    const QString &experimentId,
    bool flat)
{
    const auto safeSubjectId = subjectId.trimmed().replace('/', '_').replace('\\', '_');
    const auto safeExperimentId = experimentId.trimmed().replace('/', '_').replace('\\', '_');
    ;

    QStringList parts;
    for (const auto &part : layout) {
        if (part == ExportPathComponent::SubjectId)
            parts.append(safeSubjectId);
        else if (part == ExportPathComponent::Time)
            parts.append(time);
        else if (part == ExportPathComponent::ExperimentId)
            parts.append(safeExperimentId);
    }
    if (parts.size() != 3)
        parts = {safeSubjectId, time, safeExperimentId};

    ExportDirNameResult result;
    if (!parts[0].isEmpty())
        result.flatId = parts[0] + "_";
    if (!parts[1].isEmpty())
        result.flatId.append(parts[1] + "_");
    if (!parts[2].isEmpty())
        result.flatId.append(parts[2] + "_");
    result.flatId.removeLast();

    if (flat) {
        result.dirName = result.flatId;
        return result;
    } else {
        result.dirName = QStringLiteral("%1/%2/%3").arg(parts.at(0), parts.at(1), parts.at(2));
        return result;
    }
}

} // namespace Syntalos
