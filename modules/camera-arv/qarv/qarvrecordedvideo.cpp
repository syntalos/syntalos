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

#include "qarvrecordedvideo.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include "qarv-globals.h"
#include <arv.h>
extern "C" {
#include <libavutil/imgutils.h>
}

using namespace QArv;

// Make sure settings format matches rawrecorders.cpp!

QArvRecordedVideo::QArvRecordedVideo(const QString& filename) :
    fps(0), uncompressed(true), arvPixfmt(0), swscalePixfmt(AV_PIX_FMT_NONE),
    frameBytes_(0) {
    QSettings s(filename, QSettings::Format::IniFormat);
    isOK = s.status() == QSettings::Status::NoError;
    if (!isOK) {
        logMessage() << "Invalid description file.";
        return;
    }
    s.beginGroup("qarv_raw_video_description");
    QVariant v = s.value("description_version");
    isOK = v.toString() == "0.1";
    if (!isOK) {
        logMessage() << "Invalid video description file version.";
        return;
    }

    v = s.value("file_name");
    QFileInfo finfo(filename);
    QString dirname = finfo.absoluteDir().path();
    videofile.setFileName(dirname + "/" + v.toString());
    isOK = videofile.open(QIODevice::ReadOnly);
    uncompressed = true;
    if (!isOK) {
        // TODO: try opening compressed versions
        logMessage() << "Unable to open video file" << v.toString();
        return;
    }

    v = s.value("frame_size");
    fsize = v.toSize();
    v = s.value("nominal_fps");
    fps = v.toInt();
    v = s.value("encoding_type");
    auto type = v.toString();

    if (type == "aravis") {
        v = s.value("arv_pixel_format");
        arvPixfmt = v.toString().toULongLong(NULL, 16);
    } else if (type == "libavutil") {
        v = s.value("libavutil_pixel_format");
        swscalePixfmt = (enum AVPixelFormat)v.toLongLong();
    } else {
        logMessage() << "Unable to determine decoder type.";
        isOK = false;
        return;
    }

    v = s.value("frame_bytes");
    frameBytes_ = v.toInt();
    if (!frameBytes_) {
        isOK = false;
        logMessage() << "Unable to read frame bytesize.";
        return;
    }

    isOK = true;
}

QArvRecordedVideo::QArvRecordedVideo(const QString& filename,
                                     enum AVPixelFormat swsFmt,
                                     uint headerBytes, QSize size) :
    videofile(filename), fsize(size), fps(10), uncompressed(true),
    isOK(fsize.isValid()), arvPixfmt(0), swscalePixfmt(swsFmt) {
    if (!isOK) {
        logMessage() << "Invalid frame size" << fsize;
        return;
    }
    isOK = videofile.open(QIODevice::ReadOnly);
    if (!isOK) {
        logMessage() << "Unable to open video file" << filename;
        return;
    }
    if (headerBytes) {
        isOK = videofile.seek(headerBytes);
    }
    if (!isOK) {
        logMessage() << "Unable to skip header, file not seekable.";
        return;
    }
    frameBytes_ = av_image_get_buffer_size(
        swscalePixfmt, size.width(), size.height(), 1);
}

bool QArvRecordedVideo::status() {
    return isOK && (videofile.error() == QFile::NoError);
}

QFile::FileError QArvRecordedVideo::error() {
    return videofile.error();
}

QString QArvRecordedVideo::errorString() {
    return videofile.errorString();
}

bool QArvRecordedVideo::atEnd() {
    return videofile.atEnd();
}

bool QArvRecordedVideo::isSeekable() {
    return uncompressed;
}

int QArvRecordedVideo::framerate() {
    return fps;
}

QSize QArvRecordedVideo::frameSize() {
    return fsize;
}

uint QArvRecordedVideo::frameBytes() {
    return frameBytes_;
}

QArvDecoder* QArvRecordedVideo::makeDecoder() {
    if (!isOK) return NULL;
    if (arvPixfmt != 0) {
        return QArvDecoder::makeDecoder(arvPixfmt, fsize);
    } else if (swscalePixfmt != AV_PIX_FMT_NONE) {
        return QArvDecoder::makeSwScaleDecoder(swscalePixfmt, fsize);
    } else {
        isOK = false;
        logMessage() << "Unknown decoder type.";
        return NULL;
    }
}

bool QArvRecordedVideo::seek(quint64 frame)
{
    return videofile.seek(frame*frameBytes_);
}

QByteArray QArvRecordedVideo::read() {
    return videofile.read(frameBytes_);
}

uint QArvRecordedVideo::numberOfFrames() {
    return videofile.size() / frameBytes_;
}
