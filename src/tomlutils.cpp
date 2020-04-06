/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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
    tomlDt.time_offset = offset;

    return tomlDt;
}

// awful preprocesser macro, since I don't know a good template-based
// way to do this with the current version of TOML++ that needs to know types
// immediately for template instanciation and can't infer anything at runtime
// This macro needs a GCC-compliant compiler (GCC and Clang will work)
#define CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(func, var) ({ \
    bool success = true;                               \
    if ((var).type() == QVariant::Bool) {              \
        func((var).toBool());                          \
    }                                                  \
                                                       \
    else if ((var).isNull()) {                         \
        /* leave out the value */                      \
    }                                                  \
                                                       \
    else if ((var).type() == QVariant::String) {       \
        func((var).toString().toStdString());          \
    }                                                  \
                                                       \
    else if ((var).type() == QVariant::Int) {          \
        func((var).toInt());                           \
    }                                                  \
                                                       \
    else if ((var).canConvert<double>()) {             \
        func((var).toDouble());                        \
    }                                                  \
                                                       \
    else if ((var).canConvert<int64_t>()) {            \
        func((var).value<int64_t>());                  \
    }                                                  \
                                                       \
    else if ((var).type() == QVariant::Time) {         \
        func(qTimeToToml((var).toTime()));             \
    }                                                  \
                                                       \
    else if ((var).type() == QVariant::Date) {         \
        func(qDateToToml((var).toDate()));             \
    }                                                  \
                                                       \
    else if ((var).canConvert<QDateTime>()) {          \
        func(qDateTimeToToml((var).toDateTime()));     \
    }                                                  \
                                                       \
    /* check Qt knows how to convert the unknown */    \
    /* value  to a string representation. */           \
    else if ((var).canConvert<QString>()) {            \
        func((var).toString().toStdString());          \
    }                                                  \
                                                       \
    else {                                             \
        /* unable to convert this value */             \
        success = false;                               \
    }                                                  \
    success;                                           \
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

        if (var.canConvert<QVariantList>()) {
            arr.push_back(qVariantListToTomlArray(var.toList()));
            continue;
        }

        if (CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(arr.push_back, var))
            continue;

        qWarning().noquote() << QStringLiteral("Unable to store type `%1` in TOML attributes (array).").arg(var.typeName());
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

        if (var.canConvert<QVariantList>()) {
            tab.insert(key, qVariantListToTomlArray(var.toList()));
            continue;
        }

        auto tabInsertFunc = [&](auto v) { tab.insert(key, v); };
        if (CONVERT_SIMPLE_QTYPE_TO_TOMLTYPE(tabInsertFunc, var))
            continue;

        qWarning().noquote() << QStringLiteral("Unable to store type `%1` in TOML attributes (table).").arg(var.typeName());
        tab.insert(key, "�");
    }

    return tab;
}

QString serializeTomlTable(const toml::table &tab)
{
    std::stringstream data;
    data << tab << "\n";
    return QString::fromStdString(data.str());
}
