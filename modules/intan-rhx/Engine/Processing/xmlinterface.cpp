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

#include "xmlinterface.h"
#include "controllerinterface.h"
#include <QtXml>
#include <QFileDialog>
#include <QMessageBox>
#include <QTranslator>


XMLInterface::XMLInterface(SystemState *state_, ControllerInterface* controllerInterface_, XMLIncludeParameters includeParameters_) :
    state(state_),
    controllerInterface(controllerInterface_),
    includeParameters(includeParameters_)
{
}

bool XMLInterface::loadFile(const QString &filename, QString &errorMessage, bool stimLegacy, bool probeMap) const
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = "Unable to open input file";
        return false;
    }
    QByteArray byteArray = file.readAll();
    file.close();

    if (stimLegacy) {
        if (!parseStimLegacy(byteArray, errorMessage)) {
            qDebug() << "Failure parsing XML data. Error message: " << errorMessage;
            return false;
        }
    } else {
        if (!parseByteArray(byteArray, errorMessage, probeMap)) {
            qDebug() << "Failure parsing XML data. Error message: " << errorMessage;
            return false;
        }
    }
    return true;
}

bool XMLInterface::saveFile(const QString &filename) const
{
    QByteArray fileAsByteArray = saveByteArray();

    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Unable to open output file.";
        return false;
    }

    file.write(fileAsByteArray);
    file.close();
    return true;
}

bool XMLInterface::parseByteArray(const QByteArray &byteArray, QString &errorMessage, bool probeMap) const
{
    state->holdUpdate();
    bool ignoreStimParameters = false;
    if (!parseDocumentStart(byteArray, errorMessage, ignoreStimParameters, probeMap))  {
        state->releaseUpdate();
        return false;
    }
    if (probeMap) {
        if (!parseProbeMapSettingsDOM(byteArray, errorMessage)) {
            state->releaseUpdate();
            return false;
        }
    } else {
        if (!parseGeneralConfig(byteArray, errorMessage)) {
            state->releaseUpdate();
            return false;
        }
        if (!parseSignalGroups(byteArray, errorMessage)) {
            state->releaseUpdate();
            return false;
        }
        if (state->getControllerTypeEnum() == ControllerStimRecordUSB2 && !ignoreStimParameters &&
                (includeParameters == XMLIncludeGlobalParameters || includeParameters == XMLIncludeStimParameters)) {
            if (!parseStimParameters(byteArray, errorMessage)) {
                state->releaseUpdate();
                return false;
            }
        }
    }
    state->releaseUpdate();
    return true;
}

void XMLInterface::saveAsElement(QXmlStreamWriter &stream) const
{   
    // Write GeneralConfig element
    QStringList generalConfigList;

    // Get general and spike settings attributes
    QStringList generalGlobalList = state->getAttributes(XMLGroupGeneral);
    QStringList spikeSettingsGlobalList = state->getAttributes(XMLGroupSpikeSettings);

    // If Global Settings are being saved (XMLIncludeGlobalParameters), add those attributes to generalConfigList
    if (includeParameters == XMLIncludeGlobalParameters) {
        generalConfigList = generalGlobalList + spikeSettingsGlobalList;
    } else if (includeParameters == XMLIncludeSpikeSortingParameters) {
        // If Spike Settings are being saved (XMLIncludeSpikeSortingParameters), add those attributes to generalConfigList
        generalConfigList = spikeSettingsGlobalList;
    }

    generalConfigList.sort();

    if (generalConfigList.size() > 0) {
        // Begin GeneralConfig element
        stream.writeStartElement("GeneralConfig");

        // Write attributes
        for (auto attribute : generalConfigList) {
            // Separate colon-separated QString subsections into separate QStrings 'name' and 'value'
            QStringList subsections = attribute.split(":_:");
            QString name = "\n\t\t" + subsections.at(0);
            QString value = subsections.at(1);

            // Write attribute 'name' and 'value'
            stream.writeAttribute(name, value);
        }

        // Write unique TCPDataOutputChannels attribute.
        QStringList tcpChannelList = state->getTCPDataOutputChannels();
        QString tcpChannelString("");
        if (tcpChannelList.size() > 0) {
            // Start with first entry in the list, with no preceding comma.
            tcpChannelString.append(tcpChannelList.at(0));
            // Go through all entries in the list (after the first), appending a preceding comma before each entry.
            for (int i = 1; i < tcpChannelList.size(); i++) {
                tcpChannelString.append("," + tcpChannelList.at(i));
            }
        }
        stream.writeAttribute("\n\t\tTCPDataOutputChannels", tcpChannelString);

        // End GeneralConfig element
        stream.device()->write("\n\t");
        stream.writeEndElement();
    }

    // Write SignalGroup element for each SignalGroup that has a non-zero number of channels
    for (int group = 0; group < state->signalSources->numGroups(); group++) {

        // If only Stim Parameters are being saved (only for XMLIncludeStimParameters), skip all SignalGroups
        if (includeParameters == XMLIncludeStimParameters) {
            break;
        }

        SignalGroup *thisGroup = state->signalSources->groupByIndex(group);
        if (thisGroup->numChannels() == 0) {
            continue;
        }
        QStringList signalGroupList;

        // Get all attributes (excluding XMLGroupNone, because they're never saved)
        QStringList generalGroupList = thisGroup->getAttributes(XMLGroupGeneral);
        QStringList spikeSettingsGroupList = thisGroup->getAttributes(XMLGroupSpikeSettings);
        QStringList readOnlyGroupList = thisGroup->getAttributes(XMLGroupReadOnly);

        // If Global Settings are being saved (only for XMLIncludeGlobalParameters), add those attributes to signalGroupList
        if (includeParameters == XMLIncludeGlobalParameters) {
            signalGroupList = generalGroupList + spikeSettingsGroupList;
        } else if (includeParameters == XMLIncludeSpikeSortingParameters) {
            // If Spike Settings are being saved (XMLIncludeSpikeSortingParameters), add those attributes to signalGroupList
            signalGroupList = spikeSettingsGroupList;
        }

        signalGroupList.sort();

        // Always put read-only attributes first in signalGroupList
        signalGroupList = readOnlyGroupList + signalGroupList;

        if (signalGroupList.size() > 0) {
            // Begin SignalGroup element
            stream.writeStartElement("SignalGroup");

            // Write attributes
            for (auto attribute : signalGroupList) {
                // Separate colon-separated QString subsections into separate QStrings 'name' and 'value'
                QStringList subsections = attribute.split(":_:");
                QString name = "\n\t\t" + subsections.at(0);
                QString value = subsections.at(1);

                // Write attribute 'name' and 'value'
                stream.writeAttribute(name, value);
            }

            int numChannels = thisGroup->numChannels();
            if (thisGroup->prefix->getValueString() == "ANALOG-IN" || thisGroup->prefix->getValueString() == "ANALOG-OUT") {
                numChannels = AbstractRHXController::numAnalogIO(state->getControllerTypeEnum(), state->expanderConnected->getValue());
            } else if (thisGroup->prefix->getValueString() == "DIGITAL-IN" || thisGroup->prefix->getValueString() == "DIGITAL-OUT") {
                numChannels = AbstractRHXController::numDigitalIO(state->getControllerTypeEnum(), state->expanderConnected->getValue());
            }

            // Write Channel element for each channel
            for (int channel = 0; channel < numChannels; channel++) {
                Channel *thisChannel = thisGroup->channelByIndex(channel);
                QStringList channelList;

                // Get all attributes (excluding XMLGroupNone, because they're never saved, and excluding StimParameters, because they're saved later)
                QStringList generalChannelList = thisChannel->getAttributes(XMLGroupGeneral);
                QStringList spikeSettingsChannelList = thisChannel->getAttributes(XMLGroupSpikeSettings);
                QStringList readOnlyChannelList = thisChannel->getAttributes(XMLGroupReadOnly);

                // If Global Settings are being saved (only for XMLIncludeGlobalParameters), add those attributes to channelList
                if (includeParameters == XMLIncludeGlobalParameters) {
                    channelList = generalChannelList + spikeSettingsChannelList;
                } else if (includeParameters == XMLIncludeSpikeSortingParameters) {
                    // If Spike Settings are being saved (XMLIncludeSpikeSortingParameters), add those attributes to channelList
                    channelList = spikeSettingsChannelList;
                }

                channelList.sort();

                // Always put read-only attributes first in channelList
                channelList = readOnlyChannelList + channelList;

                if (channelList.size() > 0) {
                    // Begin Channel element
                    stream.writeStartElement("Channel");

                    // Write stand-alone attributes
                    for (auto attribute : channelList) {
                        // Separate colon-separated QString subsections into separate QStrings 'name' and 'value'
                        QStringList subsections = attribute.split(":_:");
                        QString name = "\n\t\t\t" + subsections.at(0);
                        QString value = subsections.at(1);

                        // Write attribute 'name' and 'value'
                        stream.writeAttribute(name, value);
                    }

                    // End Channel element
                    stream.device()->write("\n\t\t");
                    stream.writeEndElement();
                }
            }

            // End SignalGroup element
            stream.writeEndElement();
        }
    }

    // Write StimParameters element if (a) this is a Stim Controller and (b) Stim Parameters are in the include list
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2 &&
            (includeParameters == XMLIncludeGlobalParameters || includeParameters == XMLIncludeStimParameters)) {

        // Write StimParameters element
        stream.writeStartElement("StimParameters");

        // Go through all signal groups
        for (int group = 0; group < state->signalSources->numGroups(); ++group) {
            SignalGroup *thisGroup = state->signalSources->groupByIndex(group);

            // Go through all channels, getting all StimParameters attributes in this channel, and writing them in <Channel>
            for (int channel = 0; channel < thisGroup->numChannels(); channel++) {
                Channel *thisChannel = thisGroup->channelByIndex(channel);

                if (thisChannel->getSignalType() != AmplifierSignal && thisChannel->getSignalType() != BoardDacSignal && thisChannel->getSignalType() != BoardDigitalOutSignal) {
                    break;
                }

                // Get StimParameters attributes
                QStringList channelList = thisChannel->getAttributes(XMLGroupStimParameters);
                channelList.sort();
                if (channelList.size() < 1) {
                    continue;
                }

                // Get read-only attributes
                QStringList readOnlyChannelList = thisChannel->getAttributes(XMLGroupReadOnly);

                // Always put read-only attributes first.
                channelList = readOnlyChannelList + channelList;

                // Begin Channel element
                stream.writeStartElement("StimChannel");

                // Write attributes
                for (auto attribute : channelList) {
                    // Separate colon-separated QString subsections into separate QStrings 'name' and 'value'
                    QStringList subsections = attribute.split(":_:");
                    QString name = "\n\t\t\t" + subsections.at(0);
                    QString value = subsections.at(1);

                    // Write attribute 'name' and 'value'
                    stream.writeAttribute(name, value);
                }

                // End Channel element
                stream.device()->write("\n\t\t");
                stream.writeEndElement();
            }
        }
        // End StimParameters element
        stream.writeEndElement();
    }
}

bool XMLInterface::parseDocumentStart(const QByteArray &byteArray, QString &errorMessage, bool &ignoreStimParameters, bool probeMap) const
{
    QXmlStreamReader stream(byteArray);

    // Read document start
    QXmlStreamReader::TokenType token = stream.readNext();
    if (token == QXmlStreamReader::StartDocument) {
        token = stream.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (stream.name() == ApplicationName) {

                // Iterate through all XML attributes.
                QXmlStreamAttributes attributes = stream.attributes();
                for (auto attribute : attributes) {
                    // Get the attribute name and value from the XML
                    QString attributeName = attribute.name().toString();
                    QString attributeValue = attribute.value().toString();

                    // Try to find the attribute as a StateSingleItem in globalItems
                    StateSingleItem *singleItem = state->locateStateSingleItem(state->globalItems, attributeName);

                    // If the attribute is a StateSingleItem, compare the attribute value to the StateSingleItem value
                    if (singleItem) {
                        //if (singleItem->getDisplayValueString() != attributeValue) {
                        if (singleItem->getValueString() != attributeValue) {

                            // This read-only attribute is not matching. Either give a warning and continue, or give an error and return false.

                            // If sample rate or stim step size doesn't match, ignore stim parameters and give warning.
                            if (singleItem == (StateSingleItem*) state->sampleRate || singleItem == (StateSingleItem*) state->stimStepSize) {
                                errorMessage.append("Warning: " + singleItem->getParameterName() + " not matching. Stimulation Parameters will not be loaded.");
                                ignoreStimParameters = true;
                            }

                            // If version doesn't match, just give warning (unless it's a probe map file)
                            if (singleItem == (StateSingleItem*) state->softwareVersion && !probeMap) {
                                errorMessage.append("Warning: This file is from a different version of the IntanRHX software. Full compatibility cannot be guaranteed.");
                            }

                            // If type doesn't match, give an error.
                            if (singleItem == (StateSingleItem*) state->controllerType && !probeMap) {
                                errorMessage.append("Warning: This file is from a different Intan controller type. Loading will be canceled.");
                                return false;
                            }
                        }
                    }
                }
                return true;
            } else {
                errorMessage.append("Error: XML document doesn't start with an IntanRHX element");
                return false;
            }
        } else {
            errorMessage.append("Error: XML document starts with an invalid element");
            return false;
        }
    } else {
        errorMessage.append("Error: XML document starts with an invalid token");
        return false;
    }
}

bool XMLInterface::parseGeneralConfig(const QByteArray &byteArray, QString &errorMessage) const
{
    QXmlStreamReader stream(byteArray);

    bool dummyIgnore = false;
    QString dummyError("");
    bool validDocumentStart = parseDocumentStart(byteArray, dummyError, dummyIgnore);
    if (!validDocumentStart) return false;

    // Parse GeneralConfig
    while (stream.name() != "GeneralConfig") {
        stream.readNextStartElement();
        if (stream.atEnd()) return true;
    }

    // Clear all TCP channels
    state->signalSources->clearTCPDataOutput();

    // Iterate through all XML attributes.
    QXmlStreamAttributes attributes = stream.attributes();
    for (auto attribute : attributes) {
        // Get the attribute name and value from the XML.
        QString attributeName = attribute.name().toString();
        QString attributeValue = attribute.value().toString();

        // If attribute value is "N/A", then skip.
        if (attributeValue != "N/A") {
            // Try to find the attribute as a StateSingleItem in globalItems.
            StateSingleItem *singleItem = state->locateStateSingleItem(state->globalItems, attributeName);

            // If the attribute is a StateSingleItem, set it according to XMLIncludeParameters.
            if (singleItem) {
                bool changeItem = false;
                XMLGroup xmlGroup = singleItem->getXMLGroup();

                switch (includeParameters) {
                case XMLIncludeSpikeSortingParameters:
                    if (xmlGroup == XMLGroupSpikeSettings) changeItem = true;
                    break;
                case XMLIncludeGlobalParameters:
                    if (xmlGroup == XMLGroupSpikeSettings || xmlGroup == XMLGroupGeneral) changeItem = true;
                    break;
                default:
                    break;
                }

                if (changeItem) {
                    if (!singleItem->setValue(attributeValue)) {
                        errorMessage.append("Error: Failed to parse " + singleItem->getParameterName());
                        return false;
                    } else {
                        continue;
                    }
                }
            }

            // Try to find the attribute as a StateFilenameItem
            QString pathOrBase;
            StateFilenameItem *filenameItem = state->locateStateFilenameItem(state->stateFilenameItems, attributeName.toLower(), pathOrBase);

            // If the attribute is a StateFilenameItem, set it according to XMLIncludeParameters.
            if (filenameItem) {
                if (includeParameters == XMLIncludeGlobalParameters) {
                    if (pathOrBase == filenameItem->getPathParameterName().toLower()) {
                        filenameItem->setPath(attributeValue);
                        continue;
                    } else if (pathOrBase == filenameItem->getBaseFilenameParameterName().toLower()) {
                        filenameItem->setBaseFilename(attributeValue);
                        continue;
                    } else {
                        qDebug() << "Error: seems to be neither path nor basefilename... pathorbase: " << pathOrBase;
                        continue;
                    }
                }
            }

            // See if this attribute is the exception TCPDataOutputChannels.
            if (attributeName.toLower() == "tcpdataoutputchannels") {
                // Read the attribute value into a QStringList, separated by commas.
                QStringList tcpChannelList = attributeValue.split(',');
                // Go through each channel in tcpChannelList and find it in SignalSources
                for (auto channelName : tcpChannelList) {
                    Channel *thisChannel = state->signalSources->channelByName(channelName);
                    // If this channel couldn't be found, just continue
                    if (!thisChannel) continue;
                    // Set this channel to output to TCP
                    thisChannel->setOutputToTcp(true);
                }
            }
        }
    }

    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        controllerInterface->uploadAmpSettleSettings();
        controllerInterface->uploadChargeRecoverySettings();
    }

    return true;
}

bool XMLInterface::parseSignalGroups(const QByteArray &byteArray, QString &errorMessage) const
{
    QXmlStreamReader stream(byteArray);

    bool dummyIgnore = false;
    QString dummyError("");
    bool validDocumentStart = parseDocumentStart(byteArray, dummyError, dummyIgnore);
    if (!validDocumentStart) return false;

    // Parse SignalGroups
    while (!stream.atEnd()) {

        // Make sure we enter at least the first SignalGroup element. If we've gone past all the SignalGroups and are at the end of the document, just return.
        while (stream.name() != "SignalGroup") {
            QXmlStreamReader::TokenType token = stream.readNext();
            if (token == QXmlStreamReader::EndDocument) {
                return true;
            }
        }

        // Parse this SignalGroup element.
        QXmlStreamAttributes attributes = stream.attributes();

        // Try to find the SignalGroup's name as "Port " + prefix
        QString portName("");

        for (auto attribute : attributes) {
            if (attribute.name().toString().toLower() == "prefix") {
                portName = "Port " + attribute.value().toString();
                break;
            }
        }

        SignalGroup* thisSignalGroup = state->signalSources->groupByName(portName);
        // If the SignalGroup couldn't be found, just skip this element
        if (!thisSignalGroup) {
            stream.skipCurrentElement();
            continue;
        }

        // Iterate through all XML attributes
        for (auto attribute : attributes) {
            // Get the attribute name and vlaue from the XML.
            QString attributeName = attribute.name().toString();
            QString attributeValue = attribute.value().toString();

            // If attribute value is "N/A", then skip.
            if (attributeValue != "N/A") {
                // Try to find the attribute as a StateSingleItem in portItems.
                StateSingleItem *singleItem = state->locateStateSingleItem(thisSignalGroup->portItems, attributeName);

                // If the attribute is a StateSingleItem, set it according to XMLIncludeParameters.
                if (singleItem) {
                    bool changeItem = false;
                    XMLGroup xmlGroup = singleItem->getXMLGroup();

                    switch (includeParameters) {
                    case XMLIncludeSpikeSortingParameters:
                        if (xmlGroup == XMLGroupSpikeSettings) changeItem = true;
                        break;
                    case XMLIncludeGlobalParameters:
                        if (xmlGroup == XMLGroupSpikeSettings || xmlGroup == XMLGroupGeneral) changeItem = true;
                        break;
                    case XMLIncludeStimParameters:
                    case XMLIncludeProbeMapSettings:
                        // If we're inside a SignalGroup element, then we shouldn't have either case
                        // XMLIncludeStimParameters or XMLIncludeProbeMapSettings.
                        // If we somehow get here, throw an error and return
                        cerr << "XMLIncludeStimParameters or XMLIncludeProbeMapSettings somehow reached within SignalGroup element";
                        return false;
                    }

                    if (changeItem) {
                        if (!singleItem->setValue(attributeValue)) {
                            errorMessage.append("Error: Failed to parse " + singleItem->getParameterName());
                            return false;
                        } else {
                            continue;
                        }
                    }
                }
            }
        }

        // Parse Channels
        while (!stream.atEnd()) {

            // Make sure we enter at least the first Channel element. If we've gone past all the Channels and are at the end of the document, just return.
            while (stream.name() != "Channel") {
                QXmlStreamReader::TokenType token = stream.readNext();
                if (token == QXmlStreamReader::EndDocument) {
                    return true;
                }
            }

            // Parse this Channel element.
            QXmlStreamAttributes attributes = stream.attributes();

            // Try to find the Channel's name
            QString nativeChannelName("");
            for (auto attribute : attributes) {
                if (attribute.name().toString().toLower() == "nativechannelname") {
                    nativeChannelName = attribute.value().toString();
                    break;
                }
            }

            Channel * thisChannel = state->signalSources->channelByName(nativeChannelName);
            // If the Channel couldn't be found, just skip this element
            if (!thisChannel) {
                stream.skipCurrentElement();
                continue;
            }

            // Iterate through all XML attributes
            for (auto attribute : attributes) {
                // Get the attribute name and value from the XML.
                QString attributeName = attribute.name().toString();
                QString attributeValue = attribute.value().toString();

                // If attribute value is "N/A", then skip.
                if (attributeValue != "N/A") {
                    // Try to find the attribute as a StatesingleItem in channelItems.
                    StateSingleItem *singleItem = state->locateStateSingleItem(thisChannel->channelItems, attributeName);

                    // If the attribute is a StateSingleItem, set it according to XMLIncludeParameters.
                    if (singleItem) {
                        bool changeItem = false;
                        XMLGroup xmlGroup = singleItem->getXMLGroup();

                        switch (includeParameters) {
                        case XMLIncludeSpikeSortingParameters:
                            if (xmlGroup == XMLGroupSpikeSettings) changeItem = true;
                            break;
                        case XMLIncludeGlobalParameters:
                            if (xmlGroup == XMLGroupSpikeSettings || xmlGroup == XMLGroupGeneral) changeItem = true;
                            break;
                        case XMLIncludeStimParameters:
                        case XMLIncludeProbeMapSettings:
                            // If we're inside a Channel element, then we shouldn't have either case
                            // XMLIncludeStimParameters or XMLIncludeProbeMapSettings.
                            // If we somehow get here, throw an error and return
                            cerr << "XMLIncludeStimParameters or XMLIncludeProbeMapSettings somehow reached within Channel element";
                            return false;
                        }

                        if (changeItem) {
                            if (!singleItem->setValue(attributeValue)) {
                                errorMessage.append("Error: Failed to parse " + singleItem->getParameterName());
                                return false;
                            } else {
                                continue;
                            }
                        }
                    }
                }
            }

            // Skip this Channel element once we're done with it.
            stream.skipCurrentElement();
            stream.readNext();
        }

        // Skip this SignalGroup element once we're done with it.
        stream.skipCurrentElement();
        stream.readNext();
    }
    return true;
}

bool XMLInterface::parseStimParameters(const QByteArray &byteArray, QString &errorMessage) const
{
    QXmlStreamReader stream(byteArray);

    bool dummyIgnore = false;
    QString dummyError("");
    bool validDocumentStart = parseDocumentStart(byteArray, dummyError, dummyIgnore);
    if (!validDocumentStart) return false;

    // Parse StimParameters.
    while (stream.name() != "StimParameters") {
        stream.readNextStartElement();
        if (stream.atEnd()) return true;
    }

    // Parse StimChannels.
    while (!stream.atEnd()) {

        // Make sure we enter at least the first StimChannel element. If we've gone past all the StimChannels and are at the end of the document, just return.
        while (stream.name() != "StimChannel") {
            QXmlStreamReader::TokenType token = stream.readNext();
            if (token == QXmlStreamReader::EndDocument) {
                return true;
            }
        }

        // Parse this StimChannel element.
        QXmlStreamAttributes attributes = stream.attributes();

        // Try to find the StimChannel's name.
        QString nativeChannelName("");
        for (auto attribute : attributes) {
            if (attribute.name().toString().toLower() == "nativechannelname") {
                nativeChannelName = attribute.value().toString();
                break;
            }
        }

        Channel* thisChannel = state->signalSources->channelByName(nativeChannelName);
        // If the Channel couldn't be found, just skip this element.
        if (!thisChannel) {
            stream.skipCurrentElement();
            continue;
        }

        // Iterate through all XML attributes.
        for (auto attribute : attributes) {
            // Get the attribute name and value from XML.
            QString attributeName = attribute.name().toString();
            QString attributeValue = attribute.value().toString();

            // If attribute value is "N/A", then skip.
            if (attributeValue != "N/A") {
                // Try to find the attribute as a StateSingleItem in channelItems.
                StateSingleItem *singleItem = state->locateStateSingleItem(thisChannel->channelItems, attributeName);

                // If the attribute is a StateSingleItem, set it according to XMLIncludeParameters.
                if (singleItem) {
                    if (singleItem->getXMLGroup() == XMLGroupStimParameters) {
                        if (!singleItem->setValue(attributeValue)) {
                            errorMessage.append("Error: Failed to parse " + singleItem->getParameterName());
                            return false;
                        } else {
                            continue;
                        }
                    }
                }
            }
        }

        controllerInterface->uploadStimParameters(thisChannel);

        // Skip this StimChannel element once we're done with it.
        stream.skipCurrentElement();
        stream.readNext();
    }

    return true;
}

bool XMLInterface::parseStimLegacy(const QByteArray &byteArray, QString &errorMessage) const
{
    QXmlStreamReader stream(byteArray);
    if (!stream.readNextStartElement() || stream.name() != "xstim" || stream.attributes().value("", "version").toString() != "1.0") {
        errorMessage.append("Error: invalid xstim element and version attribute");
        return false;
    }

    Channel *channel;

    state->holdUpdate();

    while (stream.readNextStartElement() && stream.name() == "channel") {

        QString channelName = stream.attributes().value("", "name").toString();
        channel = state->signalSources->channelByName(channelName);
        StimParameters *parameters = channel->stimParameters;
        if (!channel) {
            qDebug() << "Error: channel not found: " << channelName;
        } else {
            if (!readElementLegacy(stream, parameters->enabled, "enabled")) {
                errorMessage.append("Error: enabled not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->triggerSource, "triggerSource")) {
                errorMessage.append("Error: trigger source not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->triggerEdgeOrLevel, "triggerEdgeOrLevel")) {
                errorMessage.append("Error: trigger edge or level not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->triggerHighOrLow, "triggerHighOrLow")) {
                errorMessage.append("Error: trigger high or low not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->postTriggerDelay, "postTriggerDelay")) {
                errorMessage.append("Error: post trigger delay not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->pulseOrTrain, "pulseOrTrain")) {
                errorMessage.append("Error: pulse or train not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->numberOfStimPulses, "numberOfStimPulses")) {
                errorMessage.append("Error: number of stim pulses not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->pulseTrainPeriod, "pulseTrainPeriod")) {
                errorMessage.append("Error: pulse train period not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->refractoryPeriod, "refractoryPeriod")) {
                errorMessage.append("Error: refractory period not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->stimShape, "stimShape")) {
                errorMessage.append("Error: stim shape not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->stimPolarity, "stimPolarity")) {
                errorMessage.append("Error: stim polarity not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->firstPhaseDuration, "firstPhaseDuration")) {
                errorMessage.append("Error: first phase duration not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->secondPhaseDuration, "secondPhaseDuration")) {
                errorMessage.append("Error: second phase duration not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->interphaseDelay, "interphaseDelay")) {
                errorMessage.append("Error: interphase delay not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->firstPhaseAmplitude, "firstPhaseAmplitude")) {
                errorMessage.append("Error: first phase amplitude not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->secondPhaseAmplitude, "secondPhaseAmplitude")) {
                errorMessage.append("Error: second phase amplitude not found");
                state->releaseUpdate();
                return false;
            }
            //if (!readElementLegacy(stream, parameters->baselineVoltage, "baselineVoltage"))  return false;
            if (!readElementLegacy(stream, parameters->enableAmpSettle, "enableAmpSettle")) {
                errorMessage.append("Error: enable amp settle not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->preStimAmpSettle, "preStimAmpSettle")) {
                errorMessage.append("Error: pre stim amp settle not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->postStimAmpSettle, "postStimAmpSettle")) {
                errorMessage.append("Error: post stim amp settle not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->maintainAmpSettle, "maintainAmpSettle")) {
                errorMessage.append("Error: maintain amp settle not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->enableChargeRecovery, "enableChargeRecovery")) {
                errorMessage.append("Error: enable charge recovery not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->postStimChargeRecovOn, "postStimChargeRecovOn")) {
                errorMessage.append("Error: post stim charge recov on not found");
                state->releaseUpdate();
                return false;
            }
            if (!readElementLegacy(stream, parameters->postStimChargeRecovOff, "postStimChargeRecovOff")) {
                errorMessage.append("Error: post stim charge recov off not found");
                state->releaseUpdate();
                return false;
            }
        }

        stream.skipCurrentElement();

        controllerInterface->uploadStimParameters(channel);
    }

    state->releaseUpdate();

    return true;
}

bool XMLInterface::parseProbeMapSettingsDOM(const QByteArray &byteArray, QString &errorMessage) const
{
    QDomDocument doc("XMLSettings");
    if (!doc.setContent(byteArray)) {
        errorMessage.append("Error: XML file not read as DOM properly - check for invalid syntax");
        return false;
    }

    QDomElement docElem = doc.documentElement().firstChildElement("ProbeMapSettings");
    if (docElem.isNull()) {
        return true;
    }

    state->clearProbeMapSettings();

    state->probeMapSettings.backgroundColor = docElem.attribute("backgroundColor", "Black");
    state->probeMapSettings.siteOutlineColor = docElem.attribute("siteOutlineColor", "Gray");
    state->probeMapSettings.lineColor = docElem.attribute("lineColor", "White");
    state->probeMapSettings.fontHeight = docElem.attribute("fontHeight", "10").toFloat();
    state->probeMapSettings.fontColor = docElem.attribute("fontColor", "White");
    state->probeMapSettings.textAlignment = docElem.attribute("textAlignment", "BottomLeft");
    state->probeMapSettings.siteShape = docElem.attribute("siteShape", "Rectangle");
    state->probeMapSettings.siteWidth = docElem.attribute("siteWidth", "5").toFloat();
    state->probeMapSettings.siteHeight = docElem.attribute("siteHeight", "5").toFloat();

    QDomNodeList pagesList = docElem.elementsByTagName("Page");
    for (int page = 0; page < pagesList.length(); ++page) {
        Page currentPage;
        currentPage.name = pagesList.at(page).toElement().attribute("name", QString::number(page + 1));
        currentPage.backgroundColor = pagesList.at(page).toElement().attribute("backgroundColor", "");
        currentPage.siteOutlineColor = pagesList.at(page).toElement().attribute("siteOutlineColor", "");
        currentPage.lineColor = pagesList.at(page).toElement().attribute("lineColor", "");
        currentPage.fontHeight = pagesList.at(page).toElement().attribute("fontHeight", "").toFloat();
        currentPage.fontColor = pagesList.at(page).toElement().attribute("fontColor", "");
        currentPage.textAlignment = pagesList.at(page).toElement().attribute("textAlignment", "");
        currentPage.siteShape = pagesList.at(page).toElement().attribute("siteShape", "");
        currentPage.siteWidth = pagesList.at(page).toElement().attribute("siteWidth", "").toFloat();
        currentPage.siteHeight = pagesList.at(page).toElement().attribute("siteHeight", "").toFloat();
        QDomNodeList portsList = pagesList.at(page).toElement().elementsByTagName("Port");
        for (int port = 0; port < portsList.length(); ++port) {
            Port currentPort;
            currentPort.name = portsList.at(port).toElement().attribute("name", "");
            currentPort.siteOutlineColor = portsList.at(port).toElement().attribute("siteOutlineColor", "");
            currentPort.siteShape = portsList.at(port).toElement().attribute("siteShape", "");
            currentPort.siteWidth = portsList.at(port).toElement().attribute("siteWidth", "").toFloat();
            currentPort.siteHeight = portsList.at(port).toElement().attribute("siteHeight", "").toFloat();
            QDomNodeList electrodeSiteList = portsList.at(port).toElement().elementsByTagName("ElectrodeSite");
            for (int electrodeSite = 0; electrodeSite < electrodeSiteList.length(); electrodeSite++) {
                ElectrodeSite currentElectrodeSite;
                currentElectrodeSite.channelNumber = electrodeSiteList.at(electrodeSite).toElement().attribute("channelNumber", "").toInt();
                currentElectrodeSite.x = electrodeSiteList.at(electrodeSite).toElement().attribute("x", "0").toFloat();
                currentElectrodeSite.y = electrodeSiteList.at(electrodeSite).toElement().attribute("y", "0").toFloat();
                currentElectrodeSite.nativeName = currentPort.name + QString("-%1").arg(currentElectrodeSite.channelNumber, 3, 10, QLatin1Char('0'));
                currentElectrodeSite.siteOutlineColor = electrodeSiteList.at(electrodeSite).toElement().attribute("siteOutlineColor", "");
                currentElectrodeSite.siteShape = electrodeSiteList.at(electrodeSite).toElement().attribute("siteShape", "");
                currentElectrodeSite.siteWidth = electrodeSiteList.at(electrodeSite).toElement().attribute("siteWidth", "").toFloat();
                currentElectrodeSite.siteHeight = electrodeSiteList.at(electrodeSite).toElement().attribute("siteHeight", "").toFloat();
                currentElectrodeSite.color = "";
                currentElectrodeSite.highlighted = false;
                currentElectrodeSite.enabled = false;
                currentElectrodeSite.linked = false;
                currentPort.electrodeSites.append(currentElectrodeSite);
            }
            currentPage.ports.append(currentPort);
        }
        QDomNodeList linesList = pagesList.at(page).toElement().elementsByTagName("Line");
        for (int line = 0; line < linesList.length(); line++) {
            Line currentLine;
            currentLine.lineColor = linesList.at(line).toElement().attribute("lineColor", "");
            currentLine.x1 = linesList.at(line).toElement().attribute("x1", "").toFloat();
            currentLine.y1 = linesList.at(line).toElement().attribute("y1", "").toFloat();
            currentLine.x2 = linesList.at(line).toElement().attribute("x2", "").toFloat();
            currentLine.y2 = linesList.at(line).toElement().attribute("y2", "").toFloat();
            currentPage.lines.append(currentLine);
        }
        QDomNodeList textsList = pagesList.at(page).toElement().elementsByTagName("Text");
        for (int text = 0; text < textsList.length(); text++) {
            Text currentText;
            currentText.x = textsList.at(text).toElement().attribute("x", "").toFloat();
            currentText.y = textsList.at(text).toElement().attribute("y", "").toFloat();
            currentText.fontHeight = textsList.at(text).toElement().attribute("fontHeight", "").toFloat();
            currentText.fontColor = textsList.at(text).toElement().attribute("fontColor", "");
            currentText.textAlignment = textsList.at(text).toElement().attribute("textAlignment", "");
            currentText.rotation = textsList.at(text).toElement().attribute("rotation", "0").toFloat();
            currentText.text = textsList.at(text).toElement().attribute("text", "");
            currentPage.texts.append(currentText);
        }
        state->probeMapSettings.pages.append(currentPage);
    }
    return true;
}

bool XMLInterface::readElementLegacy(QXmlStreamReader &stream, StateSingleItem *item, QString legacyName) const
{
    if (!stream.readNextStartElement() || stream.name() != legacyName) return false;

    BooleanItem* itemBoolean = dynamic_cast<BooleanItem*>(item);
    if (itemBoolean) {
        itemBoolean->setValue((bool) stream.readElementText().toInt());
        return true;
    }

    IntRangeItem* itemIntRange = dynamic_cast<IntRangeItem*>(item);
    if (itemIntRange) {
        itemIntRange->setValue(stream.readElementText().toInt());
        return true;
    }

    DiscreteItemList *itemDiscrete = dynamic_cast<DiscreteItemList*>(item);
    if (itemDiscrete) {
        itemDiscrete->setIndex(stream.readElementText().toInt());
        return true;
    }

    DoubleRangeItem* itemDoubleRange = dynamic_cast<DoubleRangeItem*>(item);
    if (itemDoubleRange) {
        itemDoubleRange->setValue(stream.readElementText().toDouble());
        return true;
    }

    return false;
}

QByteArray XMLInterface::saveByteArray() const
{
    QByteArray result;
    QXmlStreamWriter stream(&result);

    writeGlobalDocStart(stream);
    saveAsElement(stream);
    writeGlobalDocEnd(stream);

    return result;
}

void XMLInterface::writeGlobalDocStart(QXmlStreamWriter &stream) const
{
    stream.setAutoFormatting(true);
    stream.writeStartDocument();
    stream.writeStartElement(ApplicationName);

    // Write hard-wired StateItems as attributes of the IntanRHX element: SampleRate, Type, StimStepSize (if Stim), Version
    QVector<StateSingleItem*> headerStateItems = state->getHeaderStateItems();
    for (int i = 0; i < headerStateItems.size(); i++) {
        stream.writeAttribute(headerStateItems[i]->getParameterName(), headerStateItems[i]->getValueString());
    }
}

void XMLInterface::writeGlobalDocEnd(QXmlStreamWriter &stream) const
{
    stream.writeEndElement(); // end SoftwareName
    stream.writeEndDocument();
}

void XMLInterface::setOptionalAttribute(QXmlStreamReader &stream, const QString &attributeName, QString &destination, const QString &defaultValue) const
{
    if (stream.attributes().value("", attributeName).isEmpty())
        destination = defaultValue;
    else
        destination = stream.attributes().value("", attributeName).toString();
}

void XMLInterface::setOptionalAttribute(QXmlStreamReader &stream, const QString &attributeName, float &destination, const float defaultValue) const
{
    if (stream.attributes().value("", attributeName).isEmpty())
        destination = defaultValue;
    else
        destination = stream.attributes().value("", attributeName).toFloat();
}

bool XMLInterface::setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, QString &destination) const
{
    if (stream.attributes().value("", attributeName).isEmpty()) {
        qDebug() << "Error: " << attributeName << " not specified";
        return false;
    }

    destination = stream.attributes().value("", attributeName).toString();
    return true;
}

bool XMLInterface::setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, int &destination) const
{
    if (stream.attributes().value("", attributeName).isEmpty()) {
        qDebug() << "Error: " << attributeName << " not specified";
        return false;
    }

    destination = stream.attributes().value("", attributeName).toInt();
    return true;
}

bool XMLInterface::setMandatoryAttribute(QXmlStreamReader &stream, const QString &attributeName, float &destination) const
{
    if (stream.attributes().value("", attributeName).isEmpty()) {
        qDebug() << "Error: " << attributeName << " not specified";
        return false;
    }

    destination = stream.attributes().value("", attributeName).toFloat();
    return true;
}
