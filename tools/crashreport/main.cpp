/*
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "crashreportdialog.h"

#include "config.h"
#include <QApplication>
#include <QCommandLineParser>

#include "appstyle.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("syntalos-crash-reporter");
    app.setOrganizationName("DraguhnLab");
    app.setOrganizationDomain("draguhnlab.com");
    app.setApplicationVersion(PROJECT_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Automatically collect debug information about Syntalos");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption freezeDebugOption("debug-freeze", "Generate debug info about a frozen Syntalos instance");
    parser.addOption(freezeDebugOption);

    parser.process(app);

    // set our operating mode
    auto mode = ReportMode::COLLECT_CRASH_INFO;
    if (parser.isSet(freezeDebugOption))
        mode = ReportMode::DEBUG_FREEZE;

    // set Syntalos default style
    setDefaultStyle();
    switchIconTheme(QStringLiteral("breeze"));

    // finally show the dialog window
    CrashReportDialog w(mode);
    w.show();
    return app.exec();
}
