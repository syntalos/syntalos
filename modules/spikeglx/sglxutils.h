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

#include <QString>
#include <QStringList>
#include <expected>
#include <optional>
#include <vector>

#include "sglxclient.h"

/**
 * Pure helpers for the SpikeGLX module: stream naming, channel-group
 * layout, channel-subset parsing and run-name templating.
 */
namespace SglxUtils
{

/// Human stream name: "nidq", "obx<ip>", "imec<ip>".
QString streamName(Sglx::StreamId sid);
std::optional<Sglx::StreamId> parseStreamName(const QString &name);

/**
 * @brief Channel groups as laid out in a SpikeGLX stream.
 *
 * IM streams are ordered {AP, LF, SY}, OneBox streams {XA, DW, SY}
 * and NI streams {MN, MA, XA, DW}.
 */
enum class ChanGroup {
    ALL,
    AP,
    LF,
    SY,
    XA,
    DW,
    MN,
    MA
};

QString chanGroupName(ChanGroup group);
std::optional<ChanGroup> parseChanGroup(const QString &name);
/// Channel groups available for a stream type, in stream order (without ALL).
std::vector<ChanGroup> chanGroupsForStream(int js);

/// True for groups carrying digital/sync words rather than analog samples.
bool isDigitalGroup(ChanGroup group);

/**
 * @brief Resolve a channel group to an [offset, count) range within the stream.
 * @param acqCounts Per-type channel counts as returned by GETSTREAMACQCHANS.
 */
std::expected<std::pair<int, int>, QString> chanGroupRange(int js, const std::vector<int> &acqCounts, ChanGroup group);

/**
 * @brief Parse a channel subset like "0:31,40,64:95" (inclusive ranges).
 *
 * Indices are relative to the group; an empty spec (or "all") selects
 * every channel. The result is sorted and duplicate-free.
 */
std::expected<std::vector<int>, QString> parseChannelSpec(const QString &spec, int groupChanCount);

/// Compact string form of a channel index list, e.g. "0:31,40".
QString channelListToSpec(const std::vector<int> &chans);

/**
 * @brief Make a string acceptable as SpikeGLX run name.
 *
 * SpikeGLX rejects the characters /\[]<>*":;,?|= and interprets a
 * trailing _g<N>_t<M> as gate/trigger start indices. Offending characters
 * are replaced with underscores and a reserved suffix is neutralized.
 */
QString sanitizeRunName(const QString &name);

/// True if the name ends in the reserved _g<N>_t<M> pattern.
bool hasReservedGTSuffix(const QString &name);

} // namespace SglxUtils
