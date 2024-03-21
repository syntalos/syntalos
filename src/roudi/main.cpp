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

#include <signal.h>
#include <sys/prctl.h>

#include <iceoryx_posh/iceoryx_posh_config.hpp>
#include <iceoryx_posh/internal/log/posh_logging.hpp>
#include <iceoryx_posh/roudi/iceoryx_roudi_app.hpp>
#include <iceoryx_posh/roudi/roudi_cmd_line_parser_config_file_option.hpp>

int main(int argc, char *argv[])
{
    using iox::roudi::IceOryxRouDiApp;

    iox::config::CmdLineParserConfigFileOption cmdLineParser;
    auto cmdLineArgs = cmdLineParser.parse(argc, argv);
    if (cmdLineArgs.has_error() && (cmdLineArgs.get_error() != iox::config::CmdLineParserResult::INFO_OUTPUT_ONLY)) {
        iox::LogFatal() << "Unable to parse command line arguments!";
        return EXIT_FAILURE;
    }

    // tear down the daemon if our main process dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // use default configuration for now
    iox::RouDiConfig_t roudiConfig;
    roudiConfig.setDefaults();

    // execute RouDi
    IceOryxRouDiApp roudi(cmdLineArgs.value(), roudiConfig);
    return roudi.run();
}
