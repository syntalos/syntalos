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

#include "recipe.h"

#include <format>
#include <sstream>

#include "adapters/imageadapter.h"
#include "units.h"

namespace OnnxInf
{

namespace
{

void prefixWarnings(std::vector<std::string> &target, const std::vector<std::string> &warnings, const char *table)
{
    for (const auto &w : warnings)
        target.push_back(std::format("[{}] {}", table, w));
}

} // namespace

Recipe::Recipe()
    : input(ImageAdapter::paramSpecs()),
      output(findDecoderUnit("raw")->paramSpecs())
{
}

bool Recipe::setAdapter(const std::string &id)
{
    const auto unit = findAdapterUnit(id);
    if (unit == nullptr)
        return false;
    if (id != adapter) {
        adapter = id;
        input = ParamSet(unit->paramSpecs());
    }
    return true;
}

bool Recipe::setDecoder(const std::string &id)
{
    const auto unit = findDecoderUnit(id);
    if (unit == nullptr)
        return false;
    if (id != decoder) {
        decoder = id;
        output = ParamSet(unit->paramSpecs());
    }
    return true;
}

auto recipeFromToml(const toml::table &root) -> std::expected<RecipeLoadResult, std::string>
{
    RecipeLoadResult res;
    auto &recipe = res.recipe;

    if (const auto model = root["model"].as_table()) {
        for (const auto &[key, node] : *model) {
            if (key == "family" && node.is_string())
                recipe.family = node.as_string()->get();
            else if (key == "file" && node.is_string())
                recipe.modelFile = node.as_string()->get();
            else if (key == "sha256" && node.is_string())
                recipe.modelSha256 = node.as_string()->get();
            else
                res.warnings.push_back(std::format("[model] Ignored entry '{}'.", key.str()));
        }
    }

    if (const auto input = root["input"].as_table()) {
        const auto adapter = (*input)["adapter"].value_or(std::string("image"));
        if (!recipe.setAdapter(adapter))
            return std::unexpected(std::format("The recipe uses the unknown input adapter '{}'.", adapter));
        prefixWarnings(res.warnings, recipe.input.loadToml(*input, {"adapter"}), "input");
    }

    if (const auto output = root["output"].as_table()) {
        const auto decoder = (*output)["decoder"].value_or(std::string("raw"));
        if (!recipe.setDecoder(decoder))
            return std::unexpected(std::format("The recipe uses the unknown output decoder '{}'.", decoder));
        prefixWarnings(res.warnings, recipe.output.loadToml(*output, {"decoder"}), "output");
    }

    return res;
}

void recipeToToml(const Recipe &recipe, toml::table &root)
{
    toml::table model;
    model.insert("family", recipe.family);
    if (!recipe.modelFile.empty())
        model.insert("file", recipe.modelFile);
    if (!recipe.modelSha256.empty())
        model.insert("sha256", recipe.modelSha256);

    toml::table input;
    input.insert("adapter", recipe.adapter);
    recipe.input.writeToml(input);

    toml::table output;
    output.insert("decoder", recipe.decoder);
    recipe.output.writeToml(output);

    root.insert_or_assign("model", std::move(model));
    root.insert_or_assign("input", std::move(input));
    root.insert_or_assign("output", std::move(output));
}

auto loadRecipeFile(const fs::path &path) -> std::expected<RecipeLoadResult, std::string>
{
    try {
        const auto root = toml::parse_file(path.string());
        return recipeFromToml(root);
    } catch (const toml::parse_error &e) {
        return std::unexpected(std::format("Unable to parse recipe {}: {}", path.string(), e.description()));
    }
}

std::string recipeToString(const Recipe &recipe)
{
    toml::table root;
    recipeToToml(recipe, root);
    std::ostringstream ss;
    ss << root << "\n";
    return ss.str();
}

fs::path sidecarRecipePath(const fs::path &modelFile)
{
    auto path = modelFile;
    path.replace_extension(".recipe.toml");
    return path;
}

Recipe recipeSkeleton(const ModelInfo &info)
{
    Recipe recipe;

    if (!info.inputs.empty() && info.inputs[0].shape.size() == 4) {
        const auto &input = info.inputs[0];
        const auto &s = input.shape;
        const auto isChannelDim = [](int64_t dim) {
            return dim == 1 || dim == 3;
        };

        // channels-last only if the last dimension looks like channels and the second does not
        const bool nhwc = isChannelDim(s[3]) && !isChannelDim(s[1]);
        const auto channels = nhwc ? s[3] : s[1];
        const auto height = nhwc ? s[1] : s[2];
        const auto width = nhwc ? s[2] : s[3];

        (void)recipe.input.set("layout", std::string(nhwc ? "nhwc" : "nchw"));
        (void)recipe.input.set("color", std::string(channels == 1 ? "gray" : "rgb"));
        if (width > 0 && height > 0)
            (void)recipe.input.set("size", std::vector<int64_t>{width, height});
        if (input.dtype == DType::UInt8)
            (void)recipe.input.set("normalize", std::string("none"));
    }

    if (!info.outputs.empty())
        (void)recipe.output.set("tensor", info.outputs[0].name);

    return recipe;
}

} // namespace OnnxInf
