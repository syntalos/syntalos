/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QLoggingCategory>
#include <QObject>

class QSettings;

namespace Syntalos
{

Q_DECLARE_LOGGING_CATEGORY(logGlobalConfig)

enum class ColorMode {
    SYSTEM,
    BRIGHT,
    DARK
};

QString colorModeToString(ColorMode mode);
ColorMode colorModeFromString(const QString &str);

QString findSyntalosPyWorkerBinary();

class GlobalConfig : public QObject
{
    Q_OBJECT

public:
    explicit GlobalConfig(QObject *parent = nullptr);

    QString iconThemeName() const;
    void setIconThemeName(const QString &iconTheme);

    ColorMode appColorMode() const;
    void setAppColorMode(ColorMode mode);

    QByteArray mainWinGeometry() const;
    void setMainWinGeometry(const QByteArray &geometry);

    QByteArray mainWinState() const;
    void setMainWinState(const QByteArray &state);

    int defaultThreadNice() const;
    void setDefaultThreadNice(int nice);

    int defaultRTThreadPriority() const;
    void setDefaultRTThreadPriority(int priority);

    bool explicitCoreAffinities() const;
    void setExplicitCoreAffinities(bool enabled);

    bool showDevelModules() const;
    void setShowDevelModules(bool enabled);

    bool saveExperimentDiagnostics() const;
    void setSaveExperimentDiagnostics(bool enabled);

    QString appDataLocation() const;

    QString userModulesDir() const;
    QString virtualenvDir() const;
    QString homeDevelDir() const;

    bool useVenvForPyScript() const;
    void setUseVenvForPyScript(bool enabled);

    bool emergencyOOMStop() const;
    void setEmergencyOOMStop(bool enabled);

private:
    QSettings *m_s;
    QString m_userHome;
    QString m_appDataRoot;
};

} // namespace Syntalos
