/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <cmath>
#include <cstring>

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
    "precision mediump float;\n"
#else
    "#version 330 core\n"
#endif
    "in vec2 texCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D tex;\n"
    "uniform float aspectRatio;\n"
    "uniform vec4 bgColor;\n"
    "uniform lowp float showSaturation;\n"
    "uniform lowp float isGrayscale;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec2 sceneCoord = texCoord;\n"
    "    if (aspectRatio > 1.0) {\n"
    "        sceneCoord.x *= aspectRatio;\n"
    "        sceneCoord.x -= (aspectRatio - 1.0) * 0.5;\n"
    "    } else {\n"
    "        sceneCoord.y *= 1.0 / aspectRatio;\n" // Adjust for vertical centering
    "        sceneCoord.y += (1.0 - (1.0 / aspectRatio)) * 0.5;\n"
    "    }\n"
    "    if (sceneCoord.x < 0.0 || sceneCoord.x > 1.0 || "
    "        sceneCoord.y < 0.0 || sceneCoord.y > 1.0) {\n"
    "        FragColor = bgColor;\n" // Bars around image
    "    } else {\n"
    "        vec4 texColor = texture(tex, sceneCoord);\n"
    "        if (isGrayscale > 0.5) {\n"
    "            FragColor = vec4(texColor.rrr, 1.0);\n"
    "        } else {\n"
    "            FragColor = texColor;\n"
    "        }\n"
    "        if (showSaturation > 0.5) {\n"
    "            lowp float cVal = dot(FragColor.rgb, vec3(0.299, 0.587, 0.114));\n"
    "            if (cVal >= 0.99)\n"
    "                FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
    "        }\n"
    "    }\n"
    "}\n";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class ImageViewWidget::Private
{
public:
    Private()
        : textureId(0),
          textureWidth(0),
          textureHeight(0),
          lastAspectRatio(-1.0f),
          lastHighlightSaturation(false),
          lastBgColor(-1.0f, -1.0f, -1.0f, -1.0f),
          lastChannels(-1),
          pboIndex(0),
          pboSize(0)
    {
        pboIds[0] = pboIds[1] = 0;
    }

    ~Private()
    {
        // Note: OpenGL cleanup should be done in widget destructor when context is current
    }

    QVector4D bgColorVec;
    cv::Mat origImage;

    bool highlightSaturation;

    // OpenGL resources
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLShaderProgram shaderProgram;

    // Optimized texture handling
    GLuint textureId;
    int textureWidth, textureHeight;
    GLenum textureFormat;
    GLenum textureInternalFormat;

    // Cache uniforms to avoid redundant updates
    float lastAspectRatio;
    bool lastHighlightSaturation;
    QVector4D lastBgColor;
    int lastChannels;

    // Pixel Buffer Objects for async texture uploads
    GLuint pboIds[2]; // Double buffering
    int pboIndex;
    size_t pboSize;

    void setupTextureFormat(int channels)
    {
#ifdef USE_GLES
        // GL_LUMINANCE is deprecated in ES 3.0+, use GL_RED instead
        if (channels == 1) {
            textureInternalFormat = GL_RED;
            textureFormat = GL_RED;
        } else if (channels == 3) {
            textureInternalFormat = GL_RGB;
            textureFormat = GL_RGB;
        } else {
            textureInternalFormat = GL_RGBA;
            textureFormat = GL_RGBA;
        }
#else
        if (channels == 1) {
            textureInternalFormat = GL_RED;
            textureFormat = GL_RED;
        } else if (channels == 3) {
            textureInternalFormat = GL_RGB;
            textureFormat = GL_BGR;
        } else {
            textureInternalFormat = GL_RGBA;
            textureFormat = GL_BGRA;
        }
#endif
    }
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
    // Clean up OpenGL resources when context is still current
    makeCurrent();

    if (d->textureId != 0)
        glDeleteTextures(1, &d->textureId);
    if (d->pboIds[0] != 0)
        glDeleteBuffers(2, d->pboIds);

    d->vao.destroy();
    d->vbo.destroy();

    doneCurrent();
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

    // Initialize PBOs for async texture uploads (if supported)
    d->pboIds[0] = d->pboIds[1] = 0;
    if (context()->hasExtension("GL_ARB_pixel_buffer_object") || context()->format().majorVersion() >= 3) {
        glGenBuffers(2, d->pboIds);
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

    const auto imgWidth = d->origImage.cols;
    const auto imgHeight = d->origImage.rows;
    const auto channels = d->origImage.channels();

    // Setup or recreate texture only when dimensions change
    if (d->textureId == 0 || d->textureWidth != imgWidth || d->textureHeight != imgHeight) {
        if (d->textureId != 0) {
            glDeleteTextures(1, &d->textureId);
        }

        glGenTextures(1, &d->textureId);
        glBindTexture(GL_TEXTURE_2D, d->textureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        d->setupTextureFormat(channels);
        d->textureWidth = imgWidth;
        d->textureHeight = imgHeight;

        // Allocate texture storage
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            d->textureInternalFormat,
            imgWidth,
            imgHeight,
            0,
            d->textureFormat,
            GL_UNSIGNED_BYTE,
            nullptr);

        // Setup PBOs if available
        if (d->pboIds[0] != 0) {
            const size_t dataSize = imgWidth * imgHeight * channels;
            if (d->pboSize != dataSize) {
                d->pboSize = dataSize;
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[0]);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[1]);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            }
        }
    } else {
        glBindTexture(GL_TEXTURE_2D, d->textureId);
    }

    // Upload texture data
    if (d->pboIds[0] != 0) {
        // Use PBO for asynchronous texture upload
        d->pboIndex = (d->pboIndex + 1) % 2;
        const int nextIndex = (d->pboIndex + 1) % 2;

        // Bind PBO to upload data from previous frame
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[d->pboIndex]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgWidth, imgHeight, d->textureFormat, GL_UNSIGNED_BYTE, nullptr);

        // Bind next PBO and update data for next frame
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, d->pboIds[nextIndex]);

        glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, d->pboSize, d->origImage.data);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    } else {
        // Direct texture upload, if we have no PBO support
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0, imgWidth, imgHeight, d->textureFormat, GL_UNSIGNED_BYTE, d->origImage.data);
    }

    // Render
    glClear(GL_COLOR_BUFFER_BIT);

    d->shaderProgram.bind();

    // Semi-static uniforms
    if (d->lastBgColor != d->bgColorVec || d->lastChannels != channels) {
        d->shaderProgram.setUniformValue("bgColor", d->bgColorVec);
        d->shaderProgram.setUniformValue("isGrayscale", channels == 1 ? 1.0f : 0.0f);
        d->lastBgColor = d->bgColorVec;
        d->lastChannels = channels;
    }

    // Only update uniforms when they change
    const float imageAspectRatio = static_cast<float>(imgWidth) / imgHeight;
    const float aspectRatio = static_cast<float>(width()) / height() / imageAspectRatio;

    if (std::abs(aspectRatio - d->lastAspectRatio) > 0.001f) {
        d->shaderProgram.setUniformValue("aspectRatio", aspectRatio);
        d->lastAspectRatio = aspectRatio;
    }

    if (d->highlightSaturation != d->lastHighlightSaturation) {
        d->shaderProgram.setUniformValue("showSaturation", d->highlightSaturation ? 1.0f : 0.0f);
        d->lastHighlightSaturation = d->highlightSaturation;
    }

    d->vao.bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    d->vao.release();

    d->shaderProgram.release();
}

bool ImageViewWidget::showImage(const cv::Mat &mat)
{
    if (mat.empty())
        return false;

    // Store reference to the original image (its data *must* not be touched externally at this point,
    // which it won't - but sadly OpenCV doesn't allow us to lock this matrix or describe immutability
    // in the type system, so we need to assume all upstream components play nice)
    d->origImage = mat;

    update();
    return true;
}

cv::Mat ImageViewWidget::currentRawImage() const
{
    return d->origImage;
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
