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

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "params.h"

namespace OnnxInf
{

class OutputDecoder;

/**
 * An input adapter or output decoder a recipe can select.
 */
struct UnitInfo {
    std::string id;
    std::string title;
    std::string description;
    const std::vector<ParamSpec> &(*paramSpecs)();
};

const std::vector<UnitInfo> &adapterUnits();
const std::vector<UnitInfo> &decoderUnits();

const UnitInfo *findAdapterUnit(const std::string &id);
const UnitInfo *findDecoderUnit(const std::string &id);

auto createDecoder(const std::string &id, const ParamSet &params)
    -> std::expected<std::unique_ptr<OutputDecoder>, std::string>;

} // namespace OnnxInf
