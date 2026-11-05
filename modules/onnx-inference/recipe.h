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
#include <filesystem>
#include <string>
#include <vector>
#include <toml++/toml.h>

#include "ortengine.h"
#include "params.h"

namespace OnnxInf
{

namespace fs = std::filesystem;

/**
 * Everything needed to run one model: which adapter feeds it, which decoder reads
 * its outputs, and their parameters.
 */
struct Recipe {
    Recipe();

    std::string family = "generic";
    std::string modelFile;   /// as written in the TOML, may be relative
    std::string modelSha256; /// optional, filled in for the run record

    std::string adapter = "image";
    ParamSet input;

    std::string decoder = "raw";
    ParamSet output;

    /**
     * Switch to another adapter or decoder. The parameters are reset to their defaults
     * when the unit changes. Returns false for unknown IDs.
     */
    bool setAdapter(const std::string &id);
    bool setDecoder(const std::string &id);

    bool operator==(const Recipe &other) const = default;
};

struct RecipeLoadResult {
    Recipe recipe;
    std::vector<std::string> warnings;
};

/**
 * Read a recipe from the [model], [input] and [output] tables of a TOML document.
 * Unknown keys and bad values only produce warnings; an unknown adapter or decoder is an error.
 */
auto recipeFromToml(const toml::table &root) -> std::expected<RecipeLoadResult, std::string>;

/**
 * Write a recipe as [model], [input] and [output] tables into @p root.
 */
void recipeToToml(const Recipe &recipe, toml::table &root);

auto loadRecipeFile(const fs::path &path) -> std::expected<RecipeLoadResult, std::string>;
std::string recipeToString(const Recipe &recipe);

/**
 * Path of the sidecar recipe for a model, e.g. "model.onnx" -> "model.recipe.toml".
 */
fs::path sidecarRecipePath(const fs::path &modelFile);

/**
 * A starting recipe for a model, derived from its tensor shapes alone:
 * rank-4 inputs get the image adapter, outputs get the raw decoder.
 */
Recipe recipeSkeleton(const ModelInfo &info);

} // namespace OnnxInf
