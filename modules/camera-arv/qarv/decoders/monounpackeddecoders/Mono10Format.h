#pragma once

#include "../monounpacked.h"
#include <arv.h>

namespace QArv
{

class Mono10Format : public QObject, public QArvPixelFormat {
    Q_OBJECT
    Q_INTERFACES(QArvPixelFormat)
    Q_PLUGIN_METADATA(IID "si.ad-vega.qarv.Mono10Format")
public:
    ArvPixelFormat pixelFormat() override { return ARV_PIXEL_FORMAT_MONO_10; }
    QArvDecoder* makeDecoder(QSize size) override {
        return new MonoUnpackedDecoder<uint16_t, 10, ARV_PIXEL_FORMAT_MONO_10>(
            size);
    }
};

}
