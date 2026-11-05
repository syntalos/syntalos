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
#include <optional>
#include <string>
#include <vector>

#include "adapters/imageadapter.h"
#include "tensor.h"

namespace OnnxInf
{

using Row = std::vector<std::string>;

/**
 * What a decoder knows about the input that produced a set of outputs.
 */
struct DecodeInput {
    uint64_t index = 0;   /// frame index, or window index for signals
    int64_t timeUsec = 0; /// timestamp of the input, never the time the result was ready
    std::optional<ImageGeometry> image;
};

/**
 * Turns the output tensors of a model into table rows.
 */
class OutputDecoder
{
public:
    virtual ~OutputDecoder() = default;

    /**
     * Check that the model's outputs suit this decoder and fix the row layout.
     *
     * @param outputs Outputs of a warm-up run, so all shapes are concrete.
     * @param indexColumn Name of the input index column, e.g. "frame_index".
     */
    virtual auto configure(const std::vector<TensorView> &outputs, const std::string &indexColumn)
        -> std::expected<void, std::string> = 0;

    /**
     * Column names of the rows this decoder emits. Valid after configure().
     */
    [[nodiscard]] virtual std::vector<std::string> header() const = 0;

    /**
     * Decode one set of outputs, appending zero or more rows to @p rows.
     */
    virtual auto decode(const TensorSet &outputs, const DecodeInput &input, std::vector<Row> &rows)
        -> std::expected<void, std::string> = 0;
};

/**
 * Format a number for a table cell, with up to 7 significant digits.
 */
std::string formatValue(double value);

/**
 * Find the tensor a decoder should read: the one named @p name, or the first one if
 * the name is empty.
 */
auto selectTensor(const std::vector<TensorView> &tensors, const std::string &name)
    -> std::expected<const TensorView *, std::string>;

} // namespace OnnxInf
