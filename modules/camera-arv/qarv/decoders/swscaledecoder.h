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

#ifndef FFMPEG_DECODER_H
#define FFMPEG_DECODER_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include <QSize>
#include <QImage>
#include "../qarvdecoder.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}

namespace QArv
{

/*
 * This decoder uses libswscale to convert directly into a caller-owned
 * cv::Mat.
 */
class SwScaleDecoder : public QArvDecoder {
public:
    SwScaleDecoder(QSize size,
                   enum AVPixelFormat inputPixfmt,
                   ArvPixelFormat arvPixFmt,
                   int swsFlags = SWS_FAST_BILINEAR | SWS_BITEXACT);
    ~SwScaleDecoder() override;
    void decodeInto(QByteArrayView frame, cv::Mat &output) override;
    int cvType() override;
    ArvPixelFormat pixelFormat() override;
    QByteArray decoderSpecification() override;
    enum AVPixelFormat swscalePixelFormat();

private:
    bool OK;
    QSize size;
    struct SwsContext* ctx;
    int cvMatType;
    enum AVPixelFormat inputPixfmt, outputPixFmt;
    struct AVFrame srcInfo;
    ArvPixelFormat arvPixelFormat;
    int flags;
};

}

#endif
