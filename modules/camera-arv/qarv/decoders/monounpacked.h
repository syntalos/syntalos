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

#ifndef MONOUNPACKED_H
#define MONOUNPACKED_H

#include <type_traits>
#include "../qarvdecoder.h"

namespace QArv
{

template <typename InputType, uint bitsPerPixel, ArvPixelFormat pixFmt>
class MonoUnpackedDecoder : public QArvDecoder {

    static_assert(sizeof(InputType) <= sizeof(uint16_t),
                  "InputType too large.");

private:
    QSize size;
    cv::Mat M;
    static const bool OutputIsChar = bitsPerPixel <= 8;
    typedef typename std::conditional<OutputIsChar, uint8_t,
                                      uint16_t>::type OutputType;
    static const int cvMatType = OutputIsChar ? CV_8UC1 : CV_16UC1;
    static const bool typeIsSigned = std::is_signed<InputType>::value;
    static const uint zeroBits = 8*sizeof(OutputType) - bitsPerPixel;
    static const uint signedShiftBits = bitsPerPixel - 1;

public:
    MonoUnpackedDecoder(QSize size_) :
        size(size_), M(size_.height(), size_.width(), cvMatType) {}

    ArvPixelFormat pixelFormat() override { return pixFmt; }

    QByteArray decoderSpecification() override {
        QByteArray b;
        QDataStream s(&b, QIODeviceBase::WriteOnly);
        s << QString("Aravis") << size << pixFmt << false;
        return b;
    }

    int cvType() override { return cvMatType; };

    void decode(QByteArray frame) override {
        const InputType* dta =
            reinterpret_cast<const InputType*>(frame.constData());
        const int h = size.height(), w = size.width();
        for (int i = 0; i < h; i++) {
            auto line = M.ptr<OutputType>(i);
            for (int j = 0; j < w; j++) {
                OutputType tmp;
                if (typeIsSigned)
                    tmp = dta[i * w + j] + (1<<signedShiftBits);
                else
                    tmp = dta[i * w + j];
                line[j] = tmp << (zeroBits);
            }
        }
    }

    const cv::Mat getCvImage() override {
        return M;
    }
};

}

#endif
