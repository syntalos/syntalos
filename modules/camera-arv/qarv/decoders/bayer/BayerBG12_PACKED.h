#pragma once

#include "decoder.h"

namespace QArv
{

class BayerBG12_PACKED : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.BayerBG12_PACKED")

public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED>(size);
    }
};

}
