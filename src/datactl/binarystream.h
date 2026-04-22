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

#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "streammeta.h"

namespace Syntalos
{

/**
 * @brief ByteVector is a vector of bytes using the polymorphic allocator.
 *
 * In Syntalos' heavily threaded environment, where in the worst case a
 * ton of these vectors may be created and destroyed in quick succession,
 * the default glibc allocator is less than optimal.
 * Mimalloc has shown to perform a lot better for this burst-like, threaded
 * activity, therefore we prefer it.
 * Please try to reuse buffers whenever possible though - while mimalloc is
 * a significant improvement, reusing memory segments is even better.
 *
 * We do use the C++ polymorphic allocator since datactl is used in places
 * where swapping the allocator is undesirable, e.g. in mlink that may be
 * linked into non-Syntalos processes. These processes may use the default
 * allocator instead of Syntalos' mimalloc-based one.
 */
using ByteVector = std::pmr::vector<std::byte>;

/**
 * @brief Type tag used in MetaValue binary serialization.
 */
enum class MetaTypeTag : uint8_t {
    Null = 0,
    BoolFalse = 1,
    BoolTrue = 2,
    Int64 = 3,
    Double = 4,
    String = 5,
    Size = 6,
    Array = 7,
    Object = 8,
};

/**
 * @brief Writer for Syntalos data entity serialization
 */
class BinaryStreamWriter
{
public:
    explicit BinaryStreamWriter(ByteVector &buf)
        : m_buffer(buf)
    {
        m_buffer.clear();
    }

    explicit BinaryStreamWriter(const ByteVector &buf)
        : m_buffer(const_cast<ByteVector &>(buf))
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

    void write(const ByteVector &blob);
    void write(const MetaValue &v);
    void write(const MetaArray &arr);
    void write(const MetaStringMap &map);

    void reserve(size_t capacity)
    {
        m_buffer.reserve(capacity);
    }

    [[nodiscard]] size_t position() const
    {
        return m_buffer.size();
    }

private:
    ByteVector &m_buffer;
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

    void read(ByteVector &blob);
    void read(MetaValue &v);
    void read(MetaArray &arr);
    void read(MetaStringMap &map);

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

// ---------------------------------------------------------------------------
// Out-of-line inline definitions for MetaValue / MetaStringMap / ByteVector
// ---------------------------------------------------------------------------

inline void BinaryStreamWriter::write(const ByteVector &blob)
{
    write(static_cast<uint64_t>(blob.size()));
    if (blob.empty())
        return;
    const size_t oldSize = m_buffer.size();
    m_buffer.resize(oldSize + blob.size());
    std::memcpy(m_buffer.data() + oldSize, blob.data(), blob.size());
}

inline void BinaryStreamWriter::write(const MetaArray &arr)
{
    write(static_cast<uint64_t>(arr.size()));
    for (const auto &elem : arr)
        write(elem);
}

inline void BinaryStreamWriter::write(const MetaStringMap &map)
{
    write(static_cast<uint64_t>(map.size()));
    for (const auto &[key, val] : map) {
        write(key);
        write(val);
    }
}

inline void BinaryStreamWriter::write(const MetaValue &v)
{
    const auto &var = static_cast<const MetaValue::Base &>(v);
    switch (var.index()) {
    case 0: // nullptr_t
        write(static_cast<uint8_t>(MetaTypeTag::Null));
        break;
    case 1: // bool
        write(static_cast<uint8_t>(std::get<bool>(var) ? MetaTypeTag::BoolTrue : MetaTypeTag::BoolFalse));
        break;
    case 2: // int64_t
        write(static_cast<uint8_t>(MetaTypeTag::Int64));
        write(std::get<int64_t>(var));
        break;
    case 3: // double
        write(static_cast<uint8_t>(MetaTypeTag::Double));
        write(std::get<double>(var));
        break;
    case 4: // string
        write(static_cast<uint8_t>(MetaTypeTag::String));
        write(std::get<std::string>(var));
        break;
    case 5: // MetaSize (trivially copyable - written as raw bytes)
        write(static_cast<uint8_t>(MetaTypeTag::Size));
        write(std::get<MetaSize>(var));
        break;
    case 6: // MetaArray
        write(static_cast<uint8_t>(MetaTypeTag::Array));
        write(std::get<MetaArray>(var));
        break;
    case 7: // MetaStringMap
        write(static_cast<uint8_t>(MetaTypeTag::Object));
        write(std::get<MetaStringMap>(var));
        break;
    default:
        break;
    }
}

inline void BinaryStreamReader::read(ByteVector &blob)
{
    uint64_t size;
    read(size);
    blob.clear();
    if (size == 0)
        return;
    if (m_pos + size > m_buffer.size())
        throw std::runtime_error("BinaryStream read overflow");
    blob.resize(size);
    std::memcpy(blob.data(), m_buffer.data() + m_pos, size);
    m_pos += size;
}

inline void BinaryStreamReader::read(MetaArray &arr)
{
    uint64_t count;
    read(count);
    arr.clear();
    arr.resize(count);
    for (auto &elem : arr)
        read(elem);
}

inline void BinaryStreamReader::read(MetaStringMap &map)
{
    uint64_t count;
    read(count);
    map.clear();
    for (uint64_t i = 0; i < count; ++i) {
        std::string key;
        MetaValue val;
        read(key);
        read(val);
        map[std::move(key)] = std::move(val);
    }
}

inline void BinaryStreamReader::read(MetaValue &v)
{
    uint8_t tagByte;
    read(tagByte);
    switch (static_cast<MetaTypeTag>(tagByte)) {
    case MetaTypeTag::Null:
        v = nullptr;
        break;
    case MetaTypeTag::BoolFalse:
        v = false;
        break;
    case MetaTypeTag::BoolTrue:
        v = true;
        break;
    case MetaTypeTag::Int64: {
        int64_t val;
        read(val);
        v = val;
        break;
    }
    case MetaTypeTag::Double: {
        double val;
        read(val);
        v = val;
        break;
    }
    case MetaTypeTag::String: {
        std::string val;
        read(val);
        v = std::move(val);
        break;
    }
    case MetaTypeTag::Size: {
        MetaSize val;
        read(val);
        v = val;
        break;
    }
    case MetaTypeTag::Array: {
        MetaArray arr;
        read(arr);
        v = std::move(arr);
        break;
    }
    case MetaTypeTag::Object: {
        MetaStringMap map;
        read(map);
        v = std::move(map);
        break;
    }
    default:
        throw std::runtime_error("BinaryStream: unknown MetaTypeTag");
    }
}

} // namespace Syntalos
