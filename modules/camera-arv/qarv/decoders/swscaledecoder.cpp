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
#include <opencv2/core/types_c.h>
#include "../qarv-globals.h"

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
}

using namespace QArv;

SwScaleDecoder::SwScaleDecoder(QSize size_, AVPixelFormat inputPixfmt_,
                               ArvPixelFormat arvPixFmt, int swsFlags) :
    OK(false),
    size(size_),
    ctx(nullptr),
    cvMatType(-1),
    inputPixfmt(inputPixfmt_),
    outputPixFmt(AV_PIX_FMT_NONE),
    srcInfo{},
    arvPixelFormat(arvPixFmt),
    flags(swsFlags) {
    if (size.width() != (size.width() / 2) * 2
        || size.height() != (size.height() / 2) * 2) {
        qDebug().noquote() << "Frame size must be factor of two for SwScaleDecoder.";
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
            } else {
                outputPixFmt = AV_PIX_FMT_BGR48;
                cvMatType = CV_16UC3;
            }
        } else {
            if (components == 1) {
                outputPixFmt = AV_PIX_FMT_GRAY8;
                cvMatType = CV_8UC1;
            } else {
                outputPixFmt = AV_PIX_FMT_BGR24;
                cvMatType = CV_8UC3;
            }
        }
        ctx = sws_getContext(size.width(), size.height(), inputPixfmt,
                             size.width(), size.height(), outputPixFmt,
                             flags, nullptr, nullptr, nullptr);
        OK = ctx != nullptr;
    } else {
        qDebug().noquote() << "Pixel format" << av_get_pix_fmt_name(inputPixfmt)
                     << "is not supported for input.";
    }
}

SwScaleDecoder::~SwScaleDecoder() {
    if (ctx)
        sws_freeContext(ctx);
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

void SwScaleDecoder::decodeInto(QByteArrayView frame, cv::Mat &output) {
    if (!OK) {
        output.release();
        return;
    }

    output.create(size.height(), size.width(), cvMatType);
    auto dataptr = reinterpret_cast<const uint8_t*>(frame.constData());
    av_image_fill_arrays(srcInfo.data, srcInfo.linesize,
                         const_cast<uint8_t*>(dataptr),
                         inputPixfmt, size.width(), size.height(), 1);
    uint8_t *outputPointers[4] = {output.data, nullptr, nullptr, nullptr};
    const int outputStrides[4] = {static_cast<int>(output.step[0]), 0, 0, 0};
    int outheight = sws_scale(ctx, srcInfo.data, srcInfo.linesize,
                              0, size.height(),
                              outputPointers, outputStrides);
    if (outheight != size.height()) {
        qDebug().noquote() << "swscale error! outheight =" << outheight;
    }
}

QByteArray SwScaleDecoder::decoderSpecification() {
    QByteArray b;
    QDataStream s(&b, QIODeviceBase::WriteOnly);
    s << QString("SwScale") << size << (qlonglong)outputPixFmt << flags;
    return b;
}
