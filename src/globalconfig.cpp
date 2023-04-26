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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "globalconfig.h"

#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

#include "rtkit.h"
#include "utils/misc.h"

using namespace Syntalos;

namespace Syntalos {
    Q_LOGGING_CATEGORY(logGlobalConfig, "global.config")
}

GlobalConfig::GlobalConfig(QObject *parent)
    : QObject(parent)
{
    m_s = new QSettings("DraguhnLab", "Syntalos", this);

    m_userHome = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (isInFlatpakSandbox())
        m_appDataRoot = QDir(m_userHome).filePath(".var/app/io.github.bothlab.syntalos/data");
    else
        m_appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (m_userHome.isEmpty())
        qCCritical(logGlobalConfig, "Unable to determine user home directory!");
    if (m_appDataRoot.isEmpty())
        qCCritical(logGlobalConfig, "Unable to determine application data directory!");
}

QString GlobalConfig::iconThemeName() const
{
    return m_s->value("ui/icon_theme", QStringLiteral("breeze")).toString();
}

void GlobalConfig::setIconThemeName(const QString &iconTheme)
{
    m_s->setValue("ui/icon_theme", iconTheme);
}

ColorMode GlobalConfig::appColorMode() const
{
    const auto strMode = m_s->value("ui/color_mode", QStringLiteral("system")).toString();
    return colorModeFromString(strMode);
}

void GlobalConfig::setAppColorMode(ColorMode mode)
{
    m_s->setValue("ui/color_mode", colorModeToString(mode));
}

QByteArray GlobalConfig::mainWinGeometry() const
{
    return m_s->value("ui/geometry").toByteArray();
}

void GlobalConfig::setMainWinGeometry(const QByteArray &geometry)
{
    m_s->setValue("ui/geometry", geometry);
}

QByteArray GlobalConfig::mainWinState() const
{
    return m_s->value("ui/window_state").toByteArray();
}

void GlobalConfig::setMainWinState(const QByteArray &state)
{
    m_s->setValue("ui/window_state", state);
}

int GlobalConfig::defaultThreadNice() const
{
    RtKit rtkit;
    const auto minNice = rtkit.queryMinNiceLevel();
    int nice = m_s->value("engine/default_thread_nice", -10).toInt();
    if (nice > 20)
        nice = 20;
    else if (nice < minNice)
        nice = minNice;
    return nice;
}

void GlobalConfig::setDefaultThreadNice(int nice)
{
    if (nice > 20)
        nice = 20;
    else if (nice < -19)
        nice = -19;
    m_s->setValue("engine/default_thread_nice", nice);
}

int GlobalConfig::defaultRTThreadPriority() const
{
    RtKit rtkit;
    const auto maxPrio = rtkit.queryMaxRealtimePriority();
    int prio = m_s->value("engine/default_rt_thread_priority", 20).toInt();
    if (prio > 99)
        prio = 99;
    else if (prio > maxPrio)
        prio = maxPrio;

    return prio;
}

void GlobalConfig::setDefaultRTThreadPriority(int priority)
{
    if (priority > 99)
        priority = 99;
    else if (priority < 1)
        priority = 1;
    m_s->setValue("engine/default_rt_thread_priority", priority);
}

bool GlobalConfig::explicitCoreAffinities() const
{
    return m_s->value("engine/explicit_core_affinities", false).toBool();
}

void GlobalConfig::setExplicitCoreAffinities(bool enabled)
{
    m_s->setValue("engine/explicit_core_affinities", enabled);
}

bool GlobalConfig::showDevelModules() const
{
    return m_s->value("devel/show_devel_modules", false).toBool();
}

void GlobalConfig::setShowDevelModules(bool enabled)
{
    m_s->setValue("devel/show_devel_modules", enabled);
}

bool GlobalConfig::saveExperimentDiagnostics() const
{
    return m_s->value("devel/save_diagnostics", false).toBool();
}

void GlobalConfig::setSaveExperimentDiagnostics(bool enabled)
{
    m_s->setValue("devel/save_diagnostics", enabled);
}

QString GlobalConfig::appDataLocation() const
{
    return m_appDataRoot;
}

QString GlobalConfig::userModulesDir() const
{
    return QDir(m_appDataRoot).filePath("modules");
}

QString GlobalConfig::virtualenvDir() const
{
    return QDir(m_appDataRoot).filePath("venv");
}

void GlobalConfig::triggerCreateVirtualenvUserLink()
{
    if (!createVEnvUserLink())
        return;

    QDir venvDir(virtualenvDir());
    if (!venvDir.exists() || venvDir.isEmpty())
        return;

    const auto linkFname = QDir(m_userHome).filePath("SyntalosVEnvs");
    QFileInfo fi(linkFname);
    if (fi.exists())
        return;
    QFile::link(virtualenvDir(), linkFname);
}

bool GlobalConfig::createVEnvUserLink() const
{
    return m_s->value("devel/venv_user_link", false).toBool();
}

void GlobalConfig::setCreateVEnvUserLink(bool enabled)
{
    if (enabled)
        triggerCreateVirtualenvUserLink();
    m_s->setValue("devel/venv_user_link", enabled);
}

QString Syntalos::colorModeToString(ColorMode mode)
{
    switch (mode) {
    case ColorMode::BRIGHT: return QStringLiteral("bright");
    case ColorMode::DARK: return QStringLiteral("dark");
    default:
        return QStringLiteral("system");
    }
}

ColorMode Syntalos::colorModeFromString(const QString &str)
{
    if (str == "bright")
        return ColorMode::BRIGHT;
    if (str == "dark")
        return ColorMode::DARK;
    return ColorMode::SYSTEM;
}
