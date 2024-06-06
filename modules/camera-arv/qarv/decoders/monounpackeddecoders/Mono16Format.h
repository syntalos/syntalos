#pragma once

#include "../monounpacked.h"
#include <arv.h>

namespace QArv
{

class Mono16Format : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.Mono16Format")
public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_MONO_16; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new MonoUnpackedDecoder<uint16_t, 16, ARV_PIXEL_FORMAT_MONO_16>(
            size);
    }
};

}
