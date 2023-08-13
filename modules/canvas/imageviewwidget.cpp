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
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <opencv2/opencv.hpp>

#if defined(QT_OPENGL_ES)
#define USE_GLES 1
#else
#undef USE_GLES
#endif

static const char *vertexShaderSource =
#ifdef USE_GLES
    "#version 300 es\n"
#else
    "#version 330 core\n"
#endif
    "layout(location = 0) in vec2 position;\n"
    "out vec2 texCoord;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    texCoord = vec2(position.x * 0.5 + 0.5, 1.0 - position.y * 0.5 - 0.5);\n"
    "}\n";

static const char *fragmentShaderSource =
#ifdef USE_GLES
    "#version 300 es\n"
#else
    "#version 330 core\n"
#endif
    "in vec2 texCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex;\n"
    "uniform float aspectRatio;\n"
    "uniform vec4 bgColor;\n"
    "uniform lowp float showSaturation;\n"
    "\n"
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
    "        FragColor = bgColor; // Bars around image\n"
    "    } else {\n"
    "        FragColor = texture(tex, sceneCoord);\n"
    "        lowp float cVal = (FragColor.r + FragColor.g + FragColor.b) / 3.0;\n"
    "        if (cVal >= .99 && showSaturation == 1.0)\n"
    "            FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
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

    bool highlightSaturation;

    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    std::unique_ptr<QOpenGLTexture> matTex;
    QOpenGLShaderProgram shaderProgram;
};
#pragma GCC diagnostic pop

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(new ImageViewWidget::Private)
{
    d->highlightSaturation = false;
    d->bgColorVec = QVector4D(0.46, 0.46, 0.46, 1.0);
    setWindowTitle("Video");

    setMinimumSize(QSize(320, 256));
}

ImageViewWidget::~ImageViewWidget()
{
    d->vao.destroy();
    d->vbo.destroy();
}

void ImageViewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    auto bgColor = QColor::fromRgb(150, 150, 150);
    float r = ((float)bgColor.darker().red()) / 255.0f;
    float g = ((float)bgColor.darker().green()) / 255.0f;
    float b = ((float)bgColor.darker().blue()) / 255.0f;
    d->bgColorVec = QVector4D(r, g, b, 1.0);
    glClearColor(r, g, b, 1.0f);

    // Compile & link shaders
    bool glOkay = true;
    glOkay = d->shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource) && glOkay;
    glOkay = d->shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource) && glOkay;

    if (!d->shaderProgram.link()) {
        glOkay = false;
        qWarning().noquote() << "Unable to link shader program:" << d->shaderProgram.log();
    }

    // Initialize VAO & VBO
    d->vao.create();
    glOkay = glOkay && d->vao.isCreated();
    if (!glOkay) {
        QMessageBox::critical(
            this,
            QStringLiteral("Unable to initialize OpenGL"),
            QStringLiteral(
                "Unable to compiler or link OpenGL shader or initialize vertex array object. Your system needs at "
                "least "
                "OpenGL/GLES 3.2 to run this application.\n"
                "You may want to try to upgrade your graphics drivers, or check the application log for details."),
            QMessageBox::Ok);
        qFatal(
            "Unable to initialize OpenGL:\nVAO: %s\nShader Log: %s",
            d->vao.isCreated() ? "true" : "false",
            qPrintable(d->shaderProgram.log()));
        exit(6);
    }

    d->vao.bind();

    GLfloat vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    d->vbo.create();
    d->vbo.bind();
    d->vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    d->vbo.allocate(vertices, sizeof(vertices));

    d->shaderProgram.enableAttributeArray(0);
    d->shaderProgram.setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));

    d->vbo.release();
    d->vao.release();
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
#ifdef USE_GLES
        GL_RGB,
#else
        GL_BGR,
#endif
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
    d->shaderProgram.setUniformValue("showSaturation", d->highlightSaturation ? (GLfloat)1.0 : (GLfloat)0.0);

    d->vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    d->vao.release();

    d->matTex->release();
    d->shaderProgram.release();
}

bool ImageViewWidget::showImage(const cv::Mat &image)
{
    auto channels = image.channels();

#ifdef USE_GLES
    if (channels == 1)
        cvtColor(image, d->origImage, cv::COLOR_GRAY2RGB);
    else if (channels == 4)
        cvtColor(image, d->origImage, cv::COLOR_BGRA2RGB);
    else
        cvtColor(image, d->origImage, cv::COLOR_BGR2RGB);
#else
    if (channels == 1)
        cvtColor(image, d->origImage, cv::COLOR_GRAY2BGR);
    else if (channels == 4)
        cvtColor(image, d->origImage, cv::COLOR_BGRA2BGR);
    else
        image.copyTo(d->origImage);
#endif

    update();
    return true;
}

void ImageViewWidget::setMinimumSize(const QSize &size)
{
    setMinimumWidth(size.width());
    setMinimumHeight(size.height());
}

void ImageViewWidget::setHighlightSaturation(bool enabled)
{
    d->highlightSaturation = enabled;
}

bool ImageViewWidget::highlightSaturation() const
{
    return d->highlightSaturation;
}
