/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDebug>
#include <QMessageBox>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <opencv2/opencv.hpp>

static const char *vertexShaderSource =
    "#version 120\n"
    "attribute vec2 position;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    texCoord = vec2(position.x * 0.5 + 0.5, 1.0 - position.y * 0.5 - 0.5);\n"
    "}\n";

static const char *fragmentShaderSource =
    "#version 120\n"
    "uniform sampler2D tex;\n"
    "uniform float aspectRatio;\n"
    "uniform vec4 bgColor;\n"
    "varying vec2 texCoord;\n"
    "void main()\n"
    "{\n"
    "    vec2 sceneCoord = texCoord;\n"
    "    if (aspectRatio > 1.0) {\n"
    "        sceneCoord.x *= aspectRatio;\n"
    "        sceneCoord.x -= (aspectRatio - 1.0) * 0.5;\n"
    "    } else {\n"
    "        sceneCoord.y *= 1.0 / aspectRatio; // Adjust for vertical centering\n"
    "        sceneCoord.y += (1.0 - (1.0 / aspectRatio)) * 0.5;\n"
    "    }\n"
    "    if (sceneCoord.x < 0.0 || sceneCoord.x > 1.0 || "
    "        sceneCoord.y < 0.0 || sceneCoord.y > 1.0) {\n"
    "        gl_FragColor = bgColor; // Bars around image\n"
    "    } else {\n"
    "        gl_FragColor = texture2D(tex, sceneCoord);\n"
    "    }\n"
    "}\n";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class ImageViewWidget::Private
{
public:
    Private() {}
    ~Private() {}

    QVector4D bgColorVec;
    cv::Mat origImage;

    std::unique_ptr<QOpenGLTexture> matTex;
    QOpenGLShaderProgram shaderProgram;
};
#pragma GCC diagnostic pop

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new ImageViewWidget::Private)
{
    d->bgColorVec = QVector4D(0.46, 0.46, 0.46, 1.0);
    setWindowTitle("Video");

    setMinimumSize(QSize(320, 256));
}

ImageViewWidget::~ImageViewWidget() {}

void ImageViewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    auto bgColor = QColor::fromRgb(150, 150, 150);
    float r = ((float)bgColor.darker().red()) / 255.0f;
    float g = ((float)bgColor.darker().green()) / 255.0f;
    float b = ((float)bgColor.darker().blue()) / 255.0f;
    d->bgColorVec = QVector4D(r, g, b, 1.0);
    glClearColor(r, g, b, 1.0f);

    // Link shaders
    d->shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    d->shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);

    if (!d->shaderProgram.link()) {
        QMessageBox::critical(
            this,
            QStringLiteral("Unable to link OpenGL shader"),
            QStringLiteral(
                "Unable to link OpenGl shader. Your system needs at least OpenGL ES 2.0 to run this application. "
                "You may want to try to upgrade your graphics drivers.\n"
                "Log:\n%1")
                .arg(d->shaderProgram.log()),
            QMessageBox::Ok);
        qFatal("Unable to link shader program: %s", qPrintable(d->shaderProgram.log()));
        exit(6);
    }
}

void ImageViewWidget::paintGL()
{
    renderImage();
}

void ImageViewWidget::renderImage()
{
    if (d->origImage.empty())
        return;

    // Convert cv::Mat to OpenGL texture
    if (!d->matTex) {
        d->matTex.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
        d->matTex->setWrapMode(QOpenGLTexture::ClampToEdge);
        d->matTex->setMinificationFilter(QOpenGLTexture::Linear);
        d->matTex->setMagnificationFilter(QOpenGLTexture::Linear);
    }

    d->matTex->bind();
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        d->origImage.cols,
        d->origImage.rows,
        0,
        GL_BGR,
        GL_UNSIGNED_BYTE,
        d->origImage.data);

    // Render the texture on the surface
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float imageAspectRatio = static_cast<float>(d->origImage.cols) / d->origImage.rows;
    const float aspectRatio = static_cast<float>(width()) / height() / imageAspectRatio;
    d->shaderProgram.bind();
    d->shaderProgram.setUniformValue("bgColor", d->bgColorVec);
    d->shaderProgram.setUniformValue("aspectRatio", aspectRatio);

    // Draw a quad
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1);
    glVertex2f(-1, -1);
    glTexCoord2f(1, 1);
    glVertex2f(1, -1);
    glTexCoord2f(1, 0);
    glVertex2f(1, 1);
    glTexCoord2f(0, 0);
    glVertex2f(-1, 1);
    glEnd();

    d->matTex->release();
    d->shaderProgram.release();
}

bool ImageViewWidget::showImage(const cv::Mat &image)
{
    auto channels = image.channels();
    image.copyTo(d->origImage);
    if (channels == 1)
        cvtColor(d->origImage, d->origImage, cv::COLOR_GRAY2BGR);
    else if (channels == 4)
        cvtColor(d->origImage, d->origImage, cv::COLOR_BGRA2BGR);

    update();
    return true;
}

void ImageViewWidget::setMinimumSize(const QSize &size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}
