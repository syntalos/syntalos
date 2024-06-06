#pragma once

#include "../monounpacked.h"
#include <arv.h>

namespace QArv
{

class Mono8Format : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.Mono8Format")
public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_MONO_8; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new MonoUnpackedDecoder<uint8_t, 8,
                                       ARV_PIXEL_FORMAT_MONO_8>(size);
    }
};

}
