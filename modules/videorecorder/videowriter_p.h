/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 */

#ifndef VIDEOWRITER_P_H
#define VIDEOWRITER_P_H

#include <algorithm>
#include <optional>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "videowriter.h"

namespace VideoWriterPrivate
{

using EncoderByNameFinder = const AVCodec *(*)(const char *name);
using EncoderByIdFinder = const AVCodec *(*)(AVCodecID id);

const AVCodec *findSoftwareEncoder(VideoCodec codec, EncoderByNameFinder findByName, EncoderByIdFinder findById);
bool encoderSupportsPreset(VideoCodec codec, const AVCodec *encoder);

struct PresetConfig {
    int minimum;
    int maximum;
    int defaultValue;

    int clamp(int value) const
    {
        return std::clamp(value, minimum, maximum);
    }
};

PresetConfig svtAv1PresetConfig(
    const char *runtimeVersion,
    std::optional<std::pair<int, int>> optionRange = std::nullopt);

} // namespace VideoWriterPrivate

#endif // VIDEOWRITER_P_H
