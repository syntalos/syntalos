/*
 * Copyright (C) 2012-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"
#include <KDBusService>
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <gst/gst.h>
#include <pipewire/pipewire.h>
#pragma GCC diagnostic pop

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // set random seed
    srand(static_cast<uint>(time(nullptr)));

    // initialize PipeWire
    pw_init(&argc, &argv);

    // initialize GStreamer so modules can use it if they need to
    gst_init(&argc, &argv);

    // set up GUI application and application details
    QApplication app(argc, argv);
    app.setApplicationName("Syntalos");
    app.setApplicationVersion(syntalosVersionFull());

    // we deliberately do not set an organization, so QSettings
    // and other will use the app name only
    app.setOrganizationName(QString());
    app.setOrganizationDomain(QString());

    // parse command-line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Syntalos");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("project", QStringLiteral("Syntalos project file to open on startup."), "[project]");

    QCommandLineOption outputDirOption(
        QStringList() << "o" << "export-dir",
        QStringLiteral("Override the data export base directory set in the project file."),
        "directory");
    parser.addOption(outputDirOption);

    QCommandLineOption autoRunOption(
        QStringList() << "r" << "run",
        QStringLiteral("Automatically start a run immediately after the project has been loaded."));
    parser.addOption(autoRunOption);

    QCommandLineOption runForOption(
        QStringList() << "t" << "run-for",
        QStringLiteral(
            "Automatically start a run and stop it after the given number of seconds, "
            "then quit the application. Implies --run."),
        "seconds");
    parser.addOption(runForOption);

    QCommandLineOption noninteractiveOption(
        QStringList() << "n" << "non-interactive",
        QStringLiteral(
            "Try to reduce GUI user interactions when auto-running a project file, print to stderr instead."));
    parser.addOption(noninteractiveOption);

    parser.process(app);

    // fetch project filename to open
    const auto positionalArgs = parser.positionalArguments();
    const auto projectFname = positionalArgs.isEmpty() ? QString() : positionalArgs.last();

    // fetch automation / testing options
    const int runForSecs = parser.isSet(runForOption) ? parser.value(runForOption).toInt() : -1;
    const bool autoRun = parser.isSet(autoRunOption) || runForSecs > 0;

    // ensure we only ever run one instance of the application
    KDBusService service(KDBusService::Unique);

    // launch Syntalos with the provided options
    auto w = std::make_unique<MainWindow>();
    w->show();
    if (autoRun) {
        // automation-specific options
        const auto overrideExportDir = parser.value(outputDirOption);
        const bool nonInteractive = parser.isSet(noninteractiveOption);

        if (projectFname.isEmpty()) {
            qCritical().noquote()
                << "No project filename specified, despite requesting autorun. Please specify a project file to run.";
            return SY_EXIT_LOAD_ERROR;
        }
        w->scheduleProjectAutorun(projectFname, overrideExportDir, nonInteractive, runForSecs);
    } else {
        if (!projectFname.isEmpty())
            w->loadProjectFilename(projectFname);
    }

    // run application & return result
    auto rc = app.exec();
    return rc;
}
