//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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

#ifndef XMLINTERFACE_H
#define XMLINTERFACE_H
#include <QString>
#include <QVector>

#include "rhxglobals.h"
#include "systemstate.h"
#include "signalsources.h"

class SystemState;
class QXmlStreamWriter;
class QXmlStreamReader;
class StateSingleItem;
class ControllerInterface;

enum XMLIncludeParameters {
    XMLIncludeStimParameters,
    XMLIncludeSpikeSortingParameters,
    XMLIncludeGlobalParameters,
    XMLIncludeProbeMapSettings
};

class XMLInterface
{
public:
    XMLInterface(SystemState* state_, ControllerInterface* controllerInterface_, XMLIncludeParameters includeParameters_);

    bool loadFile(const QString& filename, QString &errorMessage, bool stimLegacy = false, bool probeMap = false) const;
    bool saveFile(const QString& filename) const;

    bool parseByteArray(const QByteArray &byteArray, QString &errorMessage, bool probeMap) const;

    void saveAsElement(QXmlStreamWriter &stream) const; // Save as an XML element.

private:
    QByteArray saveByteArray() const; // Save as a single QByteArray.

    bool parseDocumentStart(const QByteArray &byteArray, QString &errorMessage, bool &ignoreStimParameters, bool probeMap = false) const;
    bool parseGeneralConfig(const QByteArray &byteArray, QString &errorMessage) const;
    bool parseSignalGroups(const QByteArray &byteArray, QString &errorMessage) const;
    bool parseStimParameters(const QByteArray &byteArray, QString &errorMessage) const;

    bool parseProbeMapSettingsDOM(const QByteArray &byteArray, QString &errorMessage) const;

    bool parseStimLegacy(const QByteArray &byteArray, QString &errorMessage) const;
    bool readElementLegacy(QXmlStreamReader &stream, StateSingleItem *item, QString legacyName) const;

    void writeGlobalDocStart(QXmlStreamWriter &stream) const; // Write the start of a global IntanRHX document and element.
    void writeGlobalDocEnd(QXmlStreamWriter &stream) const; // Write the end of a global IntanRHX element and document.

    void setOptionalAttribute(QXmlStreamReader &stream, const QString &attributeName, QString &destination, const QString &defaultValue) const;
    void setOptionalAttribute(QXmlStreamReader &stream, const QString &attributeName, float &destination, const float defaultValue) const;

    bool setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, QString &destination) const;
    bool setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, int &destination) const;
    bool setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, float &destination) const;

    SystemState* state;
    ControllerInterface* controllerInterface;
    XMLIncludeParameters includeParameters;
};

#endif // XMLINTERFACE_H
