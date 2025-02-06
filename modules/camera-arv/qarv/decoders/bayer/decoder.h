#pragma once

#include <arv.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <QDataStream>

#include "../../qarvdecoder.h"


namespace QArv
{

template <ArvPixelFormat fmt>
class BayerDecoder : public QArvDecoder {
public:
    BayerDecoder(QSize size_) : size(size_),
        decoded(size.height(), size.width(), cvType()) {
        switch (fmt) {
        case ARV_PIXEL_FORMAT_BAYER_GR_10:
        case ARV_PIXEL_FORMAT_BAYER_RG_10:
        case ARV_PIXEL_FORMAT_BAYER_GB_10:
        case ARV_PIXEL_FORMAT_BAYER_BG_10:
            stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_10, size);
            break;

        case ARV_PIXEL_FORMAT_BAYER_GR_12:
        case ARV_PIXEL_FORMAT_BAYER_RG_12:
        case ARV_PIXEL_FORMAT_BAYER_GB_12:
        case ARV_PIXEL_FORMAT_BAYER_BG_12:
            stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_12, size);
            break;

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED:
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED:
#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED:
#endif
        case ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED:
            stage1 = QArvDecoder::makeDecoder(ARV_PIXEL_FORMAT_MONO_12_PACKED,
                                              size);
            break;
        }

        switch (fmt) {
        case ARV_PIXEL_FORMAT_BAYER_GR_8:
            cvt = cv::COLOR_BayerGB2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_RG_8:
            cvt = cv::COLOR_BayerBG2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_GB_8:
            cvt = cv::COLOR_BayerGR2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_BG_8:
            cvt = cv::COLOR_BayerRG2BGR;
            break;

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16
        case ARV_PIXEL_FORMAT_BAYER_GR_16:
            cvt = cv::COLOR_BayerGB2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_RG_16:
            cvt = cv::COLOR_BayerBG2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_GB_16:
            cvt = cv::COLOR_BayerGR2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_BG_16:
            cvt = cv::COLOR_BayerRG2BGR;
            break;

#endif
        case ARV_PIXEL_FORMAT_BAYER_GR_10:
            cvt = cv::COLOR_BayerGB2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_RG_10:
            cvt = cv::COLOR_BayerBG2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_GB_10:
            cvt = cv::COLOR_BayerGR2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_BG_10:
            cvt = cv::COLOR_BayerRG2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_GR_12:
            cvt = cv::COLOR_BayerGB2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_RG_12:
            cvt = cv::COLOR_BayerBG2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_GB_12:
            cvt = cv::COLOR_BayerGR2BGR;
            break;

        case ARV_PIXEL_FORMAT_BAYER_BG_12:
            cvt = cv::COLOR_BayerRG2BGR;
            break;

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED:
            cvt = cv::COLOR_BayerGB2BGR;
            break;

#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED:
            cvt = cv::COLOR_BayerBG2BGR;
            break;

#endif
#ifdef ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED
        case ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED:
            cvt = cv::COLOR_BayerGR2BGR;
            break;

#endif
        case ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED:
            cvt = cv::COLOR_BayerRG2BGR;
            break;
        }
    };

    const cv::Mat getCvImage() override { return decoded; }

    int cvType() override {
        switch (fmt) {
        case ARV_PIXEL_FORMAT_BAYER_GR_8:
        case ARV_PIXEL_FORMAT_BAYER_RG_8:
        case ARV_PIXEL_FORMAT_BAYER_GB_8:
        case ARV_PIXEL_FORMAT_BAYER_BG_8:
            return CV_8UC3;

        default:
            return CV_16UC3;
        }
    }

    ArvPixelFormat pixelFormat() override { return fmt; }

    QByteArray decoderSpecification() override {
        QByteArray b;
        QDataStream s(&b, QIODeviceBase::WriteOnly);
        s << QString("Aravis") << size << pixelFormat() << false;
        return b;
    }

    void decode(QByteArray frame) override {
        // Workaround: cv::Mat has no const data constructor, but data need
        // not be copied, as QByteArray::data() does.
        void* data =
            const_cast<void*>(reinterpret_cast<const void*>(frame.constData()));
        switch (fmt) {
        case ARV_PIXEL_FORMAT_BAYER_GR_8:
        case ARV_PIXEL_FORMAT_BAYER_RG_8:
        case ARV_PIXEL_FORMAT_BAYER_GB_8:
        case ARV_PIXEL_FORMAT_BAYER_BG_8:
            tmp = cv::Mat(size.height(), size.width(), CV_8UC1, data);
            break;

#ifdef ARV_PIXEL_FORMAT_BAYER_GR_16
        case ARV_PIXEL_FORMAT_BAYER_GR_16:
        case ARV_PIXEL_FORMAT_BAYER_RG_16:
        case ARV_PIXEL_FORMAT_BAYER_GB_16:
        case ARV_PIXEL_FORMAT_BAYER_BG_16:
            tmp = cv::Mat(size.height(), size.width(), CV_16UC1, data);
            break;

#endif
        default:
            stage1->decode(frame);
            tmp = stage1->getCvImage();
            break;
        }
        cv::cvtColor(tmp, decoded, cvt);
    }

private:
    QSize size;
    cv::Mat tmp;
    cv::Mat decoded;
    QArvDecoder* stage1;
    int cvt;
};

}
