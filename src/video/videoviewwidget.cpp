/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "videoviewwidget.h"

#include <QOpenGLFunctions>
#include <opencv2/opencv.hpp>
#include <QDebug>

VideoViewWidget::VideoViewWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_bgColor = QColor::fromRgb(150, 150, 150);
    setWindowTitle("Video");

    setMinimumSize(QSize(320, 256));
}

void VideoViewWidget::initializeGL()
{
    QOpenGLWidget::initializeGL();

    float r = ((float)m_bgColor.darker().red()) / 255.0f;
    float g = ((float)m_bgColor.darker().green()) / 255.0f;
    float b = ((float)m_bgColor.darker().blue()) / 255.0f;
    glClearColor(r, g, b, 1.0f);
}

static GLuint matToTexture(cv::Mat &mat, GLenum minFilter, GLenum magFilter, GLenum wrapFilter)
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
            magFilter == GL_NEAREST_MIPMAP_NEAREST)
    {
        qWarning() << "You can't use MIPMAPs for magnification - setting filter to GL_LINEAR";
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
    // Work out other mappings as required ( there's a list in comments in main() )
    GLenum inputColourFormat = GL_BGR;
    if (mat.channels() == 1)
    {
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

    // If we're using mipmaps then generate them. Note: This requires OpenGL 3.0 or higher
    if (minFilter == GL_LINEAR_MIPMAP_LINEAR  ||
            minFilter == GL_LINEAR_MIPMAP_NEAREST ||
            minFilter == GL_NEAREST_MIPMAP_LINEAR ||
            minFilter == GL_NEAREST_MIPMAP_NEAREST)
    {
#ifdef OPENGL_3
        glGenerateMipmap(GL_TEXTURE_2D);
#endif
    }

    return textureID;
}

void VideoViewWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, (GLint)width, (GLint)height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, width, height, 0, 0, 1);
    glMatrixMode(GL_MODELVIEW);

    recalculatePosition();
    updateScene();
}

void VideoViewWidget::updateScene()
{
    if (this->isVisible()) update();
}

void VideoViewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderImage();
}

void VideoViewWidget::renderImage()
{
    if (m_origImage.empty())
        return;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, this->width(), this->height(), 0.0f, 0.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    auto tex = matToTexture(m_origImage, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_BORDER);

    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
        glTexCoord2i(0, 1);
        glVertex2i(m_renderPosX, m_renderHeight - m_renderPosY);
        glTexCoord2i(0, 0);
        glVertex2i(m_renderPosX, -m_renderPosY);
        glTexCoord2i(1, 0);
        glVertex2i(m_renderWidth + m_renderPosX, -m_renderPosY);
        glTexCoord2i(1,1);
        glVertex2i(m_renderWidth + m_renderPosX, m_renderHeight - m_renderPosY);
    glEnd();

    glDeleteTextures(1, &tex);
    glDisable(GL_TEXTURE_2D);

    glFlush();
}

void VideoViewWidget::recalculatePosition()
{
    auto imgRatio = (float)m_origImage.cols / (float)m_origImage.rows;

    m_renderWidth = this->size().width();
    m_renderHeight = floor(m_renderWidth / imgRatio);

    if (m_renderHeight > this->size().height()) {
        m_renderHeight = this->size().height();
        m_renderWidth = floor(m_renderHeight * imgRatio);
    }

    m_renderPosX = floor((this->size().width() - m_renderWidth) / 2);
    m_renderPosY = -floor((this->size().height() - m_renderHeight) / 2);
}

bool VideoViewWidget::showImage(const cv::Mat& image)
{
    if (image.channels() == 1)
        cvtColor(image, m_origImage, CV_GRAY2BGR);
    else if (image.channels() == 4)
        cvtColor(image, m_origImage, CV_BGRA2BGR);
    else
        m_origImage = image;

    recalculatePosition();
    updateScene();

    return true;
}

void VideoViewWidget::setMinimumSize(const QSize& size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}
