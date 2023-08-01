/*
 * Copyright (C) 2018-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "appstyle.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>

#include <KColorScheme>
#include <KSharedConfig>

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

bool darkColorSchemeAvailable()
{
    const auto fname = QStringLiteral("/usr/share/color-schemes/BreezeDark.colors");
    QFile file(fname);
    if (!file.exists()) {
        qDebug().noquote() << "Could not find dark color scheme file";
        return false;
    }
    return true;
}

static void changeColorScheme(const QString &filename, bool darkColors = false)
{
    QFile file(filename);
    if (!file.exists()) {
        qWarning().noquote() << "Could not find color scheme file" << filename << "Colors will not be changed.";
        return;
    }

    auto config = KSharedConfig::openConfig(filename);

    QPalette palette = qApp->palette();
    QPalette::ColorGroup states[3] = {QPalette::Active, QPalette::Inactive, QPalette::Disabled};
    KColorScheme schemeTooltip(QPalette::Active, KColorScheme::Tooltip, config);

    for (int i = 0; i < 3; ++i) {
        QPalette::ColorGroup state = states[i];
        KColorScheme schemeView(state, KColorScheme::View, config);
        KColorScheme schemeWindow(state, KColorScheme::Window, config);
        KColorScheme schemeButton(state, KColorScheme::Button, config);
        KColorScheme schemeSelection(state, KColorScheme::Selection, config);

        palette.setBrush(state, QPalette::WindowText, schemeWindow.foreground());
        palette.setBrush(state, QPalette::Window, schemeWindow.background());
        palette.setBrush(state, QPalette::Base, schemeView.background());
        palette.setBrush(state, QPalette::Text, schemeView.foreground());
        palette.setBrush(state, QPalette::Button, schemeButton.background());
        palette.setBrush(state, QPalette::ButtonText, schemeButton.foreground());
        palette.setBrush(state, QPalette::Highlight, schemeSelection.background());
        palette.setBrush(state, QPalette::HighlightedText, schemeSelection.foreground());
        palette.setBrush(state, QPalette::ToolTipBase, schemeTooltip.background());
        palette.setBrush(state, QPalette::ToolTipText, schemeTooltip.foreground());

        palette.setColor(state, QPalette::Light, schemeWindow.shade(KColorScheme::LightShade));
        palette.setColor(state, QPalette::Midlight, schemeWindow.shade(KColorScheme::MidlightShade));
        palette.setColor(state, QPalette::Mid, schemeWindow.shade(KColorScheme::MidShade));
        palette.setColor(state, QPalette::Dark, schemeWindow.shade(KColorScheme::DarkShade));
        palette.setColor(state, QPalette::Shadow, schemeWindow.shade(KColorScheme::ShadowShade));

        palette.setBrush(state, QPalette::AlternateBase, schemeView.background(KColorScheme::AlternateBackground));
        palette.setBrush(state, QPalette::Link, schemeView.foreground(KColorScheme::LinkText));
        palette.setBrush(state, QPalette::LinkVisited, schemeView.foreground(KColorScheme::VisitedText));
    }

    qApp->setProperty("KDE_COLOR_SCHEME_PATH", filename);
    qApp->setPalette(palette);

    QIcon::setThemeName(darkColors ? QStringLiteral("breeze-dark") : QStringLiteral("breeze"));
}

void changeColorsDarkmode(bool enabled)
{
    if (enabled)
        changeColorScheme(QStringLiteral("/usr/share/color-schemes/BreezeDark.colors"), true);
    else
        changeColorScheme(QStringLiteral("/usr/share/color-schemes/BreezeLight.colors"), false);
}
