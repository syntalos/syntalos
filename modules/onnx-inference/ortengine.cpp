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

#include "ortengine.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <limits>
#include <mutex>
#include <onnxruntime_cxx_api.h>

namespace OnnxInf
{

namespace
{

std::mutex g_logMutex;
EngineLogFn g_logHandler;

void ORT_API_CALL ortLogCallback(
    void * /*param*/,
    OrtLoggingLevel severity,
    const char * /*category*/,
    const char * /*logid*/,
    const char * /*codeLocation*/,
    const char *message)
{
    EngineLogLevel level = EngineLogLevel::Info;
    if (severity >= ORT_LOGGING_LEVEL_ERROR)
        level = EngineLogLevel::Error;
    else if (severity == ORT_LOGGING_LEVEL_WARNING)
        level = EngineLogLevel::Warning;

    const std::lock_guard lock(g_logMutex);
    if (g_logHandler)
        g_logHandler(level, message);
    else
        std::cerr << "ONNX Runtime: " << message << std::endl;
}

Ort::Env &ortEnv()
{
    // Intentionally never destroyed: tearing the environment down during static
    // destruction can race ONNX Runtime's own global state.
    static auto env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "onnx-inference", ortLogCallback, nullptr);
    return *env;
}

DType dtypeFromOrt(ONNXTensorElementDataType type)
{
    switch (type) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
        return DType::Float32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16:
        return DType::Float16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
        return DType::Float64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
        return DType::UInt8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:
        return DType::Int8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:
        return DType::UInt16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:
        return DType::Int16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
        return DType::Int32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
        return DType::Int64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
        return DType::Bool;
    default:
        break;
    }
    return DType::Unknown;
}

ONNXTensorElementDataType dtypeToOrt(DType dtype)
{
    switch (dtype) {
    case DType::Float32:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    case DType::Float16:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
    case DType::Float64:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
    case DType::UInt8:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
    case DType::Int8:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
    case DType::UInt16:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
    case DType::Int16:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
    case DType::Int32:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
    case DType::Int64:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
    case DType::Bool:
        return ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
    case DType::Unknown:
        break;
    }
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
}

GraphOptimizationLevel graphOptLevelToOrt(GraphOptLevel level)
{
    switch (level) {
    case GraphOptLevel::Disabled:
        return ORT_DISABLE_ALL;
    case GraphOptLevel::Basic:
        return ORT_ENABLE_BASIC;
    case GraphOptLevel::Extended:
        return ORT_ENABLE_EXTENDED;
    case GraphOptLevel::All:
        break;
    }
    return ORT_ENABLE_ALL;
}

TensorInfo tensorInfoFromOrt(const std::string &name, const Ort::TypeInfo &typeInfo)
{
    TensorInfo info;
    info.name = name;

    // sequence and map values can not be fed or decoded by us, they keep an unknown type
    if (typeInfo.GetONNXType() != ONNX_TYPE_TENSOR)
        return info;

    const auto tsInfo = typeInfo.GetTensorTypeAndShapeInfo();
    info.dtype = dtypeFromOrt(tsInfo.GetElementType());
    info.shape = tsInfo.GetShape();

    const auto dimCount = tsInfo.GetDimensionsCount();
    std::vector<const char *> dimNames(dimCount, nullptr);
    if (dimCount > 0)
        tsInfo.GetSymbolicDimensions(dimNames.data(), dimCount);
    info.dimNames.reserve(dimCount);
    for (const auto dn : dimNames)
        info.dimNames.emplace_back(dn != nullptr ? dn : "");

    return info;
}

ModelInfo introspectSession(Ort::Session &session)
{
    Ort::AllocatorWithDefaultOptions alloc;
    ModelInfo info;

    for (size_t i = 0; i < session.GetInputCount(); ++i) {
        const auto name = session.GetInputNameAllocated(i, alloc);
        info.inputs.push_back(tensorInfoFromOrt(name.get(), session.GetInputTypeInfo(i)));
    }
    for (size_t i = 0; i < session.GetOutputCount(); ++i) {
        const auto name = session.GetOutputNameAllocated(i, alloc);
        info.outputs.push_back(tensorInfoFromOrt(name.get(), session.GetOutputTypeInfo(i)));
    }

    const auto md = session.GetModelMetadata();
    info.producerName = md.GetProducerNameAllocated(alloc).get();
    info.graphName = md.GetGraphNameAllocated(alloc).get();
    info.domain = md.GetDomainAllocated(alloc).get();
    info.description = md.GetDescriptionAllocated(alloc).get();
    // ONNX Runtime reports INT64_MAX if the model does not set a version
    info.version = md.GetVersion();
    if (info.version == std::numeric_limits<int64_t>::max())
        info.version = 0;

    for (const auto &key : md.GetCustomMetadataMapKeysAllocated(alloc)) {
        const auto value = md.LookupCustomMetadataMapAllocated(key.get(), alloc);
        info.metadata[key.get()] = value ? value.get() : "";
    }

    return info;
}

struct FileStamp {
    fs::path path;
    uintmax_t size = 0;
    fs::file_time_type mtime;

    bool operator==(const FileStamp &) const = default;
};

auto fileStamp(const fs::path &file) -> std::expected<FileStamp, std::string>
{
    std::error_code ec;
    FileStamp stamp;
    stamp.path = fs::weakly_canonical(file, ec);
    if (ec)
        return std::unexpected(std::format("Unable to resolve model path {}: {}", file.string(), ec.message()));
    stamp.size = fs::file_size(stamp.path, ec);
    if (ec)
        return std::unexpected(std::format("Unable to read model file {}: {}", file.string(), ec.message()));
    stamp.mtime = fs::last_write_time(stamp.path, ec);
    if (ec)
        return std::unexpected(std::format("Unable to read model file {}: {}", file.string(), ec.message()));
    return stamp;
}

} // namespace

std::string graphOptLevelToString(GraphOptLevel level)
{
    switch (level) {
    case GraphOptLevel::Disabled:
        return "disabled";
    case GraphOptLevel::Basic:
        return "basic";
    case GraphOptLevel::Extended:
        return "extended";
    case GraphOptLevel::All:
        break;
    }
    return "all";
}

GraphOptLevel graphOptLevelFromString(const std::string &str)
{
    if (str == "disabled")
        return GraphOptLevel::Disabled;
    if (str == "basic")
        return GraphOptLevel::Basic;
    if (str == "extended")
        return GraphOptLevel::Extended;
    return GraphOptLevel::All;
}

class OrtEngine::Private
{
public:
    Private()
        : memInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
    }

    std::unique_ptr<Ort::Session> session;
    Ort::MemoryInfo memInfo;
    FileStamp stamp;
    fs::path modelFile;
    EngineOptions options;
    ModelInfo info;

    std::vector<std::string> inputNames;
    std::vector<std::string> outputNames;
    std::vector<const char *> inputNamePtrs;
    std::vector<const char *> outputNamePtrs;
};

OrtEngine::OrtEngine()
    : d(std::make_unique<Private>())
{
}

OrtEngine::~OrtEngine() = default;

std::string OrtEngine::ortVersion()
{
    return Ort::GetVersionString();
}

std::vector<std::string> OrtEngine::availableProviders()
{
    try {
        return Ort::GetAvailableProviders();
    } catch (const Ort::Exception &e) {
        std::cerr << "Unable to list ONNX Runtime execution providers: " << e.what() << std::endl;
    }
    return {};
}

void OrtEngine::setLogHandler(EngineLogFn handler)
{
    const std::lock_guard lock(g_logMutex);
    g_logHandler = std::move(handler);
}

auto OrtEngine::inspectModel(const fs::path &modelFile) -> std::expected<ModelInfo, std::string>
{
    if (!fs::exists(modelFile))
        return std::unexpected(std::format("Model file {} does not exist.", modelFile.string()));

    try {
        Ort::SessionOptions so;
        so.SetIntraOpNumThreads(1);
        so.SetInterOpNumThreads(1);
        so.SetGraphOptimizationLevel(ORT_DISABLE_ALL);
        Ort::Session session(ortEnv(), modelFile.c_str(), so);
        return introspectSession(session);
    } catch (const Ort::Exception &e) {
        return std::unexpected(std::format("Unable to load model {}: {}", modelFile.filename().string(), e.what()));
    }
}

auto OrtEngine::load(const fs::path &modelFile, const EngineOptions &options) -> std::expected<void, std::string>
{
    unload();

    if (options.provider != "CPU")
        return std::unexpected(
            std::format("Execution provider '{}' is not supported yet, only CPU is.", options.provider));

    auto stamp = fileStamp(modelFile);
    if (!stamp)
        return std::unexpected(stamp.error());

    try {
        Ort::SessionOptions so;
        so.SetIntraOpNumThreads(std::max(1, options.intraOpThreads));
        so.SetInterOpNumThreads(1);
        so.SetExecutionMode(ORT_SEQUENTIAL);
        so.SetGraphOptimizationLevel(graphOptLevelToOrt(options.optLevel));

        // we wait for frames most of the time, busy-waiting threads would just burn CPU
        so.AddConfigEntry("session.intra_op.allow_spinning", "0");
        so.AddConfigEntry("session.inter_op.allow_spinning", "0");

        d->session = std::make_unique<Ort::Session>(ortEnv(), stamp->path.c_str(), so);
        d->info = introspectSession(*d->session);
    } catch (const Ort::Exception &e) {
        d->session.reset();
        return std::unexpected(std::format("Unable to load model {}: {}", modelFile.filename().string(), e.what()));
    }

    for (const auto &ti : d->info.inputs)
        d->inputNames.push_back(ti.name);
    for (const auto &ti : d->info.outputs)
        d->outputNames.push_back(ti.name);
    for (const auto &n : d->inputNames)
        d->inputNamePtrs.push_back(n.c_str());
    for (const auto &n : d->outputNames)
        d->outputNamePtrs.push_back(n.c_str());

    d->stamp = *stamp;
    d->modelFile = modelFile;
    d->options = options;
    return {};
}

void OrtEngine::unload()
{
    d->session.reset();
    d->stamp = {};
    d->modelFile.clear();
    d->info = {};
    d->inputNames.clear();
    d->outputNames.clear();
    d->inputNamePtrs.clear();
    d->outputNamePtrs.clear();
}

bool OrtEngine::isLoaded() const
{
    return d->session != nullptr;
}

bool OrtEngine::isLoadedWith(const fs::path &modelFile, const EngineOptions &options) const
{
    if (!d->session || options != d->options)
        return false;
    const auto stamp = fileStamp(modelFile);
    return stamp.has_value() && *stamp == d->stamp;
}

const fs::path &OrtEngine::modelFile() const
{
    return d->modelFile;
}

const ModelInfo &OrtEngine::modelInfo() const
{
    return d->info;
}

const EngineOptions &OrtEngine::options() const
{
    return d->options;
}

auto OrtEngine::run(const Tensor &input) -> std::expected<TensorSet, std::string>
{
    if (!d->session)
        return std::unexpected("No model loaded.");
    if (d->info.inputs.size() != 1)
        return std::unexpected(
            std::format("The model has {} inputs, only models with one input are supported.", d->info.inputs.size()));
    if (input.dtype() != d->info.inputs[0].dtype)
        return std::unexpected(
            std::format(
                "Input tensor is {}, but the model expects {}.",
                dtypeToString(input.dtype()),
                dtypeToString(d->info.inputs[0].dtype)));

    try {
        // ONNX Runtime only reads the input, the const_cast is safe
        auto inValue = Ort::Value::CreateTensor(
            d->memInfo,
            const_cast<void *>(input.data()),
            input.byteSize(),
            input.shape().data(),
            input.shape().size(),
            dtypeToOrt(input.dtype()));

        auto values = std::make_shared<std::vector<Ort::Value>>(d->session->Run(
            Ort::RunOptions{nullptr},
            d->inputNamePtrs.data(),
            &inValue,
            1,
            d->outputNamePtrs.data(),
            d->outputNamePtrs.size()));

        TensorSet result;
        result.tensors.reserve(values->size());
        for (size_t i = 0; i < values->size(); ++i) {
            const auto &v = (*values)[i];
            TensorView view;
            view.name = d->outputNames[i];
            if (v.IsTensor()) {
                const auto tsInfo = v.GetTensorTypeAndShapeInfo();
                view.dtype = dtypeFromOrt(tsInfo.GetElementType());
                view.shape = tsInfo.GetShape();
                if (view.dtype != DType::Unknown)
                    view.data = v.GetTensorRawData();
            }
            result.tensors.push_back(std::move(view));
        }
        result.storage = values;
        return result;
    } catch (const Ort::Exception &e) {
        return std::unexpected(std::format("Inference failed: {}", e.what()));
    }
}

} // namespace OnnxInf
