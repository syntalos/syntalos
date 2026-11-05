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

#include "tensor.h"

#include <bit>
#include <cstring>
#include <stdexcept>

namespace OnnxInf
{

namespace
{

float halfToFloat(uint16_t h)
{
    const uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exponent = (h >> 10) & 0x1Fu;
    uint32_t mantissa = h & 0x3FFu;

    uint32_t bits;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            // subnormal half, normalize it
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400u) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x3FFu;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1F) {
        // infinity or NaN
        bits = sign | 0x7F800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    return std::bit_cast<float>(bits);
}

} // namespace

std::string dtypeToString(DType dtype)
{
    switch (dtype) {
    case DType::Float32:
        return "float32";
    case DType::Float16:
        return "float16";
    case DType::Float64:
        return "float64";
    case DType::UInt8:
        return "uint8";
    case DType::Int8:
        return "int8";
    case DType::UInt16:
        return "uint16";
    case DType::Int16:
        return "int16";
    case DType::Int32:
        return "int32";
    case DType::Int64:
        return "int64";
    case DType::Bool:
        return "bool";
    case DType::Unknown:
        break;
    }
    return "unknown";
}

size_t dtypeSize(DType dtype)
{
    switch (dtype) {
    case DType::Float32:
    case DType::Int32:
        return 4;
    case DType::Float16:
    case DType::UInt16:
    case DType::Int16:
        return 2;
    case DType::Float64:
    case DType::Int64:
        return 8;
    case DType::UInt8:
    case DType::Int8:
    case DType::Bool:
        return 1;
    case DType::Unknown:
        break;
    }
    return 0;
}

size_t shapeElementCount(std::span<const int64_t> shape)
{
    size_t count = 1;
    for (const auto dim : shape) {
        if (dim > 0)
            count *= static_cast<size_t>(dim);
        else if (dim == 0)
            return 0;
    }
    return count;
}

std::string shapeToString(std::span<const int64_t> shape)
{
    std::string str = "[";
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i > 0)
            str += ", ";
        str += shape[i] < 0 ? std::string("?") : std::to_string(shape[i]);
    }
    str += "]";
    return str;
}

size_t TensorView::elementCount() const
{
    return shapeElementCount(shape);
}

double TensorView::valueAt(size_t i) const
{
    switch (dtype) {
    case DType::Float32:
        return static_cast<const float *>(data)[i];
    case DType::Float16:
        return halfToFloat(static_cast<const uint16_t *>(data)[i]);
    case DType::Float64:
        return static_cast<const double *>(data)[i];
    case DType::UInt8:
        return static_cast<const uint8_t *>(data)[i];
    case DType::Int8:
        return static_cast<const int8_t *>(data)[i];
    case DType::UInt16:
        return static_cast<const uint16_t *>(data)[i];
    case DType::Int16:
        return static_cast<const int16_t *>(data)[i];
    case DType::Int32:
        return static_cast<const int32_t *>(data)[i];
    case DType::Int64:
        return static_cast<double>(static_cast<const int64_t *>(data)[i]);
    case DType::Bool:
        return static_cast<const bool *>(data)[i] ? 1.0 : 0.0;
    case DType::Unknown:
        break;
    }
    throw std::logic_error("Can not read values of a tensor with unknown element type");
}

const TensorView *TensorSet::find(const std::string &name) const
{
    for (const auto &t : tensors) {
        if (t.name == name)
            return &t;
    }
    return nullptr;
}

Tensor::Tensor(DType dtype, std::vector<int64_t> shape)
{
    reset(dtype, std::move(shape));
}

void Tensor::reset(DType dtype, std::vector<int64_t> shape)
{
    m_dtype = dtype;
    m_shape = std::move(shape);
    m_byteSize = shapeElementCount(m_shape) * dtypeSize(m_dtype);
    if (m_buffer.size() < m_byteSize)
        m_buffer.resize(m_byteSize);
}

DType Tensor::dtype() const
{
    return m_dtype;
}

const std::vector<int64_t> &Tensor::shape() const
{
    return m_shape;
}

size_t Tensor::elementCount() const
{
    return shapeElementCount(m_shape);
}

size_t Tensor::byteSize() const
{
    return m_byteSize;
}

void *Tensor::data()
{
    return m_buffer.data();
}

const void *Tensor::data() const
{
    return m_buffer.data();
}

void Tensor::fillZero()
{
    if (m_byteSize > 0)
        std::memset(m_buffer.data(), 0, m_byteSize);
}

TensorView Tensor::view(const std::string &name) const
{
    return TensorView{
        .name = name,
        .dtype = m_dtype,
        .shape = m_shape,
        .data = m_buffer.data(),
    };
}

} // namespace OnnxInf
