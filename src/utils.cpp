/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utils.h"

#include <chrono>

using namespace std::chrono;

QString
ExperimentFeatures::toHumanString()
{
    if (videoEnabled && trackingEnabled && ephysEnabled && ioEnabled)
        return QStringLiteral("Maze");
    if (videoEnabled && ephysEnabled && !ioEnabled && !trackingEnabled)
        return QStringLiteral("Resting Box");

    return QStringLiteral("Custom");
}

QString
ExperimentFeatures::toString()
{
    if (videoEnabled && trackingEnabled && ephysEnabled && ioEnabled)
        return QStringLiteral("maze");
    if (videoEnabled && ephysEnabled && !ioEnabled && !trackingEnabled)
        return QStringLiteral("resting-box");

    return QStringLiteral("custom");
}

QJsonObject
ExperimentFeatures::toJson()
{
    QJsonObject json;
    json.insert("ephys", ephysEnabled);
    json.insert("io", ioEnabled);
    json.insert("video", videoEnabled);
    json.insert("tracking", trackingEnabled);

    return json;
}

void ExperimentFeatures::fromJson(const QJsonObject& json)
{
    if (json.empty()) {
        this->enableAll(); // backwards compatibility
        return;
    }

    ephysEnabled = json.value("ephys").toBool();
    ioEnabled    = json.value("io").toBool();
    videoEnabled = json.value("video").toBool();
    trackingEnabled = json.value("tracking").toBool();
}

bool ExperimentFeatures::isAnyEnabled()
{
    return ephysEnabled || ioEnabled || videoEnabled || trackingEnabled;
}

void ExperimentFeatures::enableAll()
{
    videoEnabled = true;
    trackingEnabled = true;
    ephysEnabled = true;
    ioEnabled = true;
}

time_t getMsecEpoch()
{
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    return ms.count();
}

QString createRandomString(int len)
{
    const auto possibleCahrs = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

    QString str;
    for (int i=0; i < len; i++) {
        int index = qrand() % possibleCahrs.length();
        QChar nextChar = possibleCahrs.at(index);
        str.append(nextChar);
    }

    return str;
}
