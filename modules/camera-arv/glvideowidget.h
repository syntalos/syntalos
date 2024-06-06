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

#ifndef GLVIDEOWIDGET_H
#define GLVIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QImage>
#include <QMouseEvent>
#include <QSvgRenderer>
#include <QPen>
#include <opencv2/core/core.hpp>

namespace QArv
{

class GLVideoWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    GLVideoWidget(QWidget *parent = NULL);
    ~GLVideoWidget();
    void paintGL() override;
    QImage *unusedFrame();
    void swapFrames();
    void setImage(const QImage &image_ = QImage());
    QSize getImageSize();

public slots:
    void enableSelection(bool enable);
    void setSelectionSize(QSize size);

signals:
    void selectionComplete(QRect region);

private:
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;
    void updateOutRect();

    QImage image, unusedImage;
    QRect in, out;
    QSvgRenderer idleImageRenderer;

    bool idling, selecting, drawRectangle, fixedSelection;
    QPoint corner1, corner2;
    QRect rectangle, drawnRectangle;
    QSize fixedSize;
    QPen whitepen, blackpen;
    QBrush backgroundBrush;
};

} // namespace QArv

#endif // GLVIDEOWIDGET_H
