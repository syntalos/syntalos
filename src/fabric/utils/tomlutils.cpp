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

#include "tomlutils.h"

#include <QDebug>
#include <QTimeZone>
#include <fstream>
#include <iostream>

toml::time qTimeToToml(const QTime &qtime)
{
    toml::time ttime;

    ttime.hour = qtime.hour();
    ttime.minute = qtime.minute();
    ttime.second = qtime.second();
    ttime.nanosecond = qtime.msec() * 1000;
    return ttime;
}

toml::date qDateToToml(const QDate &qdate)
{
    toml::date tdate;

    tdate.year = qdate.year();
    tdate.month = qdate.month();
    tdate.day = qdate.day();
    return tdate;
}

toml::date_time qDateTimeToToml(const QDateTime &qdt)
{
    toml::date_time tomlDt;
    tomlDt.date = qDateToToml(qdt.date());
    tomlDt.time = qTimeToToml(qdt.time());

    toml::time_offset offset;
    offset.minutes = qdt.offsetFromUtc() / 60;
    tomlDt.offset = offset;

    return tomlDt;
}

// awful preprocesser macro, since I don't know a good template-based
// way to do this with the current version of TOML++ that needs to know types
// immediately for template instanciation and can't infer anything at runtime
// This macro needs a GCC-compliant compiler (GCC and Clang will work)
#define CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(func, var)                                        \
    ({                                                                                     \
        bool __success = true;                                                             \
        if ((var).typeId() == QMetaType::Bool) {                                           \
            func((var).toBool());                                                          \
        }                                                                                  \
                                                                                           \
        else if ((var).isNull()) {                                                         \
            /* leave out the value */                                                      \
        }                                                                                  \
                                                                                           \
        else if ((var).typeId() == QMetaType::QString) {                                   \
            func((var).toString().toStdString());                                          \
        }                                                                                  \
                                                                                           \
        else if ((var).typeId() == QMetaType::Int) {                                       \
            func((var).toInt());                                                           \
        }                                                                                  \
                                                                                           \
        else if ((var).typeId() == QMetaType::Double) {                                    \
            func((var).toDouble());                                                        \
        }                                                                                  \
                                                                                           \
        else if ((var).canConvert<int64_t>()) {                                            \
            func((var).value<int64_t>());                                                  \
        }                                                                                  \
                                                                                           \
        else if ((var).canConvert<QDateTime>()) {                                          \
            func(qDateTimeToToml((var).toDateTime()));                                     \
        }                                                                                  \
                                                                                           \
        else if ((var).typeId() == QMetaType::QTime) {                                     \
            func(qTimeToToml((var).toTime()));                                             \
        }                                                                                  \
                                                                                           \
        else if ((var).typeId() == QMetaType::QDate) {                                     \
            func(qDateToToml((var).toDate()));                                             \
        }                                                                                  \
                                                                                           \
        /* check Qt knows how to convert the unknown value  to a string representation. */ \
        else if ((var).canConvert<QString>()) {                                            \
            func((var).toString().toStdString());                                          \
        }                                                                                  \
                                                                                           \
        else {                                                                             \
            /* unable to convert this value */                                             \
            __success = false;                                                             \
        }                                                                                  \
        __success;                                                                         \
    })

toml::array qVariantListToTomlArray(const QVariantList &varList)
{
    toml::array arr;
    for (const auto &var : varList) {
        if (var.canConvert<QVariantHash>()) {
            auto subTab = qVariantHashToTomlTable(var.toHash());
            arr.push_back(std::move(subTab));
            continue;
        }

        if (var.typeId() != QMetaType::QString && var.canConvert<QVariantList>()) {
            arr.push_back(qVariantListToTomlArray(var.toList()));
            continue;
        }

        if (CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(arr.push_back, var))
            continue;

        qWarning().noquote()
            << QStringLiteral("Unable to store type `%1` in TOML attributes (array).").arg(var.typeName());
        arr.push_back("�");
    }

    return arr;
}

toml::table qVariantHashToTomlTable(const QVariantHash &varHash)
{
    toml::table tab;

    QHashIterator<QString, QVariant> i(varHash);
    while (i.hasNext()) {
        i.next();
        auto var = i.value();
        const auto key = i.key().toStdString();

        if (var.canConvert<QVariantHash>()) {
            auto subTab = qVariantHashToTomlTable(var.toHash());
            tab.insert(key, std::move(subTab));
            continue;
        }

        if (var.typeId() != QMetaType::QString && var.canConvert<QVariantList>()) {
            tab.insert(key, qVariantListToTomlArray(var.toList()));
            continue;
        }

        auto tabInsertFunc = [&](auto v) {
            tab.insert(key, v);
        };
        if (CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(tabInsertFunc, var))
            continue;

        qWarning().noquote()
            << QStringLiteral("Unable to store type `%1` in TOML attributes (table).").arg(var.typeName());
        tab.insert(key, "�");
    }

    return tab;
}

QString serializeTomlTable(const toml::table &tab)
{
    std::stringstream data;
    data << tab;
    return QString::fromStdString(data.str());
}

QByteArray qVariantHashToTomlData(const QVariantHash &varHash)
{
    const auto tab = qVariantHashToTomlTable(varHash);
    const auto result = serializeTomlTable(tab) + "\n";
    return result.toUtf8();
}

static QTime tomlTimeToQ(const toml::time &ttime)
{
    return QTime(ttime.hour, ttime.minute, ttime.second, ttime.nanosecond / 1000);
}

static QDate tomlDateToQ(const toml::date &tdate)
{
    return QDate(tdate.year, tdate.month, tdate.day);
}

static QDateTime tomlDateTimeToQ(const toml::date_time &tdt)
{
    QDateTime qdt(tomlDateToQ(tdt.date), tomlTimeToQ(tdt.time));
    qdt.setTimeZone(QTimeZone::fromSecondsAheadOfUtc(tdt.offset->minutes * 60));
    return qdt;
}

template<typename T>
QVariant tomlValueToVariant(const T &value)
{
    QVariant res;
    value.visit([&](auto &&n) {
        if constexpr (toml::is_string<decltype(n)>)
            res = QVariant::fromValue(QString::fromStdString(n.as_string()->get()));
        else if constexpr (toml::is_integer<decltype(n)>)
            res = QVariant::fromValue(value.as_integer()->get());
        else if constexpr (toml::is_floating_point<decltype(n)>)
            res = QVariant::fromValue(n.as_floating_point()->get());
        else if constexpr (toml::is_boolean<decltype(n)>)
            res = QVariant::fromValue(n.as_boolean()->get());

        else if constexpr (toml::is_date<decltype(n)>)
            res = QVariant::fromValue(tomlDateToQ(n.as_date()->get()));
        else if constexpr (toml::is_time<decltype(n)>)
            res = QVariant::fromValue(tomlTimeToQ(n.as_time()->get()));
        else if constexpr (toml::is_date_time<decltype(n)>)
            res = QVariant::fromValue(tomlDateTimeToQ(n.as_date_time()->get()));

        else if constexpr (toml::is_array<decltype(n)>) {
            if (auto arr = n.as_array()) {
                QVariantList vList;
                for (auto &e : *arr)
                    vList.append(tomlValueToVariant(e));
                res = vList;
            }
        }

        else if constexpr (toml::is_table<decltype(n)>) {
            if (auto tab = n.as_table()) {
                QVariantHash vHash;
                for (auto &&[tk, tv] : *tab)
                    vHash.insert(QString::fromLocal8Bit(tk.data(), tk.length()), tomlValueToVariant(tv));
                res = vHash;
            }
        }
    });

    return res;
}

static QVariantHash tomlToVariantHash(const toml::table &tab)
{
    QVariantHash res;
    for (auto &&[k, v] : tab) {
        res.insert(QString::fromLocal8Bit(k.data(), k.length()), tomlValueToVariant(v));
    }

    return res;
}

QVariantHash parseTomlData(const QByteArray &data, QString &errorMessage)
{
    toml::table table;
    errorMessage = QString();

    try {
        table = toml::parse(data.toStdString());
    } catch (const toml::parse_error &e) {
        std::stringstream error;
        error << e;
        errorMessage = QString::fromStdString(error.str());
        return QVariantHash();
    }

    return tomlToVariantHash(table);
}

QVariantHash parseTomlData(const QString &data, QString &errorMessage)
{
    return parseTomlData(data.toUtf8(), errorMessage);
}

QVariantHash parseTomlFile(const QString &fname, QString &errorMessage)
{
    toml::table table;
    errorMessage = QString();

    try {
        table = toml::parse_file(fname.toStdString());
    } catch (const toml::parse_error &e) {
        std::stringstream error;
        error << e;
        errorMessage = QString::fromStdString(error.str());
        return QVariantHash();
    }

    return tomlToVariantHash(table);
}
