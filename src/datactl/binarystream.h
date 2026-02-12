/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <memory>
#include <string>
#include <vector>

namespace Syntalos
{

/**
 * @brief Writer for Syntalos data entity serialization
 */
class BinaryStreamWriter
{
public:
    explicit BinaryStreamWriter(std::vector<std::byte> &buf)
        : m_buffer(buf)
    {
    }

    explicit BinaryStreamWriter(const std::vector<std::byte> &buf)
        : m_buffer(const_cast<std::vector<std::byte> &>(buf))
    {
    }

    template<typename T>
    void write(const T &value)
        requires std::is_trivially_copyable_v<T>
    {
        const size_t oldSize = m_buffer.size();
        m_buffer.resize(oldSize + sizeof(T));
        std::memcpy(m_buffer.data() + oldSize, &value, sizeof(T));
    }

    void write(const std::string &str)
    {
        const uint64_t size = str.size();
        write(size);
        if (size == 0)
            return;

        const size_t oldSize = m_buffer.size();
        m_buffer.resize(oldSize + size);
        std::memcpy(m_buffer.data() + oldSize, str.data(), size);
    }

    void write(const std::vector<std::string> &vec)
    {
        const uint64_t size = vec.size();
        write(size);
        for (const auto &str : vec)
            write(str);
    }

    void reserve(size_t capacity)
    {
        m_buffer.reserve(capacity);
    }

    [[nodiscard]] size_t position() const
    {
        return m_buffer.size();
    }

private:
    std::vector<std::byte> &m_buffer;
};

/**
 * @brief Reader for Syntalos data entity deserialization
 */
class BinaryStreamReader
{
public:
    explicit BinaryStreamReader(std::span<const std::byte> buf)
        : m_buffer(buf),
          m_pos(0)
    {
    }

    explicit BinaryStreamReader(const void *data, size_t size)
        : m_buffer(reinterpret_cast<const std::byte *>(data), size),
          m_pos(0)
    {
    }

    template<typename T>
    void read(T &value)
        requires std::is_trivially_copyable_v<T>
    {
        if (m_pos + sizeof(T) > m_buffer.size())
            throw std::runtime_error("BinaryStream read overflow");
        std::memcpy(&value, m_buffer.data() + m_pos, sizeof(T));
        m_pos += sizeof(T);
    }

    void read(std::string &str)
    {
        uint64_t size;
        read(size);
        if (m_pos + size > m_buffer.size())
            throw std::runtime_error("BinaryStream read overflow");
        if (size > 0) {
            str.assign(reinterpret_cast<const char *>(m_buffer.data() + m_pos), size);
            m_pos += size;
        } else {
            str.clear();
        }
    }

    void read(std::vector<std::string> &vec)
    {
        uint64_t size;
        read(size);
        vec.clear();
        vec.reserve(size);
        for (uint64_t i = 0; i < size; ++i) {
            std::string str;
            read(str);
            vec.push_back(std::move(str));
        }
    }

    [[nodiscard]] size_t position() const
    {
        return m_pos;
    }

    void reset()
    {
        m_pos = 0;
    }

private:
    std::span<const std::byte> m_buffer;
    size_t m_pos;
};

} // namespace Syntalos
