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

#include <iostream>
#include <QApplication>
#include <syntalos-mlink>

#include "onnxinfermodule.h"
#include "ortengine.h"

using namespace Syntalos;

int main(int argc, char *argv[])
{
    // the settings dialog lives in this process, so we need a Qt application first
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("syntalos-onnx-inference"));
    app.setQuitOnLastWindowClosed(false);

    OnnxInf::OrtEngine::setLogHandler([](OnnxInf::EngineLogLevel level, const std::string &message) {
        const char *prefix = level == OnnxInf::EngineLogLevel::Error     ? "error"
                             : level == OnnxInf::EngineLogLevel::Warning ? "warning"
                                                                         : "info";
        std::cerr << "ONNX Runtime " << prefix << ": " << message << std::endl;
    });

    // Initialize link to Syntalos. There can only be one.
    std::unique_ptr<SyntalosLink> slink;
    try {
        slink = initSyntalosModuleLink({.renameThread = true});
    } catch (const std::exception &e) {
        std::cerr << "Failed to initialize Syntalos module link: " << e.what() << std::endl;
        return 5;
    }

    auto mod = std::make_unique<OnnxInferenceModule>(slink.get());

    // IPC events and work posted by the inference thread wake us up immediately,
    // Qt events are handled after each wake-up and at least every 20 ms
    slink->awaitDataForever(
        []() {
            QCoreApplication::processEvents();
        },
        20 * 1000);

    mod.reset();
    return 0;
}
