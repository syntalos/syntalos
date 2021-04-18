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

#ifndef STATEITEM_H
#define STATEITEM_H

#include <vector>
#include <QString>
#include <map>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class SystemState;

using namespace std;

const QString GlobalLevel = "Global";

enum XMLGroup {
    XMLGroupNone,
    XMLGroupReadOnly,
    XMLGroupGeneral,
    XMLGroupSpikeSettings,
    XMLGroupStimParameters
};

enum TypeDependency {
    TypeDependencyNone,
    TypeDependencyStim,
    TypeDependencyNonStim
};

class StateSingleItem;
class StateFilenameItem;

typedef map<string, StateSingleItem*> SingleItemList;
typedef map<string, StateFilenameItem*> FilenameItemList;

class StateItem
{
public:
    StateItem(const QString &parameterName_, SystemState* state_, XMLGroup xmlGroup_, TypeDependency typeDependency_);

    QString getParameterName() const { return parameterName; }
    XMLGroup getXMLGroup() const { return xmlGroup; }
    TypeDependency getTypeDependency() const { return typeDependency; }

    void setRestricted(bool (*restrictFunction_)(const SystemState*), const QString &restrictErrorMessage_)
        { restricted = true; restrictFunction = restrictFunction_; restrictErrorMessage = restrictErrorMessage_; }
    bool isRestricted() const { return restricted ? restrictFunction(state) : false; }
    QString getRestrictErrorMessage() const { return parameterName + " " + restrictErrorMessage; }

    virtual QString getValidValues() const = 0;

protected:
    QString parameterName;
    SystemState* state;
    XMLGroup xmlGroup;
    TypeDependency typeDependency;
    bool restricted;
    bool (*restrictFunction)(const SystemState*);
    QString restrictErrorMessage;
};

class StateSingleItem : public StateItem
{
public:
    StateSingleItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, XMLGroup xmlGroup_, TypeDependency typeDependency_);
    virtual ~StateSingleItem();

    virtual QString getValueString() const = 0;
    virtual QString getDisplayValueString() const { return getValueString(); }
    virtual bool setValue(const QString& value) = 0;

protected:
    SingleItemList hList;
};


class ChannelNameItem : public StateSingleItem
{
public:
    ChannelNameItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, const QString &defaultValue,
                    XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);

    QString getValueString() const override { return currentValue; }
    inline QString getValue() const { return currentValue; }
    int getSerialIndex() const;

    bool setValue(const QString& value) override;

    QString getValidValues() const override { return "[PortNumber]-[ChannelNumber], for example A-023"; }

private:
    QString currentValue;
};

class StateFilenameItem : public StateItem
{
public:
    StateFilenameItem(const QString &parameterName_, FilenameItemList *hList_, SystemState *state_, const QString &defaultPath = "",
                      const QString &defaultBaseFilename = "", XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);
    virtual ~StateFilenameItem();

    QString getPathParameterName() const { return "Path"; }
    QString getBaseFilenameParameterName() const { return "BaseFilename"; }

    QString getBaseFilename() const { return baseFilename; }
    QString getPath() const { return path; }

    void setBaseFilename(const QString& baseFilename_);
    void setPath(const QString& path_);

    bool isValid() const { return !path.isEmpty() && !baseFilename.isEmpty(); }
    QString getFullFilename() const { return isValid() ? path + "/" + baseFilename : ""; }

    QString getValidValues() const { return "Path: [path/to/file], BaseFilename: [filename.rhx]"; }

private:
    QString path;
    QString baseFilename;
};

class BooleanItem : public StateSingleItem
{
public:
    BooleanItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, bool defaultState = false, XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);

    QString getValueString() const override { return currentValue ? "True" : "False"; }
    inline bool getValue() const { return currentValue; }

    void setValue(bool value);
    bool setValue(const QString& value) override;
    void toggleValue();

    QString getValidValues() const override { return "True;False"; }

private:
    bool currentValue;
};

class StringItem : public StateSingleItem
{
public:
    StringItem(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, const QString &defaultValue, XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);

    QString getValueString() const override { return currentValue; }
    bool setValue(const QString &value) override;

    QString getValidValues() const override { return "Any String"; }

private:
    QString currentValue;
};


class DiscreteItemList : public StateSingleItem
{
public:
    struct DiscreteItem {
        vector<QString> valueNames;  // e.g. { "1.0", "1" }
        QString displayedValueName;  // e.g. "1.0 mV"
        double numericValue;
    };

    DiscreteItemList(const QString &parameterName_, SingleItemList &hList_, SystemState *state_, XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);
    int addItem(const QString &valueName, const QString &displayedValueName, double numericalValue = 0.0);
    bool addAlternateValueName(const QString& valueName, const QString& alternateValueName);

    int numberOfItems() const { return (int) items.size(); }

    QString getValueString() const override { return getValue(currentIndex); }
    QString getValue(int index) const;
    inline QString getValue() const { return getValue(currentIndex); }
    QString getDisplayValueString(int index) const;
    QString getDisplayValueString() const override { return getDisplayValueString(currentIndex); }
    double getNumericValue(int index) const;
    double getNumericValue() const { return getNumericValue(currentIndex); }
    int getIndex(const QString& valueName) const;
    int getIndex() const { return currentIndex; }

    bool setValue(const QString& valueName) override;
    bool setIndex(int index);
    bool incrementIndex();
    bool decrementIndex();
    int shiftIndex(int delta);

    QString getValidValues() const override;
    void setupComboBox(QComboBox* comboBox) const;

private:
    vector<DiscreteItem> items;
    int currentIndex;
};

class DoubleRangeItem : public StateSingleItem
{
public:
    DoubleRangeItem(const QString& parameterName_, SingleItemList &hList_, SystemState* state_, double minValue_ = 0.0, double maxValue_ = 1.0,
                    double defaultValue_ = 0.0, XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);
    void setMinValue(double value);
    void setMaxValue(double value);

    bool setValue(double value);
    bool setValue(const QString& value) override;
    void setValueWithLimits(double value);
    void setToMin() { setValue(minValue); }
    void setToMax() { setValue(maxValue); }

    QString getValueString() const override { return QString::number(currentValue); }
    double getMinValue() const { return minValue; }
    double getMaxValue() const { return maxValue; }
    inline double getValue() const { return currentValue; }

    QString getValidValues() const override;
    void setupSpinBox(QDoubleSpinBox* spinBox) const;

private:
    double minValue;
    double maxValue;
    double currentValue;
};

class IntRangeItem : public StateSingleItem
{
public:
    IntRangeItem(const QString& parameterName_, SingleItemList &hList_, SystemState* state_, int minValue_ = 0, int maxValue_ = 1, int defaultValue_ = 0,
                 XMLGroup xmlGroup_ = XMLGroupGeneral, TypeDependency typeDependency_ = TypeDependencyNone);
    void setMinValue(int value);
    void setMaxValue(int value);

    bool setValue(int value);
    bool setValue(const QString& value) override;
    void setValueWithLimits(int value);
    void setToMin() { setValue(minValue); }
    void setToMax() { setValue(maxValue); }

    QString getValueString() const override { return QString::number(currentValue); }
    int getMinValue() const { return minValue; }
    int getMaxValue() const { return maxValue; }
    inline int getValue() const { return currentValue; }

    QString getValidValues() const override;
    void setupSpinBox(QSpinBox* spinBox) const;

private:
    int minValue;
    int maxValue;
    int currentValue;
};

#endif // STATEITEM_H
