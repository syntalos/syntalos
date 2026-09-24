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

#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantHash>
#include <expected>

namespace SyBench
{

/**
 * @brief Source of an input port subscription
 */
struct PortSubscription {
    QString srcModuleName;
    QString srcPortId;
};

/**
 * @brief One module in a generated benchmark project
 */
struct ModuleSpec {
    QString id;   /// module type id, e.g. "flowmeter"
    QString name; /// unique module name shown in the graph; subscriptions refer to this
    QVariantHash settings;
    QByteArray extraData;
    QHash<QString, PortSubscription> subscriptions; /// input port id -> data source

    ModuleSpec &subscribe(const QString &inPortId, const QString &srcModuleName, const QString &srcPortId);
};

/**
 * @brief Description of a complete Syntalos project (.syct)
 *
 * The generator writes exactly the layout Syntalos itself saves, so the resulting
 * file can also be opened in the GUI to inspect a benchmark scenario.
 */
struct ProjectSpec {
    QString experimentId{QStringLiteral("benchmark")};
    QString exportBaseDir;
    QList<ModuleSpec> modules;

    void addModule(ModuleSpec mod);
    [[nodiscard]] const ModuleSpec *module(const QString &name) const;
    [[nodiscard]] bool hasModule(const QString &name) const;
};

/**
 * @brief Write a project description as Syntalos .syct file
 */
auto writeProjectFile(const ProjectSpec &spec, const QString &fileName) -> std::expected<void, QString>;

} // namespace SyBench
