/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "projectgen.h"

#include <KTar>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSet>

#include "utils/tomlutils.h"

namespace SyBench
{

ModuleSpec &ModuleSpec::subscribe(const QString &inPortId, const QString &srcModuleName, const QString &srcPortId)
{
    subscriptions.insert(inPortId, PortSubscription{srcModuleName, srcPortId});
    return *this;
}

void ProjectSpec::addModule(ModuleSpec mod)
{
    modules.append(std::move(mod));
}

const ModuleSpec *ProjectSpec::module(const QString &name) const
{
    for (const auto &m : modules) {
        if (m.name == name)
            return &m;
    }
    return nullptr;
}

bool ProjectSpec::hasModule(const QString &name) const
{
    return module(name) != nullptr;
}

/**
 * Place modules on the graph canvas as columns by dependency depth,
 * so the project looks sensible when opened in Syntalos.
 */
static QVariantHash makeGraphSettings(const ProjectSpec &spec)
{
    QHash<QString, int> depth;
    // modules are added in dependency order by construction, but we still resolve
    // depths generically to not depend on that
    for (int pass = 0; pass < spec.modules.size(); ++pass) {
        for (const auto &m : spec.modules) {
            int d = 0;
            for (const auto &sub : m.subscriptions)
                d = std::max(d, depth.value(sub.srcModuleName, 0) + 1);
            depth[m.name] = d;
        }
    }

    QHash<int, int> rowInColumn;
    QVariantHash positions;
    for (const auto &m : spec.modules) {
        const int col = depth.value(m.name);
        const int row = rowInColumn[col]++;
        positions.insert(m.name, QVariantList{col * 260.0, row * 110.0});
    }

    QVariantHash graph;
    graph.insert(
        QStringLiteral("Canvas"),
        QVariantHash{
            {QStringLiteral("Zoom"), 1.0}
    });
    graph.insert(QStringLiteral("NodePositions"), positions);
    return graph;
}

auto writeProjectFile(const ProjectSpec &spec, const QString &fileName) -> std::expected<void, QString>
{
    QSet<QString> names;
    for (const auto &m : spec.modules) {
        if (m.id.isEmpty() || m.name.isEmpty())
            return std::unexpected(QStringLiteral("Module without id or name in project specification."));
        if (names.contains(m.name))
            return std::unexpected(QStringLiteral("Duplicate module name '%1' in project specification.").arg(m.name));
        names.insert(m.name);
    }
    for (const auto &m : spec.modules) {
        for (const auto &sub : m.subscriptions) {
            if (!names.contains(sub.srcModuleName))
                return std::unexpected(
                    QStringLiteral("Module '%1' subscribes to unknown module '%2'.").arg(m.name, sub.srcModuleName));
        }
    }

    KTar tar(fileName);
    if (!tar.open(QIODevice::WriteOnly))
        return std::unexpected(QStringLiteral("Unable to open '%1' for writing.").arg(fileName));

    const auto timeCreated = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch());
    const auto writeEntry = [&](const QString &name, const QByteArray &data) {
        return tar.writeFile(name, data, 0100644, QString(), QString(), timeCreated, timeCreated, timeCreated);
    };
    const auto writeDirEntry = [&](const QString &name) {
        return tar.writeDir(name, QString(), QString(), 040755, timeCreated, timeCreated, timeCreated);
    };

    QVariantHash settings;
    settings.insert(QStringLiteral("version_format"), QStringLiteral("1"));
    settings.insert(QStringLiteral("version_app"), QCoreApplication::applicationVersion());
    settings.insert(QStringLiteral("time_created"), timeCreated);
    settings.insert(QStringLiteral("export_base_dir"), spec.exportBaseDir);
    settings.insert(QStringLiteral("experiment_id"), spec.experimentId);

    QVariantHash storageSettings;
    storageSettings.insert(
        QStringLiteral("order"),
        QStringList{QStringLiteral("subject-id"), QStringLiteral("time"), QStringLiteral("experiment-id")});
    storageSettings.insert(QStringLiteral("clock_time_in_dir"), false);
    storageSettings.insert(QStringLiteral("simple_names"), false);
    storageSettings.insert(QStringLiteral("flat_root"), false);
    storageSettings.insert(QStringLiteral("add_moniker"), true);
    settings.insert(QStringLiteral("storage"), storageSettings);

    QVariantHash netSettings;
    netSettings.insert(QStringLiteral("controller_enabled"), false);
    netSettings.insert(QStringLiteral("listener_enabled"), false);
    netSettings.insert(QStringLiteral("expected_listener_count"), 0);
    netSettings.insert(QStringLiteral("timeout_ms"), 6000);
    settings.insert(QStringLiteral("network"), netSettings);

    if (!writeEntry(QStringLiteral("main.toml"), qVariantHashToTomlData(settings)))
        return std::unexpected(QStringLiteral("Failed to write main.toml"));
    writeEntry(QStringLiteral("subjects.toml"), QByteArray());
    writeEntry(QStringLiteral("experimenters.toml"), QByteArray());
    writeEntry(QStringLiteral("graph.toml"), qVariantHashToTomlData(makeGraphSettings(spec)));

    int modIndex = 0;
    for (const auto &m : spec.modules) {
        const auto dirEntryId = QStringLiteral("%1-%2").arg(modIndex, 3, 10, QChar('0')).arg(m.id);
        if (!writeDirEntry(dirEntryId))
            return std::unexpected(QStringLiteral("Failed to write directory entry for '%1'").arg(m.name));

        if (!m.settings.isEmpty())
            writeEntry(QStringLiteral("%1/%2.toml").arg(dirEntryId, m.id), qVariantHashToTomlData(m.settings));
        if (!m.extraData.isEmpty())
            writeEntry(QStringLiteral("%1/%2.dat").arg(dirEntryId, m.id), m.extraData);

        QVariantHash modSubs;
        for (auto it = m.subscriptions.cbegin(); it != m.subscriptions.cend(); ++it)
            modSubs.insert(it.key(), QVariantList{it.value().srcModuleName, it.value().srcPortId});

        QVariantHash modInfo;
        modInfo.insert(QStringLiteral("id"), m.id);
        modInfo.insert(QStringLiteral("name"), m.name);
        modInfo.insert(QStringLiteral("enabled"), true);
        modInfo.insert(QStringLiteral("stop_on_failure"), true);
        modInfo.insert(QStringLiteral("subscriptions"), modSubs);
        writeEntry(QStringLiteral("%1/info.toml").arg(dirEntryId), qVariantHashToTomlData(modInfo));

        modIndex++;
    }

    if (!tar.close())
        return std::unexpected(QStringLiteral("Failed to finalize '%1'.").arg(fileName));
    return {};
}

} // namespace SyBench
