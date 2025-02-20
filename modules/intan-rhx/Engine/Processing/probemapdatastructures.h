//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef PROBEMAPDATASTRUCTURES_H
#define PROBEMAPDATASTRUCTURES_H

#include <QString>
#include <QVector>

typedef struct Text_ {
    float x;
    float y;
    float bottomLeftX;
    float bottomLeftY;
    float topLeftX;
    float topLeftY;
    float bottomRightX;
    float bottomRightY;
    float rotation;
    float fontHeight;
    QString fontColor;
    QString textAlignment;
    QString text;
} Text;


typedef struct Line_ {
    float x1;
    float x2;
    float y1;
    float y2;
    QString lineColor;
} Line;

typedef struct ElectrodeSite_ {
    int channelNumber;
    float x;
    float y;
    float siteWidth;
    float siteHeight;
    QString siteShape;
    QString siteOutlineColor;
    float boundXmin;
    float boundXmax;
    float boundYmin;
    float boundYmax;
    QString nativeName;

    bool highlighted;
    bool enabled;
    bool linked;
    QString color;
    bool impedanceValid;
    float impedanceMag;
    float impedancePhase;
    float spikeTime;

} ElectrodeSite;


typedef struct Port_ {
    QString name;
    QString siteOutlineColor;
    QString siteShape;
    float siteWidth;
    float siteHeight;
    QVector<ElectrodeSite> electrodeSites;
} Port;


typedef struct Page_ {
    QString name;
    QString backgroundColor;
    QString siteOutlineColor;
    QString lineColor;
    float fontHeight;
    QString fontColor;
    QString textAlignment;
    QString siteShape;
    float siteWidth;
    float siteHeight;
    QVector<Port> ports;
    QVector<Line> lines;
    QVector<Text> texts;
} Page;


typedef struct ProbeMapSettings_ {
    QString backgroundColor;
    QString siteOutlineColor;
    QString lineColor;
    float fontHeight;
    QString fontColor;
    QString textAlignment;
    QString siteShape;
    float siteWidth;
    float siteHeight;
    QVector<Page> pages;
} ProbeMapSettings;


#endif // PROBEMAPDATASTRUCTURES_H
