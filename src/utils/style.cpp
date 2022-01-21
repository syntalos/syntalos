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

#include "style.h"

#include <QStyleFactory>
#include <QApplication>
#include <QIcon>
#include <QDebug>
#include <QFileInfo>
#include <QPalette>

void setDefaultStyle(bool preferBreeze)
{
    // explicit style overrides take precendence
    if (!qgetenv("QT_STYLE_OVERRIDE").isEmpty())
        return;

    const auto desktopEnv = QString::fromUtf8(qgetenv("XDG_CURRENT_DESKTOP"));
    const auto availableStyle = QStyleFactory::keys();
    const bool isBreezeAvailable = availableStyle.contains(QStringLiteral("Breeze"));

    // we trust KDE has configured sensible default styles already
    if (desktopEnv.endsWith(QStringLiteral("KDE")))
        return;

    // check if we should use the Breeze style
    if (isBreezeAvailable && preferBreeze && qgetenv("SYNTALOS_USE_NATIVE_STYLE").isEmpty()) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Breeze")));
        return;
    }

    // test for a GNOME desktop and set Adwaita style if we have it
    if (desktopEnv.endsWith(QStringLiteral("GNOME"))) {
        if (availableStyle.contains(QStringLiteral("Adwaita"))) {
                QApplication::setStyle(QStyleFactory::create(QStringLiteral("Adwaita")));
                return;
        }
    }

    // if we are here, we just use whatever Qt things should be default - usually this means the "Fusion" style
}

bool switchIconTheme(const QString &themeName)
{
    if (themeName.isEmpty())
        return false;

    QString realThemeName = themeName;
    if (currentThemeIsDark() && themeName.toLower() == "breeze")
        realThemeName = QStringLiteral("breeze-dark");

    if (QIcon::themeName() == realThemeName)
        return true;

    auto found = false;
    for (auto &path : QIcon::themeSearchPaths()) {
        QFileInfo fi(QStringLiteral("%1/%2").arg(path).arg(realThemeName));
        if (fi.isDir()) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;
    QIcon::setThemeName(realThemeName);
    qDebug().noquote() << "Switched icon theme to" << realThemeName;

    return true;
}

bool currentThemeIsDark()
{
    const QPalette pal;
    const auto bgColor = pal.button().color();
    return bgColor.value() < 128;
}
