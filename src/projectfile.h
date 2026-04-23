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

#include <QString>

#include "entitylistmodels.h"
#include "exportdirutils.h"

class FlowGraphView;

namespace Syntalos
{

class Engine;

struct ProjectStorageSettings {
    QList<ExportPathComponent> exportDirLayout{};
    bool clockTimeInDir{false};
    bool simpleNames{true};
    bool flatRoot{false};

    QString exportBaseDir;
    QString experimentId;
};

using StatusMessageFn = std::function<void(const QString &)>;

bool saveProjectConfiguration(
    Engine *engine,
    FlowGraphView *graphView,
    TestSubjectListModel *subjectList,
    ExperimenterListModel *experimenterList,
    const ProjectStorageSettings &storage,
    const QString &fileName,
    StatusMessageFn statusFn = {});

bool loadProjectConfigurationInteractive(
    Engine *engine,
    FlowGraphView *graphView,
    TestSubjectListModel *subjectList,
    ExperimenterListModel *experimenterList,
    ProjectStorageSettings &outStorage,
    const QString &fileName,
    QWidget *parent = nullptr,
    std::function<void(void)> preLoadFn = {},
    StatusMessageFn statusFn = {});

} // namespace Syntalos
