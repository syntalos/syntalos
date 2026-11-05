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

#include "imageadapter.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <opencv2/imgproc.hpp>

namespace OnnxInf
{

namespace
{

double channelValue(const std::vector<double> &list, int channel)
{
    if (list.size() == 1)
        return list[0];
    return list[static_cast<size_t>(channel)];
}

} // namespace

const std::vector<ParamSpec> &ImageAdapter::paramSpecs()
{
    static const std::vector<ParamSpec> specs = {
        {
         .key = "layout",
         .type = ParamType::Choice,
         .title = "Layout",
         .help = "Order of the tensor dimensions the model expects.",
         .defaultValue = std::string("nchw"),
         .choices = {{"nchw", "NCHW (channels first)"}, {"nhwc", "NHWC (channels last)"}},
         },
        {
         .key = "color",
         .type = ParamType::Choice,
         .title = "Colour",
         .help = "Colour order of the model input. Colour frames are converted, grayscale frames are expanded.",
         .defaultValue = std::string("rgb"),
         .choices = {{"rgb", "RGB"}, {"bgr", "BGR"}, {"gray", "Grayscale"}},
         },
        {
         .key = "resize",
         .type = ParamType::Choice,
         .title = "Resize",
         .help = "How frames are fitted to the model input size. Results are mapped back to frame coordinates.",
         .defaultValue = std::string("letterbox"),
         .choices =
                {{"letterbox", "Letterbox (keep aspect ratio, pad)"},
                 {"stretch", "Stretch"},
                 {"crop", "Center crop (keep aspect ratio)"},
                 {"none", "None (frames must match the input size)"}},
         },
        {
         .key = "size",
         .type = ParamType::Size,
         .title = "Input size",
         .help = "Width and height of the model input. Only used where the model does not fix them itself.",
         .defaultValue = std::vector<int64_t>{640, 640},
         .minValue = 1,
         .maxValue = 16384,
         },
        {
         .key = "pad_value",
         .type = ParamType::Int,
         .title = "Padding value",
         .help = "Gray value of the letterbox borders, in 8-bit units.",
         .defaultValue = int64_t{114},
         .minValue = 0,
         .maxValue = 255,
         },
        {
         .key = "normalize",
         .type = ParamType::Choice,
         .title = "Normalisation",
         .help = "How pixel values are scaled for float inputs. Integer inputs always get raw pixel values.",
         .defaultValue = std::string("0-1"),
         .choices =
                {{"none", "None (raw pixel values)"},
                 {"0-1", "Scale to 0…1"},
                 {"mean-std", "Scale to 0…1, subtract mean, divide by std"}},
         },
        {
         .key = "mean",
         .type = ParamType::FloatList,
         .title = "Mean",
         .help = "Mean per channel in 0…1 units, for mean-std normalisation. One value applies to all channels.",
         .defaultValue = std::vector<double>{0.485, 0.456, 0.406},
         },
        {
         .key = "std",
         .type = ParamType::FloatList,
         .title = "Std",
         .help = "Standard deviation per channel in 0…1 units, for mean-std normalisation.",
         .defaultValue = std::vector<double>{0.229, 0.224, 0.225},
         },
        {
         .key = "bit_depth",
         .type = ParamType::Int,
         .title = "Bit depth",
         .help = "Significant bits of 16-bit frames, e.g. 12 for a 12-bit camera. 0 uses all 16 bits.",
         .defaultValue = int64_t{0},
         .minValue = 0,
         .maxValue = 16,
         },
    };
    return specs;
}

ImageAdapter::ImageAdapter(const ParamSet &params)
{
    const auto &layout = params.getString("layout");
    m_layout = layout == "nhwc" ? Layout::NHWC : Layout::NCHW;

    const auto &color = params.getString("color");
    if (color == "bgr")
        m_color = Color::BGR;
    else if (color == "gray")
        m_color = Color::Gray;
    else
        m_color = Color::RGB;

    const auto &resize = params.getString("resize");
    if (resize == "stretch")
        m_resize = Resize::Stretch;
    else if (resize == "crop")
        m_resize = Resize::Crop;
    else if (resize == "none")
        m_resize = Resize::None;
    else
        m_resize = Resize::Letterbox;

    const auto &normalize = params.getString("normalize");
    if (normalize == "none")
        m_normalize = Normalize::None;
    else if (normalize == "mean-std")
        m_normalize = Normalize::MeanStd;
    else
        m_normalize = Normalize::ZeroOne;

    const auto size = params.getSize("size");
    m_recipeSize = cv::Size(static_cast<int>(size[0]), static_cast<int>(size[1]));
    m_padValue = static_cast<int>(params.getInt("pad_value"));
    m_bitDepth = static_cast<int>(params.getInt("bit_depth"));
    m_mean = params.getFloatList("mean");
    m_std = params.getFloatList("std");
}

auto ImageAdapter::configure(const TensorInfo &modelInput, std::optional<cv::Size> streamFrameSize)
    -> std::expected<void, std::string>
{
    m_configured = false;
    m_notes.clear();

    const auto &shape = modelInput.shape;
    if (shape.size() != 4)
        return std::unexpected(
            std::format(
                "The image adapter needs a model input with 4 dimensions, but '{}' has the shape {}.",
                modelInput.name,
                shapeToString(shape)));

    if (modelInput.dtype != DType::Float32 && modelInput.dtype != DType::UInt8)
        return std::unexpected(
            std::format(
                "Model input '{}' is {}. The image adapter supports float32 and uint8 inputs.",
                modelInput.name,
                dtypeToString(modelInput.dtype)));
    if (modelInput.dtype == DType::UInt8 && m_normalize != Normalize::None)
        return std::unexpected(
            std::format(
                "Model input '{}' is uint8 and takes raw pixel values. Set the normalisation to 'none'.",
                modelInput.name));

    const bool nchw = m_layout == Layout::NCHW;
    const int64_t dimN = shape[0];
    const int64_t dimC = nchw ? shape[1] : shape[3];
    const int64_t dimH = nchw ? shape[2] : shape[1];
    const int64_t dimW = nchw ? shape[3] : shape[2];

    if (dimN > 1)
        return std::unexpected(
            std::format(
                "Model input '{}' has a fixed batch size of {}, the image adapter only feeds single frames.",
                modelInput.name,
                dimN));

    const int colorChannels = m_color == Color::Gray ? 1 : 3;
    if (dimC > 0 && dimC != 1 && dimC != 3)
        return std::unexpected(
            std::format(
                "Model input '{}' would have {} channels in {} layout. Is the layout right?",
                modelInput.name,
                dimC,
                nchw ? "NCHW" : "NHWC"));
    if (dimC > 0 && dimC != colorChannels)
        return std::unexpected(
            std::format(
                "Model input '{}' has {} channel(s), but the colour setting gives {}.",
                modelInput.name,
                dimC,
                colorChannels));
    m_channels = colorChannels;

    cv::Size base = m_recipeSize;
    if (m_resize == Resize::None && streamFrameSize.has_value() && !streamFrameSize->empty())
        base = *streamFrameSize;
    m_size = cv::Size(dimW > 0 ? static_cast<int>(dimW) : base.width, dimH > 0 ? static_cast<int>(dimH) : base.height);
    if ((dimW > 0 || dimH > 0) && m_size != base && m_resize != Resize::None)
        m_notes.push_back(
            std::format(
                "The model fixes its input size to {}x{}, the recipe's {}x{} is not used.",
                m_size.width,
                m_size.height,
                base.width,
                base.height));

    if (m_normalize == Normalize::MeanStd) {
        const auto validList = [this](const std::vector<double> &list) {
            return list.size() == 1 || list.size() == static_cast<size_t>(m_channels);
        };
        if (!validList(m_mean) || !validList(m_std))
            return std::unexpected(
                std::format("Mean and std need either one value or one per channel ({}).", m_channels));
        for (const auto s : m_std) {
            if (s == 0.0)
                return std::unexpected("A standard deviation of zero can not be used for normalisation.");
        }
    }

    m_dtype = modelInput.dtype;
    if (nchw)
        m_shape = {1, m_channels, m_size.height, m_size.width};
    else
        m_shape = {1, m_size.height, m_size.width, m_channels};

    m_configured = true;
    return {};
}

const std::vector<std::string> &ImageAdapter::notes() const
{
    return m_notes;
}

const std::vector<int64_t> &ImageAdapter::inputShape() const
{
    return m_shape;
}

DType ImageAdapter::inputDType() const
{
    return m_dtype;
}

cv::Size ImageAdapter::modelSize() const
{
    return m_size;
}

auto ImageAdapter::resizeImage(const cv::Mat &src, ImageGeometry &geometry) -> std::expected<cv::Mat, std::string>
{
    const int w = m_size.width;
    const int h = m_size.height;
    const int sw = src.cols;
    const int sh = src.rows;

    switch (m_resize) {
    case Resize::None:
        if (sw != w || sh != h)
            return std::unexpected(
                std::format(
                    "Frame size {}x{} does not match the model input size {}x{}, and resizing is off.",
                    sw,
                    sh,
                    w,
                    h));
        return src;

    case Resize::Stretch:
        geometry.scaleX = static_cast<double>(w) / sw;
        geometry.scaleY = static_cast<double>(h) / sh;
        if (sw == w && sh == h)
            return src;
        cv::resize(src, m_resized, m_size, 0, 0, cv::INTER_LINEAR);
        return m_resized;

    case Resize::Letterbox: {
        const double r = std::min(static_cast<double>(w) / sw, static_cast<double>(h) / sh);
        const int nw = std::clamp(static_cast<int>(std::lround(sw * r)), 1, w);
        const int nh = std::clamp(static_cast<int>(std::lround(sh * r)), 1, h);
        const int px = (w - nw) / 2;
        const int py = (h - nh) / 2;

        geometry.scaleX = static_cast<double>(nw) / sw;
        geometry.scaleY = static_cast<double>(nh) / sh;
        geometry.padX = px;
        geometry.padY = py;
        if (sw == w && sh == h)
            return src;

        double pad = m_padValue;
        if (src.depth() == CV_16U)
            pad = m_padValue / 255.0 * (m_bitDepth > 0 ? (1 << m_bitDepth) - 1 : 65535);
        m_resized.create(h, w, src.type());
        m_resized.setTo(cv::Scalar::all(pad));

        auto roi = m_resized(cv::Rect(px, py, nw, nh));
        if (nw == sw && nh == sh)
            src.copyTo(roi);
        else
            cv::resize(src, roi, roi.size(), 0, 0, cv::INTER_LINEAR);
        return m_resized;
    }

    case Resize::Crop: {
        const double r = std::max(static_cast<double>(w) / sw, static_cast<double>(h) / sh);
        const int cw = std::clamp(static_cast<int>(std::lround(w / r)), 1, sw);
        const int ch = std::clamp(static_cast<int>(std::lround(h / r)), 1, sh);
        const int cx = (sw - cw) / 2;
        const int cy = (sh - ch) / 2;

        geometry.scaleX = static_cast<double>(w) / cw;
        geometry.scaleY = static_cast<double>(h) / ch;
        geometry.padX = -cx * geometry.scaleX;
        geometry.padY = -cy * geometry.scaleY;

        const auto roi = src(cv::Rect(cx, cy, cw, ch));
        if (cw == w && ch == h)
            return roi;
        cv::resize(roi, m_resized, m_size, 0, 0, cv::INTER_LINEAR);
        return m_resized;
    }
    }

    return std::unexpected("Unknown resize mode.");
}

cv::Mat ImageAdapter::convertColor(const cv::Mat &img) const
{
    const int ch = img.channels();
    int code = -1;

    switch (m_color) {
    case Color::Gray:
        if (ch == 3)
            code = cv::COLOR_BGR2GRAY;
        else if (ch == 4)
            code = cv::COLOR_BGRA2GRAY;
        break;
    case Color::RGB:
        if (ch == 1)
            code = cv::COLOR_GRAY2RGB;
        else if (ch == 3)
            code = cv::COLOR_BGR2RGB;
        else if (ch == 4)
            code = cv::COLOR_BGRA2RGB;
        break;
    case Color::BGR:
        if (ch == 1)
            code = cv::COLOR_GRAY2BGR;
        else if (ch == 4)
            code = cv::COLOR_BGRA2BGR;
        break;
    }

    if (code < 0)
        return img;
    cv::Mat converted;
    cv::cvtColor(img, converted, code);
    return converted;
}

auto ImageAdapter::process(const cv::Mat &image, Tensor &out, ImageGeometry &geometry)
    -> std::expected<void, std::string>
{
    if (!m_configured)
        return std::unexpected("The image adapter was not configured.");
    if (image.empty())
        return std::unexpected("Received an empty frame.");
    if (image.depth() != CV_8U && image.depth() != CV_16U)
        return std::unexpected("Only frames with 8 or 16 bits per channel are supported.");
    if (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)
        return std::unexpected(std::format("Frames with {} channels are not supported.", image.channels()));

    geometry = ImageGeometry{
        .sourceWidth = image.cols,
        .sourceHeight = image.rows,
        .modelWidth = m_size.width,
        .modelHeight = m_size.height,
    };

    auto resized = resizeImage(image, geometry);
    if (!resized)
        return std::unexpected(resized.error());
    const auto img = convertColor(*resized);

    const double maxValue = img.depth() == CV_8U ? 255.0 : (m_bitDepth > 0 ? (1 << m_bitDepth) - 1 : 65535.0);
    const int w = m_size.width;
    const int h = m_size.height;
    const size_t planeSize = static_cast<size_t>(w) * h;

    out.reset(m_dtype, m_shape);

    if (m_dtype == DType::UInt8) {
        cv::Mat img8;
        if (img.depth() == CV_8U)
            img8 = img;
        else
            img.convertTo(img8, CV_8U, 255.0 / maxValue);

        auto data = out.dataAs<uint8_t>();
        if (m_layout == Layout::NHWC || m_channels == 1) {
            cv::Mat dst(h, w, CV_8UC(m_channels), data);
            img8.copyTo(dst);
        } else {
            std::vector<cv::Mat> planes;
            for (int c = 0; c < m_channels; ++c)
                planes.emplace_back(h, w, CV_8UC1, data + c * planeSize);
            cv::split(img8, planes.data());
        }
        return {};
    }

    // float32 input
    const double scale = m_normalize == Normalize::None ? 1.0 : 1.0 / maxValue;
    auto data = out.dataAs<float>();
    if (m_layout == Layout::NHWC || m_channels == 1) {
        cv::Mat dst(h, w, CV_32FC(m_channels), data);
        img.convertTo(dst, CV_32F, scale);
        if (m_normalize == Normalize::MeanStd) {
            cv::Scalar mean, stdDev;
            for (int c = 0; c < m_channels; ++c) {
                mean[c] = channelValue(m_mean, c);
                stdDev[c] = channelValue(m_std, c);
            }
            cv::subtract(dst, mean, dst);
            cv::divide(dst, stdDev, dst);
        }
        if (dst.data != reinterpret_cast<uchar *>(data))
            return std::unexpected("Internal error: the input tensor buffer was reallocated.");
    } else {
        img.convertTo(m_floatImg, CV_32F, scale);
        std::vector<cv::Mat> planes;
        for (int c = 0; c < m_channels; ++c)
            planes.emplace_back(h, w, CV_32FC1, data + c * planeSize);
        cv::split(m_floatImg, planes.data());
        if (m_normalize == Normalize::MeanStd) {
            for (int c = 0; c < m_channels; ++c) {
                const double s = channelValue(m_std, c);
                planes[c].convertTo(planes[c], -1, 1.0 / s, -channelValue(m_mean, c) / s);
            }
        }
    }

    return {};
}

} // namespace OnnxInf
