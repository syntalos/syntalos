/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "imageviewwidget.h"

#include <opencv2/opencv.hpp>
#include <QCoreApplication>
#include <QDebug>


#pragma GCC diagnostic ignored "-Wpadded"
class ImageViewWidget::Private
{
public:
    Private() {}
    ~Private() {}

    QColor bgColor;
    cv::Mat origImage;

    int renderWidth;
    int renderHeight;
    int renderPosX;
    int renderPosY;
};
#pragma GCC diagnostic pop

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new ImageViewWidget::Private)
{
    d->bgColor = QColor::fromRgb(150, 150, 150);
    setWindowTitle("Video");

    setMinimumSize(QSize(320, 256));
}

ImageViewWidget::~ImageViewWidget()
{
}

void ImageViewWidget::initializeGL()
{
    if (!initializeOpenGLFunctions()) {
        qCritical() << "Unable to initialize OpenGL functions. Your system needs at least OpenGL 3.0 to run this application.";
        QCoreApplication::exit(6);
    }

    float r = ((float)d->bgColor.darker().red()) / 255.0f;
    float g = ((float)d->bgColor.darker().green()) / 255.0f;
    float b = ((float)d->bgColor.darker().blue()) / 255.0f;
    glClearColor(r, g, b, 1.0f);
}

GLuint ImageViewWidget::matToTexture(cv::Mat &mat, GLenum minFilter, GLenum magFilter, GLenum wrapFilter)
{
    // Generate a number for our textureID's unique handle
    GLuint textureID;
    glGenTextures(1, &textureID);

    // Bind to our texture handle
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Catch silly-mistake texture interpolation method for magnification
    if (magFilter == GL_LINEAR_MIPMAP_LINEAR  ||
        magFilter == GL_LINEAR_MIPMAP_NEAREST ||
        magFilter == GL_NEAREST_MIPMAP_LINEAR ||
        magFilter == GL_NEAREST_MIPMAP_NEAREST) {
        qWarning().noquote() << "Cannot use MIPMAPs for magnification, resetting filter to GL_LINEAR";
        magFilter = GL_LINEAR;
    }

    // Set texture interpolation methods for minification and magnification
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    // Set texture clamping method
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapFilter);

    // Set incoming texture format to:
    // GL_BGR       for CV_CAP_OPENNI_BGR_IMAGE,
    // GL_LUMINANCE for CV_CAP_OPENNI_DISPARITY_MAP,
    // NOTE: Work out other mappings as required
    GLenum inputColourFormat = GL_BGR;
    if (mat.channels() == 1) {
        inputColourFormat = GL_LUMINANCE;
    }

    // Create the texture
    glTexImage2D(GL_TEXTURE_2D,     // Type of texture
                 0,                 // Pyramid level (for mip-mapping) - 0 is the top level
                 GL_RGB,            // Internal colour format to convert to
                 mat.cols,          // Image width  i.e. 640 for Kinect in standard mode
                 mat.rows,          // Image height i.e. 480 for Kinect in standard mode
                 0,                 // Border width in pixels (can either be 1 or 0)
                 inputColourFormat, // Input image format (i.e. GL_RGB, GL_RGBA, GL_BGR etc.)
                 GL_UNSIGNED_BYTE,  // Image data type
                 mat.ptr());        // The actual image data itself

    // If we're using mipmaps then generate them.
    if (minFilter == GL_LINEAR_MIPMAP_LINEAR  ||
            minFilter == GL_LINEAR_MIPMAP_NEAREST ||
            minFilter == GL_NEAREST_MIPMAP_LINEAR ||
            minFilter == GL_NEAREST_MIPMAP_NEAREST) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    return textureID;
}

void ImageViewWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, (GLint)width, (GLint)height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, width, height, 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);

    recalculatePosition();
    update();
}

void ImageViewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderImage();
}

void ImageViewWidget::renderImage()
{
    if (d->origImage.empty())
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, this->width(), this->height(), 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    auto tex = matToTexture(d->origImage, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_BORDER);

    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
        glTexCoord2i(0, 1);
        glVertex2i(d->renderPosX, d->renderHeight - d->renderPosY);
        glTexCoord2i(0, 0);
        glVertex2i(d->renderPosX, -d->renderPosY);
        glTexCoord2i(1, 0);
        glVertex2i(d->renderWidth + d->renderPosX, -d->renderPosY);
        glTexCoord2i(1,1);
        glVertex2i(d->renderWidth + d->renderPosX, d->renderHeight - d->renderPosY);
    glEnd();

    glDeleteTextures(1, &tex);
    glDisable(GL_TEXTURE_2D);

    glFlush();
}

void ImageViewWidget::recalculatePosition()
{
    auto imgRatio = (float)d->origImage.cols / (float)d->origImage.rows;

    d->renderWidth = this->size().width();
    d->renderHeight = floor(d->renderWidth / imgRatio);

    if (d->renderHeight > this->size().height()) {
        d->renderHeight = this->size().height();
        d->renderWidth = floor(d->renderHeight * imgRatio);
    }

    d->renderPosX = floor((this->size().width() - d->renderWidth) / 2.0);
    d->renderPosY = -floor((this->size().height() - d->renderHeight) / 2.0);
}

bool ImageViewWidget::showImage(const cv::Mat& image)
{
    auto channels = image.channels();
    if (channels == 1)
        cvtColor(image, d->origImage, cv::COLOR_GRAY2BGR);
    else if (channels == 4)
        cvtColor(image, d->origImage, cv::COLOR_BGRA2BGR);
    else
        image.copyTo(d->origImage);

    recalculatePosition();
    update();

    return true;
}

void ImageViewWidget::setMinimumSize(const QSize& size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}
