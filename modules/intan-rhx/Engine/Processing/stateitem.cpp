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

#include "stateitem.h"
#include "systemstate.h"
#include "signalsources.h"

StateItem::StateItem(const QString &parameterName_, SystemState *state_, XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    parameterName(parameterName_),
    state(state_),
    xmlGroup(xmlGroup_),
    typeDependency(typeDependency_),
    restricted(false),
    restrictFunction(nullptr)
{
    // If this item's XMLGroup is ReadOnly, that indicates it shouldn't be changed.
    // Restrict it here so we don't need to restrict the individual variables after they're created.
    if (xmlGroup_ == XMLGroupReadOnly) {
        setRestricted(RestrictAlways, ReadOnlyErrorMessage);
    }

    // If this item is type-dependent on Stim, that indicates it shouldn't be changed if not Stim.
    // Restrict it here so we don't need to restrict the individual variables after they're created.
    if (typeDependency_ == TypeDependencyStim) {
        setRestricted(RestrictIfNotStimController, NonStimErrorMessage);
    }

    // If this item is type-dependent on not being Stim, that indicates it shouldn't be changed if Stim.
    // Restrict it here so we don't need to restrict the individual variables after they're created.
    if (typeDependency_ == TypeDependencyNonStim) {
        setRestricted(RestrictIfStimController, StimErrorMessage);
    }
}


StateSingleItem::StateSingleItem(const QString &parameterName_, SingleItemList &hList_, SystemState* state_,
                                 XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateItem(parameterName_, state_, xmlGroup_, typeDependency_),
    hList(hList_)
{
    state->addStateSingleItem(hList_, this);
}

StateSingleItem::~StateSingleItem()
{
}


ChannelNameItem::ChannelNameItem(const QString& parameterName_, SingleItemList &hList_, SystemState* state_,
                                 const QString &defaultValue, XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    currentValue(defaultValue)
{
}

int ChannelNameItem::getSerialIndex() const
{
    return state->getSerialIndex(getValue());
}

bool ChannelNameItem::setValue(const QString& value)
{
    // Check if 'value' has a match in signal sources.
    for (auto& channelName : state->signalSources->amplifierChannelsNameList()) {
        if (QString::fromStdString(channelName).toLower() == value.toLower()) {
            currentValue = QString::fromStdString(channelName);
            state->forceUpdate();
            return true;
        }
    }
    return false;
}


StateFilenameItem::StateFilenameItem(const QString &parameterName_, FilenameItemList *hList_, SystemState *state_,
                                     const QString &defaultPath, const QString &defaultBaseFilename,
                                     XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateItem(parameterName_, state_, xmlGroup_, typeDependency_),
    path(defaultPath),
    baseFilename(defaultBaseFilename)
{
    state->addStateFilenameItem(*hList_, this);
}

StateFilenameItem::~StateFilenameItem()
{
}

void StateFilenameItem::setBaseFilename(const QString& baseFilename_)
{
    baseFilename = baseFilename_;
    state->forceUpdate();
}

void StateFilenameItem::setPath(const QString& path_)
{
    path = path_;
    state->forceUpdate();
}


BooleanItem::BooleanItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, bool defaultState,
                         XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    currentValue(defaultState)
{
}

void BooleanItem::setValue(bool value)
{
    if (currentValue != value) {
        currentValue = value;
        state->forceUpdate();
    }
}

bool BooleanItem::setValue(const QString& value)
{
    if (value.toLower() != "true" && value.toLower() != "false")
        return false;
    setValue(value.toLower() == "true");
    return true;
}

void BooleanItem::toggleValue()
{
    currentValue = !currentValue;
    state->forceUpdate();
}


StringItem::StringItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_,
                       const QString &defaultValue, XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    currentValue(defaultValue)
{
}

bool StringItem::setValue(const QString &value) {
    if (currentValue != value) {
        currentValue = value;
        state->forceUpdate();
    }
    return true;
}


DiscreteItemList::DiscreteItemList(const QString &parameterName_, SingleItemList &hList_, SystemState *state_,
                                   XMLGroup xmlGroup_, TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    currentIndex(0)
{
}

int DiscreteItemList::addItem(const QString &valueName, const QString &displayedValueName, double numericalValue)
{
    vector<QString> valueNames;
    valueNames.push_back(valueName);
    DiscreteItem item = { valueNames, displayedValueName, numericalValue };
    items.push_back(item);
    return (int) (items.size()) - 1;    // Return index to new item.
}

int DiscreteItemList::getIndex(const QString &valueName) const
{
    int size = (int) items.size();
    for (int i = 0; i < size; ++i) {
        for (auto& name : items[i].valueNames) {
            if (name.toLower() == valueName.toLower()) {
                return i;
            }
        }
        if (items[i].displayedValueName == valueName) {
            return i;
        }
    }
    return -1;
}

bool DiscreteItemList::addAlternateValueName(const QString &valueName, const QString &alternateValueName)
{
    int index = getIndex(valueName);
    if (index < 0) return false;
    items[index].valueNames.push_back(alternateValueName);
    return true;
}

QString DiscreteItemList::getValue(int index) const
{
    if (index < 0 || index >= (int) items.size()) {
        return QString("");
    }
    return items[index].valueNames[0];
}

QString DiscreteItemList::getDisplayValueString(int index) const
{
    if (index < 0 || index >= (int) items.size()) {
        return QString("");
    }
    return items[index].displayedValueName;
}

double DiscreteItemList::getNumericValue(int index) const
{
    if (index < 0 || index >= (int) items.size()) {
        return 0.0;
    }
    return items[index].numericValue;
}

bool DiscreteItemList::setIndex(int index)
{
    if (index < 0 || index >= (int) items.size()) return false;
    if (currentIndex == index) return true;
    currentIndex = index;
    state->forceUpdate();
    return true;
}

bool DiscreteItemList::setValue(const QString &valueName)
{
    int index = getIndex(valueName);
    if (index < 0) return false;
    if (currentIndex == index) return true;
    currentIndex = index;
    state->forceUpdate();
    return true;
}

bool DiscreteItemList::incrementIndex()
{
    if (currentIndex < (int) items.size() - 1) {
        ++currentIndex;
        state->forceUpdate();
        return true;
    } else {
        return false;
    }
}

bool DiscreteItemList::decrementIndex()
{
    if (currentIndex > 0) {
        --currentIndex;
        state->forceUpdate();
        return true;
    } else {
        return false;
    }
}

int DiscreteItemList::shiftIndex(int delta)
{
    int newIndex = currentIndex + delta;
    if (newIndex < 0) {
        newIndex = 0;
    } else if (newIndex >= numberOfItems()) {
        newIndex = numberOfItems() - 1;
    }
    int actualChange = newIndex - currentIndex;
    setIndex(newIndex);  // Let setCurrentIndex() determine if state needs to signal that it has changed.
    return actualChange;
}

QString DiscreteItemList::getValidValues() const
{
    QString validValues;
    for (auto& item : items) {
        for (auto& name : item.valueNames) {
            validValues.append(name + ";");
        }
    }
    return validValues;
}

void DiscreteItemList::setupComboBox(QComboBox* comboBox) const
{
   for (auto& item : items) {
       comboBox->addItem(item.displayedValueName);
   }
   comboBox->setCurrentIndex(currentIndex);
}


DoubleRangeItem::DoubleRangeItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_,
                                 double minValue_, double maxValue_, double defaultValue_, XMLGroup xmlGroup_,
                                 TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    minValue(minValue_),
    maxValue(maxValue_),
    currentValue(defaultValue_)
{
}

void DoubleRangeItem::setMinValue(double value)
{
    minValue = value;
    if (minValue > currentValue)
        setValue(value);
}

void DoubleRangeItem::setMaxValue(double value)
{
    maxValue = value;
    if (maxValue < currentValue)
        setValue(value);
}

bool DoubleRangeItem::setValue(double value)
{
    if (value < minValue || value > maxValue)
        return false;
    if (currentValue == value) return true;
    currentValue = value;
    state->forceUpdate();
    return true;
}

void DoubleRangeItem::setValueWithLimits(double value)
{
    if (currentValue == value) return;
    if (value < minValue) {
        currentValue = minValue;
    } else if (value > maxValue) {
        currentValue = maxValue;
    } else {
        currentValue = value;
    }
    state->forceUpdate();
}

bool DoubleRangeItem::setValue(const QString& value)
{
    bool ok = false;
    double valueDouble = value.toDouble(&ok);
    if (!ok)
        return false;
    return setValue(valueDouble);
}

QString DoubleRangeItem::getValidValues() const
{
    return "[ " + QString::number(minValue) + " , " + QString::number(maxValue) + " ]";
}

void DoubleRangeItem::setupSpinBox(QDoubleSpinBox *spinBox) const
{
    spinBox->setMinimum(minValue);
    spinBox->setMaximum(maxValue);
    spinBox->setValue(currentValue);
}


IntRangeItem::IntRangeItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_,
                           int minValue_, int maxValue_, int defaultValue_, XMLGroup xmlGroup_,
                           TypeDependency typeDependency_) :
    StateSingleItem(parameterName_, hList_, state_, xmlGroup_, typeDependency_),
    minValue(minValue_),
    maxValue(maxValue_),
    currentValue(defaultValue_)
{
}

void IntRangeItem::setMinValue(int value)
{
    minValue = value;
    if (minValue > currentValue)
        setValue(value);
}

void IntRangeItem::setMaxValue(int value)
{
    maxValue = value;
    if (maxValue < currentValue)
        setValue(value);
}

bool IntRangeItem::setValue(int value)
{
    if (value < minValue || value > maxValue)
        return false;
    if (currentValue == value) return true;
    currentValue = value;
    state->forceUpdate();
    return true;
}

void IntRangeItem::setValueWithLimits(int value)
{
    if (currentValue == value) return;
    if (value < minValue) {
        currentValue = minValue;
    } else if (value > maxValue) {
        currentValue = maxValue;
    } else {
        currentValue = value;
    }
    state->forceUpdate();
}

bool IntRangeItem::setValue(const QString &value)
{
    bool ok = false;
    int valueInt = value.toInt(&ok);
    if (!ok)
        return false;
    return setValue(valueInt);
}

QString IntRangeItem::getValidValues() const
{
    return "[ " + QString::number(minValue) + " , " + QString::number(maxValue) + " ]";
}

void IntRangeItem::setupSpinBox(QSpinBox *spinBox) const
{
    spinBox->setMinimum(minValue);
    spinBox->setMaximum(maxValue);
    spinBox->setValue(currentValue);
}
