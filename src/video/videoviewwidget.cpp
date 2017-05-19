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
    mBgColor = QColor::fromRgb(150, 150, 150);
    setWindowTitle("Video");

    setMinimumSize(QSize(320, 256));
}

void VideoViewWidget::initializeGL()
{
    QOpenGLWidget::initializeGL();

    float r = ((float)mBgColor.darker().red()) / 255.0f;
    float g = ((float)mBgColor.darker().green()) / 255.0f;
    float b = ((float)mBgColor.darker().blue()) / 255.0f;
    glClearColor(r, g, b, 1.0f);
}

void VideoViewWidget::resizeGL(int width, int height)
{
    glViewport(0, 0, (GLint)width, (GLint)height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, width, -height, 0, 0, 1);
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
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_renderQtImg.isNull())
    {
        glLoadIdentity();
        glPushMatrix();
        {
            if (m_resizedImg.width() <= 0)
            {
                if (mRenderWidth == m_renderQtImg.width() && mRenderHeight == m_renderQtImg.height())
                    m_resizedImg = m_renderQtImg;
                else
                    m_resizedImg = m_renderQtImg.scaled(QSize(mRenderWidth, mRenderHeight),
                                                        Qt::IgnoreAspectRatio,
                                                        Qt::SmoothTransformation);
            }

            // center image
            glRasterPos2i(mRenderPosX, mRenderPosY);
            glPixelZoom(1, -1);
            glDrawPixels(m_resizedImg.width(), m_resizedImg.height(), GL_RGBA, GL_UNSIGNED_BYTE, m_resizedImg.bits());
        }
        glPopMatrix();

        glFlush();
    }
}

void VideoViewWidget::recalculatePosition()
{
    mImgRatio = (float)m_origImage.cols / (float)m_origImage.rows;

    mRenderWidth = this->size().width();
    mRenderHeight = floor(mRenderWidth / mImgRatio);

    if (mRenderHeight > this->size().height())
    {
        mRenderHeight = this->size().height();
        mRenderWidth = floor(mRenderHeight * mImgRatio);
    }

    mRenderPosX = floor((this->size().width() - mRenderWidth) / 2);
    mRenderPosY = -floor((this->size().height() - mRenderHeight) / 2);

    m_resizedImg = QImage();
}

bool VideoViewWidget::showImage(const cv::Mat& image)
{
    qDebug() << "(" << this->windowTitle() << ")" << "Render img start";
    if (image.channels() == 3)
        cvtColor(image, m_origImage, CV_BGR2RGBA);
    else if (image.channels() == 1)
        cvtColor(image, m_origImage, CV_GRAY2RGBA);
    else
        m_origImage = image;

    m_renderQtImg = QImage((const unsigned char*)(m_origImage.data),
                          m_origImage.cols, m_origImage.rows,
                          m_origImage.step1(), QImage::Format_RGB32);

    qDebug() << "(" << this->windowTitle() << ")" << "AAA";
    recalculatePosition();
    qDebug() << "(" << this->windowTitle() << ")" << "BBB";
    updateScene();

    qDebug() << "(" << this->windowTitle() << ")" << "Render img done";
    return true;
}

void VideoViewWidget::setMinimumSize(const QSize& size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}
