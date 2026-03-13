/*
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "ipc-config-private.h"

#include <QStandardPaths>
#include <QDir>
#include <iox2/iceoryx2.hpp>

#include "ipc-types-private.h"

using namespace Syntalos;

static constexpr const char IOX_DEFAULT_CONFIG[] = R"([global]
root-path = "/tmp/syntalos-iox/"
prefix = "syiox_"

[global.node]
cleanup-dead-nodes-on-creation = true
cleanup-dead-nodes-on-destruction = true

[defaults.publish-subscribe]
max-subscribers = 16
max-publishers = 16
max-nodes = 48
subscriber-max-buffer-size = 48
subscriber-max-borrowed-samples = 8
publisher-max-loaned-samples = 8
publisher-history-size = 4
enable-safe-overflow = true
unable-to-deliver-strategy = "Block"
subscriber-expired-connection-buffer = 256

[defaults.event]
max-listeners = 128
max-notifiers = 128
max-nodes = 48
event-id-max-value = 255

[defaults.request-response]
enable-safe-overflow-for-requests = true
enable-safe-overflow-for-responses = true
max-active-requests-per-client = 8
max-response-buffer-size = 6
max-servers = 16
max-clients = 16
max-nodes = 48
max-borrowed-responses-per-pending-response = 8
max-loaned-requests = 8
server-max-loaned-responses-per-request = 8
client-unable-to-deliver-strategy = "Block"
server-unable-to-deliver-strategy = "Block"
client-expired-connection-buffer = 128
enable-fire-and-forget-requests = true
server-expired-connection-buffer = 128
)";

std::string ipc::makeModuleServiceName(const std::string &instanceId, const std::string &channelName)
{
    // the total resulting length of this string must not be longer than 255 characters, because
    // that is the length set for IDs in SY_IOX_ID_MAX_LEN
    std::string svcId = "Sy/" + instanceId.substr(0, 120) + "/" + channelName.substr(0, 128);
    assert(svcId.length() <= SY_IOX_ID_MAX_LEN);
    return svcId;
}

static void findAndCleanupDeadNodes()
{
    iox2::Node<iox2::ServiceType::Ipc>::list(iox2::Config::global_config(), [](auto node_state) -> auto {
        node_state.dead([](auto view) -> auto {
            std::cout << "ipc: Detected dead node: ";
            if (view.details().has_value()) {
                std::cout << view.details().value().name().to_string().unchecked_access().c_str();
            }
            std::cout << std::endl;
            IOX2_DISCARD_RESULT(view.remove_stale_resources().value());
        });
        return iox2::CallbackProgression::Continue;
    }).value();
}

std::expected<bool, std::string> ipc::setupIoxConfiguration()
{
    auto configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configDir.isEmpty())
        return std::unexpected("Failed to determine writable config directory");
    configDir = QStringLiteral("%1/Syntalos").arg(configDir);

    auto configPath = configDir + "/syntalos-iox.toml";
    if (!QFile::exists(configPath)) {
        QDir().mkpath(configDir);
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::WriteOnly))
            return std::unexpected("Failed to create IOX config file");
        if (configFile.write(IOX_DEFAULT_CONFIG, static_cast<qint64>(sizeof(IOX_DEFAULT_CONFIG) - 1))
            != static_cast<qint64>(sizeof(IOX_DEFAULT_CONFIG) - 1))
            return std::unexpected("Failed to write IOX config file");
    }

    qDebug().noquote() << "Using IOX config file at:" << configPath;
    auto ioxConfPathStr =
        iox2::bb::StaticString<iox2::bb::platform::IOX2_MAX_PATH_LENGTH>::from_utf8_null_terminated_unchecked(
            configPath.toStdString().c_str())
            .value();
    auto ioxConfPath = iox2::bb::FilePath::create(ioxConfPathStr).value();
    auto res = iox2::Config::setup_global_config_from_file(ioxConfPath);
    if (!res.has_value())
        return std::unexpected(iox2::bb::into<const char *>(res.error()));

    // cleanup
    findAndCleanupDeadNodes();

    return true;
}
