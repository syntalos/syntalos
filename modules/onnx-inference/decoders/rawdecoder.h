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

#include "decoder.h"
#include "params.h"

namespace OnnxInf
{

/**
 * Writes the values of one small output tensor as they are, one row per inference.
 *
 * This is the escape hatch for models no other decoder understands.
 */
class RawDecoder : public OutputDecoder
{
public:
    static const std::vector<ParamSpec> &paramSpecs();

    explicit RawDecoder(const ParamSet &params);

    auto configure(const std::vector<TensorView> &outputs, const std::string &indexColumn)
        -> std::expected<void, std::string> override;
    [[nodiscard]] std::vector<std::string> header() const override;
    auto decode(const TensorSet &outputs, const DecodeInput &input, std::vector<Row> &rows)
        -> std::expected<void, std::string> override;

private:
    std::string m_tensorParam;
    size_t m_maxValues;

    std::string m_tensorName;
    size_t m_valueCount{0};
    std::vector<std::string> m_header;
};

} // namespace OnnxInf
