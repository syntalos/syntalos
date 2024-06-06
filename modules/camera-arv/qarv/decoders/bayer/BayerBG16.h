#pragma once

#include "decoder.h"

namespace QArv
{

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16

class BayerBG16 : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.BayerBG16")

public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_BAYER_BG_16; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_BG_16>(size);
    }
};

#endif

}
