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

static constexpr uint32_t ONE_KILOBYTE = 1024U;
static constexpr uint32_t ONE_MEGABYTE = 1024U * 1024;

int main(int argc, char *argv[])
{
    using iox::roudi::IceOryxRouDiApp;

    iox::config::CmdLineArgs_t roudiArgs;

    // disable monitoring for now, as RouDi is far too eager to kill live processes
    // under very high CPU load, which may happen with Syntalos
    roudiArgs.monitoringMode = iox::roudi::MonitoringMode::OFF;

    // set other defaults
    roudiArgs.logLevel = iox::log::LogLevel::kWarn;
    roudiArgs.compatibilityCheckLevel = iox::version::CompatibilityCheckLevel::PATCH;
    roudiArgs.processKillDelay = iox::units::Duration::fromSeconds(90);
    roudiArgs.run = true;

    // tear down the daemon if our main process dies
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // set a default configuration that works for Syntalos
    iox::RouDiConfig_t roudiConfig;
    iox::mepoo::MePooConfig mpConfig;

    mpConfig.addMemPool({ONE_KILOBYTE, 50});
    mpConfig.addMemPool({ONE_KILOBYTE * 512, 50});
    mpConfig.addMemPool({ONE_MEGABYTE, 20});
    mpConfig.addMemPool({ONE_MEGABYTE * 6, 20});
    mpConfig.addMemPool({ONE_MEGABYTE * 24, 10});

    /// use the Shared Memory Segment for the current user
    auto currentGroup = iox::posix::PosixGroup::getGroupOfCurrentProcess();

    // create an Entry for a new Shared Memory Segment from the MempoolConfig and add it to the RouDiConfig
    roudiConfig.m_sharedMemorySegments.push_back({currentGroup.getName(), currentGroup.getName(), mpConfig});

    // execute RouDi
    IceOryxRouDiApp roudi(roudiArgs, roudiConfig);
    return roudi.run();
}
