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

#include "decoder.h"

#include <format>

namespace OnnxInf
{

std::string formatValue(double value)
{
    return std::format("{:.7g}", value);
}

auto selectTensor(const std::vector<TensorView> &tensors, const std::string &name)
    -> std::expected<const TensorView *, std::string>
{
    if (tensors.empty())
        return std::unexpected("The model has no outputs.");
    if (name.empty())
        return &tensors.front();

    for (const auto &t : tensors) {
        if (t.name == name)
            return &t;
    }

    std::string names;
    for (const auto &t : tensors)
        names += (names.empty() ? "" : ", ") + t.name;
    return std::unexpected(std::format("The model has no output named '{}'. Its outputs are: {}", name, names));
}

} // namespace OnnxInf
