/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Syntalos
{

/**
 * @brief Represents the dimensions of a two-dimensional object with width and height.
 *
 * This structure is used in Syntalos' stream metadata to declare dimensions
 * of two-dimensional objects in advance.
 *
 * Dimensions are 32-bit signed integers for compatibility with other libraries.
 */
struct MetaSize {
    int32_t width = 0;
    int32_t height = 0;

    constexpr MetaSize() = default;
    constexpr MetaSize(int32_t w, int32_t h)
        : width(w),
          height(h)
    {
    }

    [[nodiscard]] constexpr bool isEmpty() const
    {
        return width <= 0 || height <= 0;
    }

    bool operator==(const MetaSize &) const = default;
};

struct MetaValue;

/**
 * @brief Array of values used for stream metadata.
 */
struct MetaArray : std::vector<MetaValue> {
    using vector::vector;
    bool operator==(const MetaArray &) const = default;
};

/**
 * @brief Mapping of values used for stream metadata.
 */
struct MetaStringMap : std::map<std::string, MetaValue> {
    using map::map;

    auto value(const std::string &key) const -> std::optional<const MetaValue>;
    const MetaValue &valueOr(const std::string &key, const MetaValue &fallback) const;

    template<typename T>
    [[nodiscard]] std::optional<T> value(const std::string &key) const;
    template<typename T>
    [[nodiscard]] T valueOr(const std::string &key, T fallback) const;
};

/**
 * @brief Data type that can be used as stream metadata.
 */
struct MetaValue
    : std::variant<std::nullptr_t, bool, int64_t, double, std::string, MetaSize, MetaArray, MetaStringMap> {
    using Base = std::variant<std::nullptr_t, bool, int64_t, double, std::string, MetaSize, MetaArray, MetaStringMap>;
    using variant::variant;

    MetaValue(const char *s)
        : variant(std::string{s})
    {
    }

    MetaValue(int32_t v)
        : variant(static_cast<int64_t>(v))
    {
    }

    bool operator==(const MetaValue &other) const
    {
        return static_cast<const Base &>(*this) == static_cast<const Base &>(other);
    }

    /**
     * @brief Get the contained value as the selected type.
     */
    template<typename T>
    [[nodiscard]] std::optional<T> get() const
    {
        const T *ptr = std::get_if<T>(this);
        return ptr ? std::optional<T>(*ptr) : std::nullopt;
    }

    /**
     * @brief Get the contained value as the selected type or return a fallback value.
     */
    template<typename T>
    [[nodiscard]] T getOr(T fallback) const
    {
        const T *ptr = std::get_if<T>(this);
        return ptr ? *ptr : std::move(fallback);
    }
};

/**
 * @brief Retrieves the value associated with the specified key in the map.
 *
 * @param key The string key to search for in the map.
 * @return A constant reference to an optional containing the value associated with the key if found,
 *         or `std::nullopt` if the key is not present.
 */
inline auto MetaStringMap::value(const std::string &key) const -> std::optional<const MetaValue>
{
    auto it = find(key);
    if (it != end())
        return it->second;
    return std::nullopt;
}

/**
 * @brief Retrieves the value associated with the specified key or returns a fallback value.
 *
 * If the key is found, the corresponding value is returned. If the key is not found,
 * the provided fallback value is returned instead.
 *
 * @param key The string key to search for in the map.
 * @param fallback The value to return if the key is not present in the map.
 * @return A reference to the value associated with the key if found; otherwise, a reference to the fallback value.
 */
inline const MetaValue &MetaStringMap::valueOr(const std::string &key, const MetaValue &fallback) const
{
    auto it = find(key);
    return it != end() ? it->second : fallback;
}

/**
 * @brief Retrieves the value associated with the specified key or returns a fallback value.
 */
template<typename T>
[[nodiscard]] std::optional<T> MetaStringMap::value(const std::string &key) const
{
    const auto v = this->value(key);
    return v.has_value() ? v->template get<T>() : std::nullopt;
}

/**
 * @brief Retrieves the value associated with the specified key or returns a fallback value.
 */
template<typename T>
[[nodiscard]] T MetaStringMap::valueOr(const std::string &key, T fallback) const
{
    const auto v = this->value(key);
    return v.has_value() ? v->template getOr<T>(std::move(fallback)) : std::move(fallback);
}

} // namespace Syntalos
