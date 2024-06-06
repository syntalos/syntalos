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

#include "qarvdecoder.h"
#include "decoders/swscaledecoder.h"
#include "decoders/graymap.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <QPluginLoader>
#include <QMap>
#include "qarv-globals.h"
#include <type_traits>
#include <arv.h>

using namespace QArv;

template<bool grayscale, bool depth8>
void CV2QImage_RGB24Template(const cv::Mat& image, QImage& img) {
    typedef typename std::conditional<depth8, uint8_t,
                                      uint16_t>::type InputType;
    const int h = image.rows, w = image.cols;
    QSize s = img.size();
    if (s.height() != h
        || s.width() != w
        || img.format() != QImage::Format_RGB888) {
        if (image.channels() == 1)
            img = QImage(w, h, QImage::Format_Indexed8);
        else
            img = QImage(w, h, QImage::Format_RGB888);
    }
    if (grayscale) {
        img.setColorTable(graymap);
        for (int i = 0; i < h; i++) {
            auto line = image.ptr<InputType>(i);
            auto I = img.scanLine(i);
            for (int j = 0; j < w; j++)
                if (depth8)
                    I[j] = line[j];
                else
                    I[j] = line[j] >> 8;
        }
    } else {
        for (int i = 0; i < h; i++) {
            auto imgLine = img.scanLine(i);
            auto imageLine = image.ptr<cv::Vec<InputType, 3> >(i);
            for (int j = 0; j < w; j++) {
                auto& bgr = imageLine[j];
                for (int px = 0; px < 3; px++) {
                    if (depth8)
                        imgLine[3*j + px] = bgr[2-px];
                    else
                        imgLine[3*j + px] = bgr[2-px] >> 8;
                }
            }
        }
    }
}

void QArvDecoder::CV2QImage_RGB24(const cv::Mat& image, QImage& out) {
    switch (image.type()) {
    case CV_16UC1:
        CV2QImage_RGB24Template<true, false>(image, out);
        break;

    case CV_16UC3:
        CV2QImage_RGB24Template<false, false>(image, out);
        break;

    case CV_8UC1:
        CV2QImage_RGB24Template<true, true>(image, out);
        break;

    case CV_8UC3:
        CV2QImage_RGB24Template<false, true>(image, out);
        break;

    default:
        logMessage() << "CV2QImage: Invalid CV image format";
        return;
    }
}

QImage QArvDecoder::CV2QImage_RGB24(const cv::Mat& image) {
    QImage img;
    CV2QImage_RGB24(image, img);
    return img;
}

template<bool grayscale, bool depth8>
void CV2QImageTemplate(const cv::Mat& image_, QImage& image) {
    typedef typename std::conditional<depth8, uint8_t,
                                      uint16_t>::type InputType;
    const int h = image_.rows, w = image_.cols;
    QSize s = image.size();
    if (s.height() != h
        || s.width() != w
        || image.format() != QImage::Format_ARGB32_Premultiplied)
        image = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    if (!grayscale) {
        for (int i = 0; i < h; i++) {
            auto imgLine = image.scanLine(i);
            auto imageLine = image_.ptr<cv::Vec<InputType, 3> >(i);
            for (int j = 0; j < w; j++) {
                auto& bgr = imageLine[j];
                for (int px = 0; px < 3; px++) {
                    if (depth8)
                        imgLine[4*j + 2 - px] = bgr[2-px];
                    else
                        imgLine[4*j + 2 - px] = bgr[2-px] >> 8;
                }
                imgLine[4*j + 3] = 255;
            }
        }
    } else {
        for (int i = 0; i < h; i++) {
            auto imgLine = image.scanLine(i);
            auto imageLine = image_.ptr<InputType>(i);
            for (int j = 0; j < w; j++) {
                uint8_t gray;
                if (depth8)
                    gray = imageLine[j];
                else
                    gray = imageLine[j] >> 8;
                for (int px = 0; px < 3; px++) {
                    imgLine[4*j + px] = gray;
                }
                imgLine[4*j + 3] = 255;
            }
        }
    }
}

void QArvDecoder::CV2QImage(const cv::Mat& image, QImage& out) {
    switch (image.type()) {
    case CV_16UC1:
        CV2QImageTemplate<true, false>(image, out);
        break;

    case CV_16UC3:
        CV2QImageTemplate<false, false>(image, out);
        break;

    case CV_8UC1:
        CV2QImageTemplate<true, true>(image, out);
        break;

    case CV_8UC3:
        CV2QImageTemplate<false, true>(image, out);
        break;

    default:
        logMessage() << "CV2QImage: Invalid CV image format";
        return;
    }
}

QImage QArvDecoder::CV2QImage(const cv::Mat& image) {
    QImage img;
    CV2QImage(image, img);
    return img;
}

static QList<QArvPixelFormat*> initPluginFormats() {
    QList<QArvPixelFormat*> list;
    auto plugins = QPluginLoader::staticInstances();
    foreach (auto plugin, plugins) {
        auto fmt = qobject_cast<QArvPixelFormat*>(plugin);
        if (fmt != NULL) list << fmt;
    }
    return list;
}

static QMap<ArvPixelFormat, enum AVPixelFormat> initSwScaleFormats();

// List of formats supported by plugins.
Q_GLOBAL_STATIC_WITH_ARGS(QList<QArvPixelFormat*>, pluginFormats,
                          (initPluginFormats()))

// List of formats supported by libswscale, with mappings to
// appropriate ffmpeg formats.
QMap<ArvPixelFormat, enum AVPixelFormat> swScaleFormats = initSwScaleFormats();

QList<ArvPixelFormat> QArvPixelFormat::supportedFormats() {
    QList<ArvPixelFormat> list;
    foreach (auto fmt, *pluginFormats) {
        list << fmt->pixelFormat();
    }
    list << swScaleFormats.keys();
    return list;
}

/*
 * "specification" contains serialized decoder type and necessary
 * parameters. This function simply dispatches on type, which is
 * a string. All decoders also contain the size.
 */
QArvDecoder* QArvDecoder::makeDecoder(QByteArray specification) {
    static QStringList types = {
        "Aravis",
        "SwScale",
    };
    QDataStream s(&specification, QIODevice::ReadOnly);
    QString type;
    QSize size;
    s >> type;
    if (!types.contains(type)) return NULL;
    s >> size;
    if (type == "Aravis") {
        ArvPixelFormat fmt;
        bool fast;
        s >> fmt >> fast;
        return QArvDecoder::makeDecoder(fmt, size, fast);
    } else if (type == "SwScale") {
        static_assert(sizeof(AVPixelFormat) <= sizeof(qlonglong),
                      "qlonglong not large enough to hold libav PixelFormat.");
        qlonglong fmt;
        int flags;
        s >> fmt >> flags;
        return QArvDecoder::makeSwScaleDecoder((AVPixelFormat)fmt, size, flags);
    }
    return NULL;
}

/*!
 * Returns NULL if the format is not supported.
 */
QArvDecoder* QArvDecoder::makeDecoder(ArvPixelFormat format,
                                      QSize size,
                                      bool fast) {
    foreach (auto fmt, *pluginFormats) {
        if (fmt->pixelFormat() == format) return fmt->makeDecoder(size);
    }
    for (auto fmtI = swScaleFormats.keyBegin(), end = swScaleFormats.keyEnd();
         fmtI != end; ++fmtI) {
        const uint arvfmt = *fmtI;
        int swsFlags;
        if (fast)
            swsFlags = SWS_FAST_BILINEAR;
        else
            swsFlags = SWS_FAST_BILINEAR | SWS_BITEXACT;
        if (arvfmt == format)
            return new QArv::SwScaleDecoder(size,
                                            swScaleFormats[arvfmt],
                                            arvfmt,
                                            swsFlags);
    }
    return NULL;
}

/*!
 * Returns NULL if the format is not supported.
 */
QArvDecoder* QArvDecoder::makeSwScaleDecoder(AVPixelFormat fmt,
                                             QSize size,
                                             int swsFlags) {
    if (swsFlags)
        return new QArv::SwScaleDecoder(size, fmt, 0, swsFlags);
    else
        return new QArv::SwScaleDecoder(size, fmt, 0);
}

static QMap<ArvPixelFormat, AVPixelFormat> initSwScaleFormats() {
    QMap<ArvPixelFormat, AVPixelFormat> m;

    m[ARV_PIXEL_FORMAT_YUV_422_PACKED] = AV_PIX_FMT_UYVY422;
    m[ARV_PIXEL_FORMAT_RGB_8_PACKED] = AV_PIX_FMT_RGB24;
    m[ARV_PIXEL_FORMAT_BGR_8_PACKED] = AV_PIX_FMT_BGR24;
    m[ARV_PIXEL_FORMAT_RGBA_8_PACKED] = AV_PIX_FMT_RGBA;
    m[ARV_PIXEL_FORMAT_BGRA_8_PACKED] = AV_PIX_FMT_BGRA;
    m[ARV_PIXEL_FORMAT_YUV_411_PACKED] = AV_PIX_FMT_UYYVYY411;
    m[ARV_PIXEL_FORMAT_YUV_422_PACKED] = AV_PIX_FMT_UYVY422;
    m[ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED] = AV_PIX_FMT_YUYV422;

    return m;
}
