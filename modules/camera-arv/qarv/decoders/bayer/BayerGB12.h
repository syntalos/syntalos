#pragma once

#include "decoder.h"

namespace QArv
{

class BayerGB12 : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.BayerGB12")

public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_BAYER_GB_12; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new BayerDecoder<ARV_PIXEL_FORMAT_BAYER_GB_12>(size);
    }
};

}
