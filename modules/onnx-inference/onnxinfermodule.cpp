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

#include "onnxinfermodule.h"

#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <pthread.h>
#include <QCryptographicHash>
#include <QFile>
#include <QtConcurrent>

#include "settingsdialog.h"
#include "units.h"

using namespace Syntalos;
using namespace OnnxInf;
namespace fs = std::filesystem;

namespace
{

auto sha256OfFile(const fs::path &file) -> std::expected<std::string, std::string>
{
    QFile f(QString::fromStdString(file.string()));
    if (!f.open(QIODevice::ReadOnly))
        return std::unexpected(std::format("Unable to read {}: {}", file.string(), f.errorString().toStdString()));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f))
        return std::unexpected(std::format("Unable to read {}.", file.string()));
    return hash.result().toHex().toStdString();
}

/**
 * Store a model path relative to the project directory if the model lives inside it.
 */
std::string modelPathForSettings(const fs::path &modelFile, const fs::path &baseDir)
{
    if (modelFile.empty())
        return {};
    if (!baseDir.empty()) {
        const auto rel = modelFile.lexically_normal().lexically_relative(baseDir.lexically_normal());
        if (!rel.empty() && *rel.begin() != "..")
            return rel.string();
    }
    return modelFile.string();
}

} // namespace

OnnxInferenceModule::OnnxInferenceModule(SyntalosLink *slink, QObject *parent)
    : QObject(parent),
      SyntalosLinkModule(slink),
      m_link(slink)
{
    m_framesIn = registerInputPortOrAbort<Frame>("frames-in", "Frames", this, &OnnxInferenceModule::onFrameReceived);
    m_rowsOut = registerOutputPortOrAbort<TableRow>("rows-out", "Results");

    m_link->setShowSettingsCallback([this]() {
        showSettings();
    });

    connect(
        &m_inspectWatcher,
        &QFutureWatcher<ModelInspection>::finished,
        this,
        &OnnxInferenceModule::onModelInspected);

    m_statusTimer.setInterval(500);
    connect(&m_statusTimer, &QTimer::timeout, this, &OnnxInferenceModule::updateStatus);

    updateStatus();
    setState(ModuleState::IDLE);
}

OnnxInferenceModule::~OnnxInferenceModule()
{
    stopInferenceThread();
    m_inspectWatcher.waitForFinished();
    delete m_settingsDlg;
}

void OnnxInferenceModule::showSettings()
{
    if (!m_settingsDlg) {
        m_settingsDlg = new OnnxSettingsDialog;
        connect(m_settingsDlg, &OnnxSettingsDialog::modelFileSelected, this, [this](const QString &file) {
            selectModelFile(file.toStdString());
        });
        connect(m_settingsDlg, &OnnxSettingsDialog::recipeChanged, this, [this](const Recipe &recipe) {
            m_recipe = recipe;
        });
        connect(m_settingsDlg, &OnnxSettingsDialog::engineOptionsChanged, this, [this](const EngineOptions &options) {
            m_engineOptions = options;
            updateStatus();
        });
        connect(m_settingsDlg, &OnnxSettingsDialog::autoFillRequested, this, [this]() {
            if (m_modelInfo.has_value()) {
                autoFillRecipe();
                updateDialog();
            } else {
                inspectModel(true);
            }
        });

        updateDialog();
        if (!m_modelInfo.has_value() && !m_modelFile.empty())
            inspectModel(false);
    }

    const auto name = m_link->moduleName();
    if (!name.empty())
        m_settingsDlg->setWindowTitle(QStringLiteral("%1 - Settings").arg(QString::fromStdString(name)));
    m_settingsDlg->setRunning(m_running);
    m_settingsDlg->show();
    m_settingsDlg->raise();
    m_settingsDlg->activateWindow();
}

void OnnxInferenceModule::selectModelFile(const fs::path &file)
{
    m_modelFile = file;
    m_modelInfo.reset();
    m_modelInfoError.clear();
    m_recipeNotes.clear();
    inspectModel(true);
    updateStatus();
}

void OnnxInferenceModule::inspectModel(bool autoFillRecipe)
{
    if (m_modelFile.empty())
        return;

    m_inspectAutoFill = autoFillRecipe;
    if (m_settingsDlg)
        m_settingsDlg->setModelInfoLoading();

    const auto file = m_modelFile;
    m_inspectWatcher.setFuture(QtConcurrent::run([file]() {
        return ModelInspection{.modelFile = file, .info = OrtEngine::inspectModel(file)};
    }));
}

void OnnxInferenceModule::onModelInspected()
{
    const auto res = m_inspectWatcher.result();
    if (res.modelFile != m_modelFile)
        return; // another model was selected meanwhile

    if (!res.info.has_value()) {
        m_modelInfo.reset();
        m_modelInfoError = QString::fromStdString(res.info.error());
    } else {
        m_modelInfo = *res.info;
        m_modelInfoError.clear();
        if (m_inspectAutoFill)
            autoFillRecipe();
    }

    updateDialog();
}

void OnnxInferenceModule::autoFillRecipe()
{
    m_recipeNotes.clear();

    const auto sidecar = sidecarRecipePath(m_modelFile);
    if (fs::exists(sidecar)) {
        auto res = loadRecipeFile(sidecar);
        if (res.has_value()) {
            m_recipe = res->recipe;
            m_recipeNotes.append(
                QStringLiteral("Recipe imported from %1.").arg(QString::fromStdString(sidecar.filename().string())));
            for (const auto &w : res->warnings)
                m_recipeNotes.append(QString::fromStdString(w));
            return;
        }
        m_recipeNotes.append(QString::fromStdString(res.error()));
    }

    if (m_modelInfo.has_value()) {
        m_recipe = recipeSkeleton(*m_modelInfo);
        m_recipeNotes.append(
            QStringLiteral("The model has no metadata we know, so the recipe was derived from its tensor shapes."));
    }
}

void OnnxInferenceModule::updateDialog()
{
    if (!m_settingsDlg)
        return;

    m_settingsDlg->setModelFile(QString::fromStdString(m_modelFile.string()));
    if (m_modelInfo.has_value())
        m_settingsDlg->setModelInfo(&*m_modelInfo);
    else
        m_settingsDlg->setModelInfo(nullptr, m_modelInfoError);
    m_settingsDlg->setRecipe(m_recipe);
    m_settingsDlg->setRecipeNotes(m_recipeNotes);
    m_settingsDlg->setEngineOptions(m_engineOptions);
}

bool OnnxInferenceModule::prepare()
{
    SyntalosLinkModule::prepare();

    if (m_modelFile.empty()) {
        raiseError("No model selected. Please choose an ONNX model file in the module settings.");
        return false;
    }
    if (!fs::exists(m_modelFile)) {
        raiseError(std::format("The model file {} does not exist.", m_modelFile.string()));
        return false;
    }

    // keep the session of the previous run if nothing changed
    if (!m_engine.isLoadedWith(m_modelFile, m_engineOptions)) {
        m_warmupOutputs.reset();
        m_modelSha256.clear();

        if (auto r = m_engine.load(m_modelFile, m_engineOptions); !r) {
            raiseError(r.error());
            return false;
        }
        auto sha = sha256OfFile(m_modelFile);
        if (!sha) {
            raiseError(sha.error());
            return false;
        }
        m_modelSha256 = *sha;
    }

    const auto &info = m_engine.modelInfo();
    if (info.inputs.size() != 1) {
        raiseError(
            std::format("The model has {} inputs. Only models with one input are supported.", info.inputs.size()));
        return false;
    }
    if (m_recipe.adapter != "image") {
        raiseError(std::format("The input adapter '{}' is not available.", m_recipe.adapter));
        return false;
    }

    m_adapter = std::make_unique<ImageAdapter>(m_recipe.input);
    std::optional<cv::Size> streamSize;
    if (const auto size = m_framesIn->metadata().value<MetaSize>("size"); size.has_value() && !size->isEmpty())
        streamSize = cv::Size(size->width, size->height);
    if (auto r = m_adapter->configure(info.inputs[0], streamSize); !r) {
        raiseError(r.error());
        return false;
    }
    for (const auto &note : m_adapter->notes())
        std::cerr << "onnx-inference: " << note << std::endl;

    // one run with zeros, so slow first-run initialization does not hit the first frame,
    // and so the decoder learns the concrete output shapes
    if (!m_warmupOutputs.has_value() || m_warmupShape != m_adapter->inputShape()) {
        Tensor zeros(m_adapter->inputDType(), m_adapter->inputShape());
        zeros.fillZero();
        auto out = m_engine.run(zeros);
        if (!out) {
            raiseError(std::format("Warm-up run failed: {}", out.error()));
            return false;
        }
        m_warmupOutputs = std::move(*out);
        m_warmupShape = m_adapter->inputShape();
    }

    auto decoder = createDecoder(m_recipe.decoder, m_recipe.output);
    if (!decoder) {
        raiseError(decoder.error());
        return false;
    }
    if (auto r = (*decoder)->configure(m_warmupOutputs->tensors, ImageAdapter::indexColumn()); !r) {
        raiseError(r.error());
        return false;
    }
    m_decoder = std::move(*decoder);

    MetaArray header;
    for (const auto &col : m_decoder->header())
        header.push_back(col);
    m_rowsOut->setMetadataVar("table_header", header);

    if (auto r = writeRunRecord(); !r) {
        raiseError(r.error());
        return false;
    }

    ++m_runGeneration;
    m_framesDropped = 0;
    m_lastFramesDropped = 0;
    {
        const std::lock_guard lock(m_statsMutex);
        m_statsCount = 0;
        m_statsSumMsec = 0;
    }

    // the settings must not change while a run uses them
    if (m_settingsDlg)
        m_settingsDlg->setRunning(true);

    setState(ModuleState::READY);
    return true;
}

auto OnnxInferenceModule::writeRunRecord() -> std::expected<void, std::string>
{
    auto dset = createDefaultDataset();
    if (!dset)
        return std::unexpected(std::format("Unable to create the dataset: {}", dset.error()));
    m_dataset = *dset;

    auto record = m_recipe;
    record.modelFile = m_modelFile.string();
    record.modelSha256 = m_modelSha256;

    auto recipePath = m_dataset->addAuxDataFile("recipe.toml", "recipe", "Inference recipe used for this run");
    if (!recipePath)
        return std::unexpected(std::format("Unable to store the recipe: {}", recipePath.error()));
    std::ofstream f(*recipePath);
    f << recipeToString(record);
    if (!f.good())
        return std::unexpected(std::format("Unable to write {}.", recipePath->string()));

    MetaArray inputShape;
    for (const auto dim : m_adapter->inputShape())
        inputShape.push_back(dim);

    m_dataset->insertAttribute("model_file", m_modelFile.filename().string());
    m_dataset->insertAttribute("model_sha256", m_modelSha256);
    m_dataset->insertAttribute("model_family", m_recipe.family);
    m_dataset->insertAttribute("input_adapter", m_recipe.adapter);
    m_dataset->insertAttribute("output_decoder", m_recipe.decoder);
    m_dataset->insertAttribute("input_shape", inputShape);
    m_dataset->insertAttribute("ort_version", OrtEngine::ortVersion());
    m_dataset->insertAttribute("execution_provider", m_engineOptions.provider);
    m_dataset->insertAttribute("intra_op_threads", static_cast<int64_t>(m_engineOptions.intraOpThreads));
    return {};
}

void OnnxInferenceModule::start()
{
    {
        const std::lock_guard lock(m_mailboxMutex);
        m_pendingFrame.reset();
        m_stopThread = false;
    }
    m_inferThread = std::thread(&OnnxInferenceModule::inferenceLoop, this, m_runGeneration);

    m_lastStatusTime = std::chrono::steady_clock::now();
    m_statusTimer.start();
    SyntalosLinkModule::start();
}

void OnnxInferenceModule::onFrameReceived(const Frame &frame)
{
    if (!m_running)
        return;

    {
        const std::lock_guard lock(m_mailboxMutex);
        // we never queue: a frame the inference thread did not pick up yet is replaced.
        // clone() shares the pixel buffer, and the link never overwrites a buffer someone else holds.
        if (m_pendingFrame.has_value())
            m_framesDropped++;
        m_pendingFrame = frame.clone();
    }
    m_mailboxCond.notify_one();
}

void OnnxInferenceModule::inferenceLoop(uint64_t runGeneration)
{
    pthread_setname_np(pthread_self(), "onnx-infer");

    // errors are raised from the link thread, and only if they belong to the current run
    const auto fail = [this, runGeneration](const std::string &message) {
        postToLinkThread([this, runGeneration, message]() {
            if (runGeneration == m_runGeneration && m_running)
                raiseError(message);
        });
    };

    Tensor input;
    ImageGeometry geometry;
    std::vector<Row> rows;

    while (true) {
        Frame frame;
        {
            std::unique_lock lock(m_mailboxMutex);
            m_mailboxCond.wait(lock, [this]() {
                return m_stopThread || m_pendingFrame.has_value();
            });
            if (m_stopThread)
                break;
            frame = std::move(*m_pendingFrame);
            m_pendingFrame.reset();
        }

        const auto startTime = std::chrono::steady_clock::now();

        if (auto r = m_adapter->process(frame.mat, input, geometry); !r) {
            fail(r.error());
            break;
        }
        auto outputs = m_engine.run(input);
        if (!outputs) {
            fail(outputs.error());
            break;
        }

        rows.clear();
        const DecodeInput decodeInput{
            .index = frame.index,
            .timeUsec = frame.time.count(),
            .image = geometry,
        };
        if (auto r = m_decoder->decode(*outputs, decodeInput, rows); !r) {
            fail(r.error());
            break;
        }

        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - startTime;
        {
            const std::lock_guard lock(m_statsMutex);
            m_statsCount++;
            m_statsSumMsec += elapsed.count();
        }

        if (rows.empty())
            continue;
        postToLinkThread([this, runGeneration, rows = std::move(rows)]() mutable {
            if (runGeneration != m_runGeneration || !m_running)
                return;
            for (auto &r : rows) {
                TableRow row;
                row.data = std::move(r);
                m_rowsOut->submit(row);
            }
        });
        rows = {};
    }
}

void OnnxInferenceModule::stopInferenceThread()
{
    {
        const std::lock_guard lock(m_mailboxMutex);
        m_stopThread = true;
    }
    m_mailboxCond.notify_all();
    if (m_inferThread.joinable())
        m_inferThread.join();

    const std::lock_guard lock(m_mailboxMutex);
    m_pendingFrame.reset();
}

void OnnxInferenceModule::stop()
{
    stopInferenceThread();
    m_statusTimer.stop();
    m_dataset.reset();

    updateStatus();
    if (m_settingsDlg)
        m_settingsDlg->setRunning(false);

    SyntalosLinkModule::stop();
}

void OnnxInferenceModule::shutdown()
{
    stopInferenceThread();
    if (m_settingsDlg)
        m_settingsDlg->hide();
    SyntalosLinkModule::shutdown();
}

std::string OnnxInferenceModule::modelDisplayName() const
{
    return m_modelFile.stem().string();
}

void OnnxInferenceModule::updateStatus()
{
    if (m_modelFile.empty()) {
        setStatusMessage("No model selected");
        return;
    }
    if (!m_running) {
        setStatusMessage(std::format("{} · {}", modelDisplayName(), m_engineOptions.provider));
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> interval = now - m_lastStatusTime;
    m_lastStatusTime = now;
    if (interval.count() <= 0)
        return;

    uint64_t count;
    double sumMsec;
    {
        const std::lock_guard lock(m_statsMutex);
        count = m_statsCount;
        sumMsec = m_statsSumMsec;
        m_statsCount = 0;
        m_statsSumMsec = 0;
    }
    const auto dropped = m_framesDropped - m_lastFramesDropped;
    m_lastFramesDropped = m_framesDropped;

    setStatusMessage(
        std::format(
            "{} · {} · {:.1f} ms · {:.1f}/s · {:.0f} dropped/s",
            modelDisplayName(),
            m_engineOptions.provider,
            count > 0 ? sumMsec / count : 0.0,
            count / interval.count(),
            dropped / interval.count()));
}

void OnnxInferenceModule::saveSettings(ByteVector &settings, const fs::path &baseDir)
{
    auto recipe = m_recipe;
    recipe.modelFile = modelPathForSettings(m_modelFile, baseDir);
    recipe.modelSha256.clear();

    toml::table root;
    recipeToToml(recipe, root);
    root.insert(
        "engine",
        toml::table{
            {"provider",           m_engineOptions.provider                       },
            {"intra_op_threads",   m_engineOptions.intraOpThreads                 },
            {"graph_optimization", graphOptLevelToString(m_engineOptions.optLevel)},
    });

    std::ostringstream ss;
    ss << root << "\n";
    const auto text = ss.str();
    const auto bytes = reinterpret_cast<const std::byte *>(text.data());
    settings.assign(bytes, bytes + text.size());
}

bool OnnxInferenceModule::loadSettings(const ByteVector &settings, const fs::path &baseDir)
{
    if (settings.empty())
        return true;

    toml::table root;
    try {
        root = toml::parse(std::string_view(reinterpret_cast<const char *>(settings.data()), settings.size()));
    } catch (const toml::parse_error &e) {
        raiseError(std::format("Unable to parse the module settings: {}", e.description()));
        return false;
    }

    auto res = recipeFromToml(root);
    if (!res) {
        raiseError(res.error());
        return false;
    }
    for (const auto &w : res->warnings)
        std::cerr << "onnx-inference: settings: " << w << std::endl;

    m_recipe = res->recipe;
    m_modelFile.clear();
    if (!m_recipe.modelFile.empty()) {
        const fs::path file(m_recipe.modelFile);
        m_modelFile = (file.is_relative() && !baseDir.empty() ? baseDir / file : file).lexically_normal();
    }

    m_engineOptions = EngineOptions{};
    if (const auto engine = root["engine"].as_table()) {
        m_engineOptions.provider = (*engine)["provider"].value_or(std::string("CPU"));
        m_engineOptions.intraOpThreads = std::max(1, (*engine)["intra_op_threads"].value_or(2));
        m_engineOptions.optLevel = graphOptLevelFromString(
            (*engine)["graph_optimization"].value_or(std::string("all")));
    }

    m_modelInfo.reset();
    m_modelInfoError.clear();
    m_recipeNotes.clear();
    if (m_settingsDlg) {
        updateDialog();
        inspectModel(false);
    }
    updateStatus();
    return true;
}
