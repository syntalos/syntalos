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

#include "mono12packed.h"

using namespace QArv;

Mono12PackedDecoder::Mono12PackedDecoder(QSize size_) :
    size(size_), M(size_.height(), size_.width(), CV_16U) {}


void Mono12PackedDecoder::decode(QByteArray frame) {
    const uchar* dta = reinterpret_cast<const uchar*>(frame.constData());
    const int h = size.height(), w = size.width();

    int line = 0;
    auto linestart = M.ptr<uint16_t>(0);
    int outcurrent = 0;
    const uchar* inptr = dta;
    uint16_t pixel;
    uchar* bytes = reinterpret_cast<uchar*>(&pixel);
    while (inptr < dta + frame.size()) {
        bytes[0] = inptr[1] << 4;
        bytes[1] = inptr[0];
        linestart[outcurrent++] = pixel;

        if (outcurrent == w) {
            if (++line == h) break;
            linestart = M.ptr<uint16_t>(line);
            outcurrent = 0;
        }

        bytes[0] = inptr[1] & 0xF0;
        bytes[1] = inptr[2];
        linestart[outcurrent++] = pixel;

        if (outcurrent == w) {
            if (++line == h) break;
            linestart = M.ptr<uint16_t>(line);
            outcurrent = 0;
        }

        inptr += 3;
    }
}

const cv::Mat Mono12PackedDecoder::getCvImage() {
    return M;
}

QByteArray Mono12PackedDecoder::decoderSpecification() {
    QByteArray b;
    QDataStream s(&b, QIODevice::WriteOnly);
    s << QString("Aravis") << size << pixelFormat() << false;
    return b;
}


Q_IMPORT_PLUGIN(Mono12PackedFormat)
