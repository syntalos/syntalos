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

#ifndef MONO8_H
#define MONO8_H

#include "../qarvdecoder.h"
#include <arv.h>

namespace QArv
{

class Mono12PackedDecoder : public QArvDecoder {
public:
    Mono12PackedDecoder(QSize size_);
    void decode(QByteArray frame) override;
    const cv::Mat getCvImage() override;
    int cvType() override { return CV_16UC1; }
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_MONO_12_PACKED; }
    QByteArray decoderSpecification() override;

private:
    QSize size;
    cv::Mat M;
};

class Mono12PackedFormat : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.Mono12PackedFormat")

public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_MONO_12_PACKED; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new Mono12PackedDecoder(size);
    }
};

}

#endif
