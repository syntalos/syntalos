#include "graymap.h"

static QVector<QRgb> initGraymap() {
    QVector<QRgb> map(256);
    for (int i = 0; i < 256; i++) map[i] = qRgb(i, i, i);
    return map;
}

QVector<QRgb> graymap = initGraymap();
