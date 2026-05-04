/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ipc-iox-private.h"

#include <iox2/iceoryx2.hpp>

namespace fs = std::filesystem;

namespace Syntalos::ipc
{

std::string makeModuleServiceName(const std::string &instanceId, const std::string &channelName)
{
    // the total resulting length of this string must not be longer than 255 characters, because
    // that is the length set for IDs in SY_IOX_ID_MAX_LEN
    std::string svcId = "Sy/" + instanceId.substr(0, 120) + "/" + channelName.substr(0, 128);
    assert(svcId.length() <= SY_IOX_ID_MAX_LEN);
    return svcId;
}

void findAndCleanupDeadNodes()
{
    iox2::Node<iox2::ServiceType::Ipc>::list(ioxDefaultConfig().global_config(), [](auto node_state) -> auto {
        node_state.dead([](auto view) -> auto {
            std::cout << "ipc: Detected dead node: ";
            if (view.details().has_value()) {
                std::cout << view.details().value().name().to_string().unchecked_access().c_str();
            }
            std::cout << std::endl;
            IOX2_DISCARD_RESULT(view.try_remove_stale_resources());
        });
        return iox2::CallbackProgression::Continue;
    }).value();
}

const iox2::Config &ioxDefaultConfig()
{
    static const auto config = [] {
        auto cfg = iox2::Config();

        fs::path runtimeDir;
        if (const char *env_p = std::getenv("XDG_RUNTIME_DIR")) {
            runtimeDir = fs::path(env_p);
            if (!fs::is_directory(runtimeDir))
                runtimeDir = fs::path("/tmp");
        } else {
            runtimeDir = fs::path("/tmp");
        }

        const auto rootPath =
            iox2::bb::StaticString<iox2::bb::platform::IOX2_MAX_PATH_LENGTH>::from_utf8_null_terminated_unchecked(
                (runtimeDir / "syntalos-iox").c_str())
                .value();

        cfg.global().set_root_path(iox2::bb::Path::create(rootPath).value());
        cfg.global().set_prefix(iox2::bb::FileName::create("sy_").value());

        cfg.global().node().set_cleanup_dead_nodes_on_creation(true);
        cfg.global().node().set_cleanup_dead_nodes_on_destruction(true);

        cfg.defaults().publish_subscribe().set_unable_to_deliver_strategy(
            iox2::UnableToDeliverStrategy::RetryUntilDelivered);
        cfg.defaults().request_response().set_client_unable_to_deliver_strategy(
            iox2::UnableToDeliverStrategy::RetryUntilDelivered);
        cfg.defaults().request_response().set_server_unable_to_deliver_strategy(
            iox2::UnableToDeliverStrategy::RetryUntilDelivered);

        return cfg;
    }();

    return config;
}

} // namespace Syntalos::ipc
