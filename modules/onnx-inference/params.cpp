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

#include "params.h"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace OnnxInf
{

namespace
{

std::string formatParamNumber(double value)
{
    return std::format("{}", value);
}

/**
 * Read a TOML node as a parameter value of the type the spec wants.
 */
auto paramValueFromToml(const ParamSpec &spec, const toml::node &node) -> std::expected<ParamValue, std::string>
{
    switch (spec.type) {
    case ParamType::Bool:
        if (node.is_boolean())
            return node.as_boolean()->get();
        return std::unexpected("expected true or false");

    case ParamType::Int:
        if (node.is_integer())
            return node.as_integer()->get();
        return std::unexpected("expected an integer");

    case ParamType::Float:
        if (node.is_floating_point())
            return node.as_floating_point()->get();
        if (node.is_integer())
            return static_cast<double>(node.as_integer()->get());
        return std::unexpected("expected a number");

    case ParamType::String:
    case ParamType::Choice:
        if (node.is_string())
            return node.as_string()->get();
        return std::unexpected("expected a string");

    case ParamType::Size: {
        const auto arr = node.as_array();
        if (arr == nullptr || arr->size() != 2 || !arr->is_homogeneous(toml::node_type::integer))
            return std::unexpected("expected two integers, [width, height]");
        return std::vector<int64_t>{arr->at(0).as_integer()->get(), arr->at(1).as_integer()->get()};
    }

    case ParamType::FloatList: {
        const auto arr = node.as_array();
        if (arr == nullptr)
            return std::unexpected("expected a list of numbers");
        std::vector<double> list;
        list.reserve(arr->size());
        for (const auto &item : *arr) {
            if (item.is_floating_point())
                list.push_back(item.as_floating_point()->get());
            else if (item.is_integer())
                list.push_back(static_cast<double>(item.as_integer()->get()));
            else
                return std::unexpected("expected a list of numbers");
        }
        return list;
    }

    case ParamType::StringList: {
        const auto arr = node.as_array();
        if (arr == nullptr)
            return std::unexpected("expected a list of strings");
        std::vector<std::string> list;
        list.reserve(arr->size());
        for (const auto &item : *arr) {
            if (!item.is_string())
                return std::unexpected("expected a list of strings");
            list.push_back(item.as_string()->get());
        }
        return list;
    }
    }

    return std::unexpected("unknown parameter type");
}

} // namespace

auto validateParamValue(const ParamSpec &spec, const ParamValue &value) -> std::expected<void, std::string>
{
    const auto checkRange = [&spec](double v) -> std::expected<void, std::string> {
        if (v < spec.minValue || v > spec.maxValue)
            return std::unexpected(
                std::format(
                    "value {} is outside of the range {} to {}",
                    formatParamNumber(v),
                    formatParamNumber(spec.minValue),
                    formatParamNumber(spec.maxValue)));
        return {};
    };

    switch (spec.type) {
    case ParamType::Bool:
        if (!std::holds_alternative<bool>(value))
            return std::unexpected("expected a boolean");
        return {};

    case ParamType::Int:
        if (!std::holds_alternative<int64_t>(value))
            return std::unexpected("expected an integer");
        return checkRange(static_cast<double>(std::get<int64_t>(value)));

    case ParamType::Float:
        if (!std::holds_alternative<double>(value))
            return std::unexpected("expected a number");
        return checkRange(std::get<double>(value));

    case ParamType::String:
        if (!std::holds_alternative<std::string>(value))
            return std::unexpected("expected a string");
        return {};

    case ParamType::Choice: {
        if (!std::holds_alternative<std::string>(value))
            return std::unexpected("expected a string");
        const auto &str = std::get<std::string>(value);
        const bool known = std::ranges::any_of(spec.choices, [&str](const ParamChoice &c) {
            return c.value == str;
        });
        if (!known) {
            std::string valid;
            for (const auto &c : spec.choices)
                valid += (valid.empty() ? "" : ", ") + c.value;
            return std::unexpected(std::format("'{}' is not one of: {}", str, valid));
        }
        return {};
    }

    case ParamType::Size: {
        if (!std::holds_alternative<std::vector<int64_t>>(value))
            return std::unexpected("expected [width, height]");
        const auto &size = std::get<std::vector<int64_t>>(value);
        if (size.size() != 2)
            return std::unexpected("expected [width, height]");
        for (const auto v : size) {
            if (auto r = checkRange(static_cast<double>(v)); !r)
                return r;
        }
        return {};
    }

    case ParamType::FloatList:
        if (!std::holds_alternative<std::vector<double>>(value))
            return std::unexpected("expected a list of numbers");
        return {};

    case ParamType::StringList:
        if (!std::holds_alternative<std::vector<std::string>>(value))
            return std::unexpected("expected a list of strings");
        return {};
    }

    return std::unexpected("unknown parameter type");
}

ParamSet::ParamSet(const std::vector<ParamSpec> &specs)
    : m_specs(&specs)
{
    resetToDefaults();
}

const std::vector<ParamSpec> &ParamSet::specs() const
{
    static const std::vector<ParamSpec> noSpecs;
    return m_specs != nullptr ? *m_specs : noSpecs;
}

const ParamSpec *ParamSet::spec(const std::string &key) const
{
    for (const auto &s : specs()) {
        if (s.key == key)
            return &s;
    }
    return nullptr;
}

const ParamValue &ParamSet::value(const std::string &key) const
{
    const auto it = m_values.find(key);
    if (it == m_values.end())
        throw std::out_of_range(std::format("Unknown recipe parameter '{}'", key));
    return it->second;
}

bool ParamSet::getBool(const std::string &key) const
{
    return std::get<bool>(value(key));
}

int64_t ParamSet::getInt(const std::string &key) const
{
    return std::get<int64_t>(value(key));
}

double ParamSet::getFloat(const std::string &key) const
{
    return std::get<double>(value(key));
}

const std::string &ParamSet::getString(const std::string &key) const
{
    return std::get<std::string>(value(key));
}

std::vector<int64_t> ParamSet::getSize(const std::string &key) const
{
    return std::get<std::vector<int64_t>>(value(key));
}

const std::vector<double> &ParamSet::getFloatList(const std::string &key) const
{
    return std::get<std::vector<double>>(value(key));
}

const std::vector<std::string> &ParamSet::getStringList(const std::string &key) const
{
    return std::get<std::vector<std::string>>(value(key));
}

auto ParamSet::set(const std::string &key, const ParamValue &value) -> std::expected<void, std::string>
{
    const auto s = spec(key);
    if (s == nullptr)
        return std::unexpected(std::format("unknown parameter '{}'", key));
    if (auto r = validateParamValue(*s, value); !r)
        return std::unexpected(std::format("{}: {}", key, r.error()));
    m_values[key] = value;
    return {};
}

void ParamSet::resetToDefaults()
{
    m_values.clear();
    for (const auto &s : specs())
        m_values[s.key] = s.defaultValue;
}

std::vector<std::string> ParamSet::loadToml(const toml::table &tab, const std::vector<std::string> &ignoreKeys)
{
    std::vector<std::string> warnings;
    for (const auto &[tkey, node] : tab) {
        const std::string key(tkey.str());
        if (std::ranges::contains(ignoreKeys, key))
            continue;

        const auto s = spec(key);
        if (s == nullptr) {
            warnings.push_back(std::format("Ignored unknown parameter '{}'.", key));
            continue;
        }

        auto value = paramValueFromToml(*s, node);
        if (!value) {
            warnings.push_back(std::format("Ignored parameter '{}': {}.", key, value.error()));
            continue;
        }
        if (auto r = set(key, *value); !r)
            warnings.push_back(std::format("Ignored parameter {}.", r.error()));
    }

    return warnings;
}

void ParamSet::writeToml(toml::table &tab) const
{
    for (const auto &s : specs()) {
        const auto &v = value(s.key);
        switch (s.type) {
        case ParamType::Bool:
            tab.insert_or_assign(s.key, std::get<bool>(v));
            break;
        case ParamType::Int:
            tab.insert_or_assign(s.key, std::get<int64_t>(v));
            break;
        case ParamType::Float:
            tab.insert_or_assign(s.key, std::get<double>(v));
            break;
        case ParamType::String:
        case ParamType::Choice:
            tab.insert_or_assign(s.key, std::get<std::string>(v));
            break;
        case ParamType::Size: {
            toml::array arr;
            for (const auto dim : std::get<std::vector<int64_t>>(v))
                arr.push_back(dim);
            tab.insert_or_assign(s.key, std::move(arr));
            break;
        }
        case ParamType::FloatList: {
            toml::array arr;
            for (const auto item : std::get<std::vector<double>>(v))
                arr.push_back(item);
            tab.insert_or_assign(s.key, std::move(arr));
            break;
        }
        case ParamType::StringList: {
            toml::array arr;
            for (const auto &item : std::get<std::vector<std::string>>(v))
                arr.push_back(item);
            tab.insert_or_assign(s.key, std::move(arr));
            break;
        }
        }
    }
}

bool ParamSet::operator==(const ParamSet &other) const
{
    return m_specs == other.m_specs && m_values == other.m_values;
}

} // namespace OnnxInf
