/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QColor>
#include <QFile>
#include <QIcon>

// This file contains globally valid styling information that not only the Syntalos main application,
// but also any modules should have access to.

// define a few commonly used color code for Syntalos
const auto SyColorDanger = QColor(218, 68, 83);
const auto SyColorDangerHigh = QColor(237, 21, 21);
const auto SyColorWarning = QColor(246, 116, 0);
const auto SyColorSuccess = QColor(39, 174, 96);

const auto SyColorWhite = QColor(252, 252, 252);
const auto SyColorDark = QColor(44, 62, 80);

void setDefaultStyle(bool preferBreeze = false);
bool currentThemeIsDark();

/**
 * Apply an icon to the selected widget from our internal resource,
 * selecting a dark variant if necessary.
 */
template<typename T>
void setWidgetIconFromResource(const T &widget, const QString &name, bool isDark)
{
    QString path;
    bool pathValid = false;
    if (isDark) {
        path = QStringLiteral(":/icons/dark/") + name;
        pathValid = QFile::exists(path);
    }
    if (!pathValid)
        path = QStringLiteral(":/icons/") + name;
    widget->setIcon(QIcon(path));
}
