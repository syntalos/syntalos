/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <string>
#include <variant>
#include <vector>
#include <toml++/toml.h>

namespace OnnxInf
{

/**
 * Kind of value a recipe parameter holds.
 */
enum class ParamType {
    Bool,
    Int,
    Float,
    String,
    Choice,     /// one string out of a fixed set
    Size,       /// width and height, as [w, h] in TOML
    FloatList,  /// list of numbers
    StringList, /// list of strings
};

using ParamValue = std::
    variant<bool, int64_t, double, std::string, std::vector<int64_t>, std::vector<double>, std::vector<std::string>>;

struct ParamChoice {
    std::string value;
    std::string title;
};

/**
 * Description of one parameter of an input adapter or output decoder.
 *
 * Each adapter and decoder declares its parameters once with these. The same list
 * drives TOML loading and saving, validation and the settings form.
 */
struct ParamSpec {
    std::string key;   /// TOML key
    ParamType type;    /// kind of value
    std::string title; /// short label for the settings form
    std::string help;  /// longer explanation, shown as tooltip
    ParamValue defaultValue;
    std::vector<ParamChoice> choices = {}; /// valid values, for ParamType::Choice
    double minValue = std::numeric_limits<double>::lowest();
    double maxValue = std::numeric_limits<double>::max();
};

/**
 * Values for a list of parameter specs. Missing values are always filled with
 * the parameter's default, so every key of the spec list can be read.
 */
class ParamSet
{
public:
    ParamSet() = default;
    explicit ParamSet(const std::vector<ParamSpec> &specs);

    [[nodiscard]] const std::vector<ParamSpec> &specs() const;
    [[nodiscard]] const ParamSpec *spec(const std::string &key) const;

    [[nodiscard]] const ParamValue &value(const std::string &key) const;
    [[nodiscard]] bool getBool(const std::string &key) const;
    [[nodiscard]] int64_t getInt(const std::string &key) const;
    [[nodiscard]] double getFloat(const std::string &key) const;
    [[nodiscard]] const std::string &getString(const std::string &key) const;
    [[nodiscard]] std::vector<int64_t> getSize(const std::string &key) const;
    [[nodiscard]] const std::vector<double> &getFloatList(const std::string &key) const;
    [[nodiscard]] const std::vector<std::string> &getStringList(const std::string &key) const;

    /**
     * Set a value. Fails if the key is unknown, or if the value has the wrong type
     * or lies outside the range or set of choices of its spec.
     */
    auto set(const std::string &key, const ParamValue &value) -> std::expected<void, std::string>;

    void resetToDefaults();

    /**
     * Read values from a TOML table. Keys in @p ignoreKeys are skipped silently.
     * Unknown keys and invalid values are skipped too and returned as warnings,
     * the affected parameters keep their previous value.
     */
    std::vector<std::string> loadToml(const toml::table &tab, const std::vector<std::string> &ignoreKeys = {});

    /**
     * Write all values to a TOML table.
     */
    void writeToml(toml::table &tab) const;

    bool operator==(const ParamSet &other) const;

private:
    const std::vector<ParamSpec> *m_specs{nullptr};
    std::map<std::string, ParamValue> m_values;
};

/**
 * Check whether @p value is valid for @p spec.
 */
auto validateParamValue(const ParamSpec &spec, const ParamValue &value) -> std::expected<void, std::string>;

} // namespace OnnxInf
