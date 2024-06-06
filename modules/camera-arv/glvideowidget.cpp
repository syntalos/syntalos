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

#include "qarv/qarv-globals.h"
#include "glvideowidget.h"
#include "qarv/qarvdecoder.h"
#include <QPainter>
#include <QApplication>

using namespace QArv;

GLVideoWidget::GLVideoWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      idleImageRenderer(QString(":/module/camera-generic")),
      idling(true),
      selecting(false),
      drawRectangle(false),
      fixedSelection(false),
      corner1(),
      corner2(),
      rectangle(),
      whitepen(Qt::white),
      blackpen(Qt::black),
      backgroundBrush(QApplication::palette().base())
{
    whitepen.setWidth(0);
    whitepen.setStyle(Qt::DotLine);
    blackpen.setWidth(0);
}

GLVideoWidget::~GLVideoWidget() {}

void GLVideoWidget::setImage(const QImage &image_)
{
    if (image_.isNull()) {
        if (!idling) {
            idling = true;
            in = QRect();
            updateOutRect();
        }
    } else {
        idling = false;
        image = image_;
        if (in.size() != image.size()) {
            in = image.rect();
            updateOutRect();
        }
    }
    update();
}

void GLVideoWidget::swapFrames()
{
    idling = false;
    image.swap(unusedImage);
    if (in.size() != image.size()) {
        in = image.rect();
        updateOutRect();
    }
    update();
}

QImage *GLVideoWidget::unusedFrame()
{
    return &unusedImage;
}

void GLVideoWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    updateOutRect();
}

void GLVideoWidget::updateOutRect()
{
    auto view = rect();
    out = view;
    QSize thesize;
    if (idling)
        thesize = idleImageRenderer.defaultSize();
    else
        thesize = in.size();
    if (thesize != view.size()) {
        float aspect = thesize.width() / (float)thesize.height();
        float vaspect = view.width() / (float)view.height();
        int x, y, w, h;
        if (vaspect > aspect) {
            h = view.height();
            w = aspect * h;
            y = view.y();
            x = view.x() + (view.width() - w) / 2.;
        } else {
            w = view.width();
            h = w / aspect;
            x = view.x();
            y = view.y() + (view.height() - h) / 2.;
        }
        out.setRect(x, y, w, h);
    }
}

void GLVideoWidget::paintGL()
{
    QPainter painter(this);
    painter.fillRect(rect(), backgroundBrush);
    if (!idling) {
        if (in.size() != out.size())
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(out, image);

        if (drawRectangle) {
            painter.setPen(blackpen);
            painter.drawRect(drawnRectangle);
            painter.setPen(whitepen);
            painter.drawRect(drawnRectangle);
        }
    } else {
        idleImageRenderer.render(&painter, out);
    }
}

void GLVideoWidget::enableSelection(bool enable)
{
    if (enable) {
        selecting = true;
        setCursor(Qt::CrossCursor);

        if (fixedSelection)
            setMouseTracking(true);
    } else {
        selecting = false;
        drawRectangle = false;
        rectangle = QRect();
        setCursor(Qt::ArrowCursor);
        setMouseTracking(false);
    }
}

void GLVideoWidget::setSelectionSize(QSize size)
{
    if (size.width() == 0 || size.height() == 0)
        fixedSelection = false;
    else {
        fixedSelection = true;
        fixedSize = size;
    }
}

void GLVideoWidget::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (fixedSelection)
        return;
    if (selecting)
        corner1 = event->pos();
}

void GLVideoWidget::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);

    if (!selecting)
        return;

    drawRectangle = true;
    float scale = out.width() / (float)in.width();

    if (fixedSelection) {
        if ((fixedSize.width() > in.width()) || (fixedSize.height() > in.height())) {
            rectangle = in;
            drawnRectangle = out;
            return;
        }

        rectangle.setSize(fixedSize);
        rectangle.moveCenter((event->pos() - out.topLeft()) / scale);

        if (rectangle.x() < 0)
            rectangle.translate(-rectangle.x(), 0);

        if (rectangle.y() < 0)
            rectangle.translate(0, -rectangle.y());

        int hmargin = (rectangle.x() + rectangle.width()) - in.width();
        if (hmargin > 0)
            rectangle.translate(-hmargin, 0);

        int vmargin = (rectangle.y() + rectangle.height()) - in.height();
        if (vmargin > 0)
            rectangle.translate(0, -vmargin);

        drawnRectangle.moveTopLeft(out.topLeft() + rectangle.topLeft() * scale);
        drawnRectangle.setSize(rectangle.size() * scale);
    } else {
        corner2 = event->pos();
        QRect rec(corner1, corner2);
        rec &= out;
        rec = rec.normalized();
        QPoint corner = rec.topLeft(), outcorner = out.topLeft();
        corner = (corner - outcorner) / scale;
        int width = rec.width() / scale, height = rec.height() / scale;
        rectangle.setRect(corner.x(), corner.y(), width, height);
        drawnRectangle = rec;
    }
}

void GLVideoWidget::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    if (selecting) {
        selecting = false;
        emit selectionComplete(rectangle);
    }
}

QSize GLVideoWidget::getImageSize()
{
    return image.size();
}
