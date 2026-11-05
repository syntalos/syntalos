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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace OnnxInf
{

/**
 * Element types of tensors we know about.
 */
enum class DType {
    Unknown,
    Float32,
    Float16,
    Float64,
    UInt8,
    Int8,
    UInt16,
    Int16,
    Int32,
    Int64,
    Bool,
};

std::string dtypeToString(DType dtype);
size_t dtypeSize(DType dtype);

/**
 * Number of elements of a tensor with the given shape.
 * Dynamic dimensions (-1) count as one.
 */
size_t shapeElementCount(std::span<const int64_t> shape);
std::string shapeToString(std::span<const int64_t> shape);

/**
 * Non-owning view of a dense, row-major tensor.
 */
struct TensorView {
    std::string name;
    DType dtype = DType::Unknown;
    std::vector<int64_t> shape;
    const void *data = nullptr;

    [[nodiscard]] size_t elementCount() const;

    /**
     * Element @p i converted to double. Works for every DType except Unknown.
     */
    [[nodiscard]] double valueAt(size_t i) const;
};

/**
 * Named tensors, e.g. the outputs of one inference run.
 * The views stay valid as long as this set (or a copy of it) exists.
 */
struct TensorSet {
    std::vector<TensorView> tensors;
    std::shared_ptr<const void> storage;

    [[nodiscard]] const TensorView *find(const std::string &name) const;
};

/**
 * Small owning tensor, used for model inputs and in tests.
 */
class Tensor
{
public:
    Tensor() = default;
    Tensor(DType dtype, std::vector<int64_t> shape);

    /**
     * Change type and shape. The buffer is only reallocated if it has to grow,
     * its contents are undefined afterwards.
     */
    void reset(DType dtype, std::vector<int64_t> shape);

    [[nodiscard]] DType dtype() const;
    [[nodiscard]] const std::vector<int64_t> &shape() const;
    [[nodiscard]] size_t elementCount() const;
    [[nodiscard]] size_t byteSize() const;

    void *data();
    [[nodiscard]] const void *data() const;

    template<typename T>
    T *dataAs()
    {
        return static_cast<T *>(data());
    }

    template<typename T>
    const T *dataAs() const
    {
        return static_cast<const T *>(data());
    }

    void fillZero();

    [[nodiscard]] TensorView view(const std::string &name = {}) const;

private:
    DType m_dtype{DType::Unknown};
    std::vector<int64_t> m_shape;
    std::vector<std::byte> m_buffer;
    size_t m_byteSize{0};
};

} // namespace OnnxInf
