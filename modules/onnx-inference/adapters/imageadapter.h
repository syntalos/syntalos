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

#include <expected>
#include <optional>
#include <string>
#include <vector>
#include <opencv2/core.hpp>

#include "ortengine.h"
#include "params.h"
#include "tensor.h"

namespace OnnxInf
{

/**
 * How a source image was mapped onto the model input.
 *
 * Model coordinates map back to the source with x_src = (x_model - padX) / scaleX,
 * the same for y. Cropping shows up as negative padding.
 */
struct ImageGeometry {
    double scaleX = 1.0;
    double scaleY = 1.0;
    double padX = 0.0;
    double padY = 0.0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int modelWidth = 0;
    int modelHeight = 0;

    [[nodiscard]] double toSourceX(double xModel) const
    {
        return (xModel - padX) / scaleX;
    }

    [[nodiscard]] double toSourceY(double yModel) const
    {
        return (yModel - padY) / scaleY;
    }
};

/**
 * Turns frames into an image tensor: resizing, colour conversion, normalisation and layout.
 */
class ImageAdapter
{
public:
    static const std::vector<ParamSpec> &paramSpecs();

    explicit ImageAdapter(const ParamSet &params);

    /**
     * Work out the input tensor's shape. Static model dimensions win over the recipe,
     * dynamic ones are taken from the recipe (or, for resize mode "none", from the
     * stream's frame size if it is known).
     */
    auto configure(const TensorInfo &modelInput, std::optional<cv::Size> streamFrameSize = std::nullopt)
        -> std::expected<void, std::string>;

    /**
     * Things the user should know about how the recipe was applied, e.g. ignored sizes.
     */
    [[nodiscard]] const std::vector<std::string> &notes() const;

    [[nodiscard]] const std::vector<int64_t> &inputShape() const;
    [[nodiscard]] DType inputDType() const;
    [[nodiscard]] cv::Size modelSize() const;

    /**
     * Convert one image into the input tensor. 8-bit and 16-bit images with
     * 1, 3 (BGR) or 4 (BGRA) channels are accepted.
     */
    auto process(const cv::Mat &image, Tensor &out, ImageGeometry &geometry) -> std::expected<void, std::string>;

    static std::string indexColumn()
    {
        return "frame_index";
    }

private:
    enum class Layout {
        NCHW,
        NHWC
    };
    enum class Color {
        RGB,
        BGR,
        Gray
    };
    enum class Resize {
        Letterbox,
        Stretch,
        Crop,
        None
    };
    enum class Normalize {
        None,
        ZeroOne,
        MeanStd
    };

    auto resizeImage(const cv::Mat &src, ImageGeometry &geometry) -> std::expected<cv::Mat, std::string>;
    cv::Mat convertColor(const cv::Mat &img) const;

    Layout m_layout;
    Color m_color;
    Resize m_resize;
    Normalize m_normalize;
    cv::Size m_recipeSize;
    int m_padValue;
    int m_bitDepth;
    std::vector<double> m_mean;
    std::vector<double> m_std;

    bool m_configured{false};
    DType m_dtype{DType::Unknown};
    int m_channels{3};
    cv::Size m_size;
    std::vector<int64_t> m_shape;
    std::vector<std::string> m_notes;

    cv::Mat m_resized;
    cv::Mat m_floatImg;
};

} // namespace OnnxInf
