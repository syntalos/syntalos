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

#include "units.h"

#include <format>

#include "adapters/imageadapter.h"
#include "decoders/rawdecoder.h"
#include "decoders/scoresdecoder.h"

namespace OnnxInf
{

const std::vector<UnitInfo> &adapterUnits()
{
    static const std::vector<UnitInfo> units = {
        {
         .id = "image",
         .title = "Image",
         .description = "Feeds video frames to models with an image input.",
         .paramSpecs = &ImageAdapter::paramSpecs,
         },
    };
    return units;
}

const std::vector<UnitInfo> &decoderUnits()
{
    static const std::vector<UnitInfo> units = {
        {
         .id = "raw",
         .title = "Raw values",
         .description = "Writes the values of one small output as they are.",
         .paramSpecs = &RawDecoder::paramSpecs,
         },
        {
         .id = "scores",
         .title = "Class scores",
         .description = "Class or event scores, with the best class and its label.",
         .paramSpecs = &ScoresDecoder::paramSpecs,
         },
    };
    return units;
}

const UnitInfo *findAdapterUnit(const std::string &id)
{
    for (const auto &u : adapterUnits()) {
        if (u.id == id)
            return &u;
    }
    return nullptr;
}

const UnitInfo *findDecoderUnit(const std::string &id)
{
    for (const auto &u : decoderUnits()) {
        if (u.id == id)
            return &u;
    }
    return nullptr;
}

auto createDecoder(const std::string &id, const ParamSet &params)
    -> std::expected<std::unique_ptr<OutputDecoder>, std::string>
{
    if (id == "raw")
        return std::make_unique<RawDecoder>(params);
    if (id == "scores")
        return std::make_unique<ScoresDecoder>(params);
    return std::unexpected(std::format("Unknown output decoder '{}'.", id));
}

} // namespace OnnxInf
