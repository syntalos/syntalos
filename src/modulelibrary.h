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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QList>
#include <QPixmap>
#include <QScopedPointer>
#include <QLoggingCategory>

namespace Syntalos {
class ModuleInfo;

Q_DECLARE_LOGGING_CATEGORY(logModLibrary)
}
using namespace Syntalos;

/**
 * @brief The ModuleLibrary class
 * Manage the lifecycle of MazeAmaze modules.
 */
class ModuleLibrary : public QObject
{
    Q_OBJECT
public:
    explicit ModuleLibrary(QObject *parent = nullptr);
    ~ModuleLibrary();

    bool load();
    void refreshIcons();

    QList<QSharedPointer<ModuleInfo>> moduleInfo() const;
    QSharedPointer<ModuleInfo> moduleInfo(const QString &id);

    QString issueLogHtml() const;

private:
    Q_DISABLE_COPY(ModuleLibrary)
    class Private;
    QScopedPointer<Private> d;

    bool loadLibraryModInfo(const QString &modId, const QString &modDir, const QString &libFname);
    bool loadPythonModInfo(const QString &modId, const QString &modDir, const QVariantHash &modData);
    void logModuleIssue(const QString &modId, const QString &context, const QString &msg);
};
