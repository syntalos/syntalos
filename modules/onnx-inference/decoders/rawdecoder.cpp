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

#include "rawdecoder.h"

#include <algorithm>
#include <format>

namespace OnnxInf
{

const std::vector<ParamSpec> &RawDecoder::paramSpecs()
{
    static const std::vector<ParamSpec> specs = {
        {
         .key = "tensor",
         .type = ParamType::String,
         .title = "Output tensor",
         .help = "Name of the model output to write. Empty selects the first output.",
         .defaultValue = std::string(),
         },
        {
         .key = "max_values",
         .type = ParamType::Int,
         .title = "Maximum values",
         .help = "Largest number of values per row. Bigger outputs need a decoder that understands them.",
         .defaultValue = int64_t{256},
         .minValue = 1,
         .maxValue = 4096,
         },
    };
    return specs;
}

RawDecoder::RawDecoder(const ParamSet &params)
    : m_tensorParam(params.getString("tensor")),
      m_maxValues(static_cast<size_t>(params.getInt("max_values")))
{
}

auto RawDecoder::configure(const std::vector<TensorView> &outputs, const std::string &indexColumn)
    -> std::expected<void, std::string>
{
    const auto tensor = selectTensor(outputs, m_tensorParam);
    if (!tensor)
        return std::unexpected(tensor.error());
    const auto t = *tensor;

    if (t->dtype == DType::Unknown)
        return std::unexpected(std::format("Model output '{}' has a type the raw decoder can not read.", t->name));

    m_valueCount = t->elementCount();
    if (m_valueCount > m_maxValues)
        return std::unexpected(
            std::format(
                "Model output '{}' has {} values, but the raw decoder writes at most {} per row.",
                t->name,
                m_valueCount,
                m_maxValues));

    m_tensorName = t->name;
    m_header = {"time_usec", indexColumn};
    for (size_t i = 0; i < m_valueCount; ++i)
        m_header.push_back(std::format("v{}", i));

    return {};
}

std::vector<std::string> RawDecoder::header() const
{
    return m_header;
}

auto RawDecoder::decode(const TensorSet &outputs, const DecodeInput &input, std::vector<Row> &rows)
    -> std::expected<void, std::string>
{
    const auto t = outputs.find(m_tensorName);
    if (t == nullptr || t->data == nullptr)
        return std::unexpected(std::format("Model output '{}' is missing.", m_tensorName));

    // rows keep the width of the header, even if a dynamic output changes its size
    const auto count = std::min(t->elementCount(), m_valueCount);
    Row row;
    row.reserve(2 + m_valueCount);
    row.push_back(std::to_string(input.timeUsec));
    row.push_back(std::to_string(input.index));
    for (size_t i = 0; i < count; ++i)
        row.push_back(formatValue(t->valueAt(i)));
    row.resize(2 + m_valueCount);

    rows.push_back(std::move(row));
    return {};
}

} // namespace OnnxInf
