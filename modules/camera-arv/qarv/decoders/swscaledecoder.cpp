/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "swscaledecoder.h"
#include <cstdlib>
#include <opencv2/core/types_c.h>
#include "../qarv-globals.h"

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
}

using namespace QArv;

SwScaleDecoder::SwScaleDecoder(QSize size_, AVPixelFormat inputPixfmt_,
                               ArvPixelFormat arvPixFmt, int swsFlags) :
    size(size_),
    inputPixfmt(inputPixfmt_), arvPixelFormat(arvPixFmt), flags(swsFlags) {
    if (size.width() != (size.width() / 2) * 2
        || size.height() != (size.height() / 2) * 2) {
        logMessage() << "Frame size must be factor of two for SwScaleDecoder.";
        OK = false;
        return;
    }
    if (sws_isSupportedInput(inputPixfmt) > 0) {
        int bitsPerPixel =
            av_get_bits_per_pixel(av_pix_fmt_desc_get(inputPixfmt));
        uint8_t components = av_pix_fmt_desc_get(inputPixfmt)->nb_components;
        if (bitsPerPixel / components > 8) {
            if (components == 1) {
                outputPixFmt = AV_PIX_FMT_GRAY16;
                cvMatType = CV_16UC1;
                bufferBytesPerPixel = 2;
            } else {
                outputPixFmt = AV_PIX_FMT_BGR48;
                cvMatType = CV_16UC3;
                bufferBytesPerPixel = 6;
            }
        } else {
            if (components == 1) {
                outputPixFmt = AV_PIX_FMT_GRAY8;
                cvMatType = CV_8UC1;
                bufferBytesPerPixel = 1;
            } else {
                outputPixFmt = AV_PIX_FMT_BGR24;
                cvMatType = CV_8UC3;
                bufferBytesPerPixel = 3;
            }
        }
        OK = 0 < av_image_alloc(image_pointers, image_strides, size.width(),
                                size.height(), outputPixFmt, 16);
        if (OK)
            ctx = sws_getContext(size.width(), size.height(), inputPixfmt,
                                 size.width(), size.height(), outputPixFmt,
                                 flags, 0, 0, 0);
    } else {
        logMessage() << "Pixel format" << av_get_pix_fmt_name(inputPixfmt)
                     << "is not supported for input.";
        OK = false;
    }
}

SwScaleDecoder::~SwScaleDecoder() {
    if (OK) {
        sws_freeContext(ctx);
        av_freep(&image_pointers[0]);
    }
}

ArvPixelFormat SwScaleDecoder::pixelFormat() {
    return arvPixelFormat;
}

AVPixelFormat SwScaleDecoder::swscalePixelFormat() {
    return inputPixfmt;
}

int SwScaleDecoder::cvType() {
    return cvMatType;
}

void SwScaleDecoder::decode(QByteArray frame) {
    if (!OK) return;
    auto dataptr = reinterpret_cast<const uint8_t*>(frame.constData());
    av_image_fill_arrays(srcInfo.data, srcInfo.linesize,
                         const_cast<uint8_t*>(dataptr),
                         inputPixfmt, size.width(), size.height(), 1);
    int outheight = sws_scale(ctx, srcInfo.data, srcInfo.linesize,
                              0, size.height(),
                              image_pointers, image_strides);
    if (outheight != size.height()) {
        logMessage() << "swscale error! outheight =" << outheight;
    }
}

const cv::Mat SwScaleDecoder::getCvImage() {
    if (!OK) return cv::Mat();
    cv::Mat M(size.height(), size.width(), cvMatType,
              image_pointers[0], image_strides[0]);
    return M;
}

QByteArray SwScaleDecoder::decoderSpecification() {
    QByteArray b;
    QDataStream s(&b, QIODevice::WriteOnly);
    s << QString("SwScale") << size << (qlonglong)outputPixFmt << flags;
    return b;
}
