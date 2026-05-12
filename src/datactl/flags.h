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

#include <type_traits>

namespace Syntalos
{

/**
 * @brief Type-safe bitfield wrapper for scoped enums.
 *
 * Flags<E> wraps a scoped enum E whose enumerators are individual bit values
 * and provides the usual bitwise operations, testFlag / setFlag helpers, and
 * an explicit bool conversion.
 */
template<typename E>
class Flags
{
    static_assert(std::is_enum_v<E>, "Flags<E> requires an enum type");
    using Int = std::underlying_type_t<E>;

public:
    constexpr Flags() noexcept
        : m_value(0)
    {
    }
    constexpr Flags(E flag) noexcept
        : m_value(static_cast<Int>(flag))
    {
    }
    explicit constexpr Flags(Int raw) noexcept
        : m_value(raw)
    {
    }

    constexpr Flags operator|(Flags rhs) const noexcept
    {
        return Flags(m_value | rhs.m_value);
    }
    constexpr Flags operator&(Flags rhs) const noexcept
    {
        return Flags(m_value & rhs.m_value);
    }
    constexpr Flags operator~() const noexcept
    {
        return Flags(~m_value);
    }
    constexpr Flags &operator|=(Flags rhs) noexcept
    {
        m_value |= rhs.m_value;
        return *this;
    }
    constexpr Flags &operator&=(Flags rhs) noexcept
    {
        m_value &= rhs.m_value;
        return *this;
    }
    constexpr bool operator==(Flags rhs) const noexcept
    {
        return m_value == rhs.m_value;
    }
    constexpr bool operator!=(Flags rhs) const noexcept
    {
        return m_value != rhs.m_value;
    }
    constexpr explicit operator bool() const noexcept
    {
        return m_value != 0;
    }

    constexpr bool hasFlag(E flag) const noexcept
    {
        const auto bit = static_cast<Int>(flag);
        return bit != 0 && (m_value & bit) == bit;
    }

    constexpr Flags &setFlag(E flag, bool enabled = true) noexcept
    {
        const auto bit = static_cast<Int>(flag);
        if (enabled)
            m_value |= bit;
        else
            m_value &= ~bit;
        return *this;
    }

    constexpr Int toInt() const noexcept
    {
        return m_value;
    }

private:
    Int m_value;
};

/**
 * Allow @c E | E to produce @c Flags<E> for any scoped enum.
 *
 * Restricted to scoped enums so we don't shadow the built-in int-returning
 * operator| that plain (unscoped) enums rely on - most notably Eigen's
 * internal storage-option enums, which are evaluated inside Syntalos
 * namespace scope when Eigen templates are instantiated against our types.
 */
template<typename E, typename = std::enable_if_t<std::is_scoped_enum_v<E>>>
constexpr Flags<E> operator|(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) | Flags<E>(rhs);
}

} // namespace Syntalos
