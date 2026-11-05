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
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tensor.h"

namespace OnnxInf
{

namespace fs = std::filesystem;

/**
 * Name, element type and shape of a model input or output.
 * Dynamic dimensions are -1, their symbolic names (if any) are in dimNames.
 */
struct TensorInfo {
    std::string name;
    DType dtype = DType::Unknown;
    std::vector<int64_t> shape;
    std::vector<std::string> dimNames;
};

/**
 * Everything we can learn about a model without running it.
 */
struct ModelInfo {
    std::vector<TensorInfo> inputs;
    std::vector<TensorInfo> outputs;
    std::map<std::string, std::string> metadata; /// custom metadata_props of the model

    std::string producerName;
    std::string graphName;
    std::string domain;
    std::string description;
    int64_t version = 0; /// 0 if the model has none
};

enum class GraphOptLevel {
    Disabled,
    Basic,
    Extended,
    All,
};

std::string graphOptLevelToString(GraphOptLevel level);
GraphOptLevel graphOptLevelFromString(const std::string &str);

struct EngineOptions {
    std::string provider = "CPU";
    int intraOpThreads = 2;
    GraphOptLevel optLevel = GraphOptLevel::All;

    bool operator==(const EngineOptions &) const = default;
};

enum class EngineLogLevel {
    Info,
    Warning,
    Error,
};

using EngineLogFn = std::function<void(EngineLogLevel level, const std::string &message)>;

/**
 * Runs ONNX models with ONNX Runtime.
 *
 * One engine holds at most one inference session. Running it is not thread-safe,
 * use it from one thread at a time.
 */
class OrtEngine
{
public:
    OrtEngine();
    ~OrtEngine();

    OrtEngine(const OrtEngine &) = delete;
    OrtEngine &operator=(const OrtEngine &) = delete;

    static std::string ortVersion();
    static std::vector<std::string> availableProviders();

    /**
     * Route ONNX Runtime's own log messages. Must be called before any other engine
     * function is used, the default handler prints to stderr.
     */
    static void setLogHandler(EngineLogFn handler);

    /**
     * Load a model only to read its inputs, outputs and metadata. This creates a
     * throwaway session with all graph optimizations off, which is fast and may
     * be called from any thread.
     */
    static auto inspectModel(const fs::path &modelFile) -> std::expected<ModelInfo, std::string>;

    /**
     * Create the inference session.
     */
    auto load(const fs::path &modelFile, const EngineOptions &options) -> std::expected<void, std::string>;
    void unload();

    [[nodiscard]] bool isLoaded() const;

    /**
     * Whether the current session was made for this file and these options.
     * The file counts as unchanged while its size and modification time are the same.
     */
    [[nodiscard]] bool isLoadedWith(const fs::path &modelFile, const EngineOptions &options) const;

    [[nodiscard]] const fs::path &modelFile() const;
    [[nodiscard]] const ModelInfo &modelInfo() const;
    [[nodiscard]] const EngineOptions &options() const;

    /**
     * Run the model on one input. Only models with exactly one input are supported.
     */
    auto run(const Tensor &input) -> std::expected<TensorSet, std::string>;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace OnnxInf
