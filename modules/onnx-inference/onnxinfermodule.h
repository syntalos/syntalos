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

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <syntalos-mlink>

#include "adapters/imageadapter.h"
#include "decoders/decoder.h"
#include "ortengine.h"
#include "recipe.h"

class OnnxSettingsDialog;

/**
 * Result of reading a model in the background, for the settings dialog.
 */
struct ModelInspection {
    std::filesystem::path modelFile;
    std::expected<OnnxInf::ModelInfo, std::string> info;
};

class OnnxInferenceModule : public QObject, public Syntalos::SyntalosLinkModule
{
    Q_OBJECT
public:
    explicit OnnxInferenceModule(Syntalos::SyntalosLink *slink, QObject *parent = nullptr);
    ~OnnxInferenceModule() override;

    bool prepare() override;
    void start() override;
    void stop() override;
    void shutdown() override;

protected:
    void saveSettings(Syntalos::ByteVector &settings, const std::filesystem::path &baseDir) override;
    bool loadSettings(const Syntalos::ByteVector &settings, const std::filesystem::path &baseDir) override;

private:
    void showSettings();
    void selectModelFile(const std::filesystem::path &file);
    void inspectModel(bool autoFillRecipe);
    void onModelInspected();
    void autoFillRecipe();
    void updateDialog();

    void onFrameReceived(const Syntalos::Frame &frame);
    void inferenceLoop(uint64_t runGeneration);
    void stopInferenceThread();
    auto writeRunRecord() -> std::expected<void, std::string>;
    void updateStatus();
    [[nodiscard]] std::string modelDisplayName() const;

    Syntalos::SyntalosLink *m_link;
    std::shared_ptr<Syntalos::InputPortInfo> m_framesIn;
    std::shared_ptr<Syntalos::OutputPortLink<Syntalos::TableRow>> m_rowsOut;

    // settings
    std::filesystem::path m_modelFile;
    OnnxInf::Recipe m_recipe;
    OnnxInf::EngineOptions m_engineOptions;

    // model details for the settings dialog
    QPointer<OnnxSettingsDialog> m_settingsDlg;
    QFutureWatcher<ModelInspection> m_inspectWatcher;
    bool m_inspectAutoFill{false};
    std::optional<OnnxInf::ModelInfo> m_modelInfo;
    QString m_modelInfoError;
    QStringList m_recipeNotes;

    // run setup, only changed while no inference thread runs
    OnnxInf::OrtEngine m_engine;
    std::string m_modelSha256;
    std::optional<OnnxInf::TensorSet> m_warmupOutputs;
    std::vector<int64_t> m_warmupShape;
    std::unique_ptr<OnnxInf::ImageAdapter> m_adapter;
    std::unique_ptr<OnnxInf::OutputDecoder> m_decoder;
    std::shared_ptr<Syntalos::EDLDataset> m_dataset;
    uint64_t m_runGeneration{0};

    // the inference thread and its single-slot mailbox
    std::thread m_inferThread;
    std::mutex m_mailboxMutex;
    std::condition_variable m_mailboxCond;
    std::optional<Syntalos::Frame> m_pendingFrame;
    bool m_stopThread{false};
    uint64_t m_framesDropped{0};

    // statistics for the status line
    std::mutex m_statsMutex;
    uint64_t m_statsCount{0};
    double m_statsSumMsec{0.0};
    uint64_t m_lastFramesDropped{0};
    QTimer m_statusTimer;
    std::chrono::steady_clock::time_point m_lastStatusTime;
};
