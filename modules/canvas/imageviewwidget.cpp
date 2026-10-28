/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMessageBox>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLExtraFunctions>
#include <QSurfaceFormat>
#include <opencv2/opencv.hpp>
#include <cmath>

#include "fabric/logging.h"

#if QT_CONFIG(opengles2)
#define USE_GLES 1
#else
#undef USE_GLES
#endif

static const char *vertexShaderSource =
#ifdef USE_GLES
    "#version 320 es\n"
#else
    "#version 410 core\n"
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
    "#version 320 es\n"
    "precision highp float;\n"
#else
    "#version 410 core\n"
    "#define lowp\n"
    "#define mediump\n"
    "#define highp\n"
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
    "        sceneCoord.y *= 1.0 / aspectRatio;\n" // Adjust for vertical centering
    "        sceneCoord.y += (1.0 - (1.0 / aspectRatio)) * 0.5;\n"
    "    }\n"
    "    if (sceneCoord.x < 0.0 || sceneCoord.x > 1.0 || "
    "        sceneCoord.y < 0.0 || sceneCoord.y > 1.0) {\n"
    "        FragColor = bgColor;\n" // Bars around image
    "    } else {\n"
    "        FragColor = texture(tex, sceneCoord);\n" // swizzle mask handles grayscale expansion
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
        : imageChanged(false),
          highlightSaturation(false),
          textureId(0),
          textureWidth(0),
          textureHeight(0),
          textureChannels(0),
          textureDepth(-1),
          textureDataType(GL_UNSIGNED_BYTE),
          textureFormat(GL_RGB),
          textureInternalFormat(GL_RGB),
          lastAspectRatio(-1.0f),
          lastHighlightSaturation(false),
          lastBgColor(-1.0f, -1.0f, -1.0f, -1.0f),
          uniLocAspectRatio(-1),
          uniLocBgColor(-1),
          uniLocShowSaturation(-1)
    {
    }

    ~Private() = default;

    QVector4D bgColorVec;
    cv::Mat glImage;   // Image reference (possibly converted for upload)
    cv::Mat bgraImage; // Reusable buffer for the BGRA version of 3-channel images
    bool imageChanged; // Whether glImage needs to be uploaded to the texture

    bool highlightSaturation;

    // OpenGL resources
    // These are heap-allocated because they are tied to a specific GL context:
    // if our context is recreated (see initializeGL()), they must be destroyed
    // and built anew rather than reused.
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    std::unique_ptr<QOpenGLBuffer> vbo;
    std::unique_ptr<QOpenGLShaderProgram> shaderProgram;

    // Optimized texture handling
    GLuint textureId;
    int textureWidth, textureHeight, textureChannels;
    int textureDepth;       // OpenCV depth type of the current texture (e.g. CV_8U)
    GLenum textureDataType; // GL upload type matching textureDepth
    GLenum textureFormat;
    GLenum textureInternalFormat;

    // Cache uniforms to avoid redundant updates
    float lastAspectRatio;
    bool lastHighlightSaturation;
    QVector4D lastBgColor;
    int uniLocAspectRatio; // glGetUniformLocation result, cached once after link
    int uniLocBgColor;
    int uniLocShowSaturation;

    void setupTextureFormat(int channels, int depth)
    {
        const bool is16bit = (depth == CV_16U);
        textureDataType = is16bit ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

        // clang-format off
        // (formatter created buggy result for this section)
        switch (channels) {
        case 1:
            textureFormat = GL_RED;
#ifdef USE_GLES
            textureInternalFormat = GL_RED; // GLES: 16-bit already down-converted in showImage
#else
            textureInternalFormat = is16bit ? GL_R16 : GL_RED;
#endif
            break;
        default: // 4 channels, showImage() converts 3-channel images to 4 channels
#ifdef USE_GLES
            // GLES doesn't support GL_BGRA
            textureInternalFormat = GL_RGBA;
            textureFormat = GL_RGBA;
#else
            textureInternalFormat = is16bit ? GL_RGBA16 : GL_RGBA;
            textureFormat = GL_BGRA;
#endif
            break;
        }
        // clang-format on
    }
};
#pragma GCC diagnostic pop

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d(std::make_unique<ImageViewWidget::Private>())
{
    d->bgColorVec = QVector4D(0.46f, 0.46f, 0.46f, 1.0f);
    setWindowTitle("Video");
    QWidget::setMinimumSize(QSize(320, 256));

    // CanvasModule paces its display rate purely through vsync, draining the frame
    // queue to the freshest frame on each repaint. That only holds if buffer swaps
    // actually block until the next vertical refresh, so request a swap interval of 1
    // explicitly instead of relying on the platform/driver default.
    QSurfaceFormat fmt = format();
    fmt.setSwapInterval(1);
    setFormat(fmt);
}

ImageViewWidget::~ImageViewWidget()
{
    // Clean up OpenGL resources while the context is still current.
    // If we have no context (it was already destroyed, e.g. because the whole
    // window went away), all GL objects are dead already and we must not call
    // any GL function - the Qt wrapper objects handle that case themselves.
    if (context() == nullptr)
        return;

    makeCurrent();
    cleanupGL();
    doneCurrent();
}

void ImageViewWidget::cleanupGL()
{
    if (d->textureId != 0) {
        glDeleteTextures(1, &d->textureId);
        d->textureId = 0;
        d->textureWidth = 0;
        d->textureHeight = 0;
        d->textureChannels = 0;
        d->textureDepth = -1;
    }

    // Destroy the context-bound GL objects, they can not be carried over
    d->vao.reset();
    d->vbo.reset();
    d->shaderProgram.reset();
}

void ImageViewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // Qt may recreate our OpenGL context and call this function again - this happens
    // when the widget is reparented (e.g. undocking its dock widget), but also when
    // the underlying window is recreated, for example after a screen configuration
    // change. Any resource from a previous context is invalid now and must go.
    cleanupGL();

    auto bgColor = QColor::fromRgb(150, 150, 150);
    float r = ((float)bgColor.darker().red()) / 255.0f;
    float g = ((float)bgColor.darker().green()) / 255.0f;
    float b = ((float)bgColor.darker().blue()) / 255.0f;
    d->bgColorVec = QVector4D(r, g, b, 1.0);
    glClearColor(r, g, b, 1.0f);

    // Compile & link shaders
    d->shaderProgram = std::make_unique<QOpenGLShaderProgram>();

    bool glOkay = true;
    glOkay = d->shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource) && glOkay;
    glOkay = d->shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource) && glOkay;

    if (!d->shaderProgram->link()) {
        glOkay = false;
        LOG_ERROR(logRoot, "Unable to link shader program: {}", d->shaderProgram->log());
    }

    // Initialize VAO & VBO
    d->vao = std::make_unique<QOpenGLVertexArrayObject>();
    d->vao->create();
    glOkay = glOkay && d->vao->isCreated();
    if (!glOkay) {
        QMessageBox::critical(
            this,
            QStringLiteral("Unable to initialize OpenGL"),
            QStringLiteral(
                "Unable to compile or link OpenGL shader or initialize vertex array object. Your system needs at "
                "least OpenGL 4.1 or GLES 3.2 to run this application.\n"
                "You may want to try to upgrade your graphics drivers, or check the application log for details."),
            QMessageBox::Ok);
        qFatal(
            "Unable to initialize OpenGL:\nVAO: %s\nShader Log: %s",
            d->vao->isCreated() ? "true" : "false",
            qPrintable(d->shaderProgram->log()));
        exit(6);
    }

    d->vao->bind();

    GLfloat vertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    d->vbo = std::make_unique<QOpenGLBuffer>();
    d->vbo->create();
    d->vbo->bind();
    d->vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    d->vbo->allocate(vertices, sizeof(vertices));

    d->shaderProgram->enableAttributeArray(0);
    d->shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(GLfloat));

    d->vbo->release();
    d->vao->release();

    // Cache uniform locations to avoid per-frame string lookups (glGetUniformLocation)
    d->uniLocAspectRatio = d->shaderProgram->uniformLocation("aspectRatio");
    d->uniLocBgColor = d->shaderProgram->uniformLocation("bgColor");
    d->uniLocShowSaturation = d->shaderProgram->uniformLocation("showSaturation");

    // Bind the sampler once to texture unit 0; it never changes
    d->shaderProgram->bind();
    d->shaderProgram->setUniformValue("tex", 0);
    d->shaderProgram->release();

    // Invalidate the uniform value cache: the uniforms of this freshly linked program
    // are at their defaults, so the values we believe we have set are not in effect.
    // Without this, a re-initialized context would render with wrong background color,
    // aspect ratio and saturation settings until they happen to change.
    d->lastAspectRatio = -1.0f;
    d->lastHighlightSaturation = !d->highlightSaturation;
    d->lastBgColor = QVector4D(-1.0f, -1.0f, -1.0f, -1.0f);

    // Canvas receives arbitrary crop widths, so we must not rely on OpenGL's default
    // 4-byte unpack alignment when uploading rows.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
}

void ImageViewWidget::paintGL()
{
    renderImage();
}

void ImageViewWidget::renderImage()
{
    if (d->glImage.empty())
        return;

    const auto imgWidth = d->glImage.cols;
    const auto imgHeight = d->glImage.rows;
    const auto channels = d->glImage.channels();
    const auto depth = d->glImage.depth();

    // Setup or recreate texture when dimensions, channel count, or bit-depth change
    bool textureCreated = false;
    if (d->textureId == 0 || d->textureWidth != imgWidth || d->textureHeight != imgHeight
        || d->textureChannels != channels || d->textureDepth != depth) {
        if (d->textureId != 0)
            glDeleteTextures(1, &d->textureId);

        glGenTextures(1, &d->textureId);
        glBindTexture(GL_TEXTURE_2D, d->textureId);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        d->setupTextureFormat(channels, depth);
        d->textureWidth = imgWidth;
        d->textureHeight = imgHeight;
        d->textureChannels = channels;
        d->textureDepth = depth;

        // Allocate texture storage
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            d->textureInternalFormat,
            imgWidth,
            imgHeight,
            0,
            d->textureFormat,
            d->textureDataType,
            nullptr);

        // For single-channel textures, set a swizzle mask so the GPU expands
        // (r) -> (r, r, r, 1) automatically
        if (channels == 1) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
        }

        textureCreated = true;
    } else {
        glBindTexture(GL_TEXTURE_2D, d->textureId);
    }

    // Upload texture data only if we have a new image, other repaints (e.g. on resize) reuse the texture
    if (d->imageChanged || textureCreated) {
        // Set OpenGL unpack state to handle row stride
        const int unpackRowLengthPx = static_cast<int>(d->glImage.step / d->glImage.elemSize());
        glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLengthPx);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            imgWidth,
            imgHeight,
            d->textureFormat,
            d->textureDataType,
            d->glImage.data);
        d->imageChanged = false;
    }

    // Render
    glClear(GL_COLOR_BUFFER_BIT);

    d->shaderProgram->bind();

    // Only update uniforms when their values change
    if (d->lastBgColor != d->bgColorVec) {
        d->shaderProgram->setUniformValue(d->uniLocBgColor, d->bgColorVec);
        d->lastBgColor = d->bgColorVec;
    }

    const float imageAspectRatio = static_cast<float>(imgWidth) / imgHeight;
    const float aspectRatio = static_cast<float>(width()) / height() / imageAspectRatio;

    if (std::abs(aspectRatio - d->lastAspectRatio) > 0.001f) {
        d->shaderProgram->setUniformValue(d->uniLocAspectRatio, aspectRatio);
        d->lastAspectRatio = aspectRatio;
    }

    if (d->highlightSaturation != d->lastHighlightSaturation) {
        d->shaderProgram->setUniformValue(d->uniLocShowSaturation, d->highlightSaturation ? 1.0f : 0.0f);
        d->lastHighlightSaturation = d->highlightSaturation;
    }

    d->vao->bind();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    d->vao->release();

    d->shaderProgram->release();
}

bool ImageViewWidget::showImage(const cv::Mat &mat)
{
    if (mat.empty())
        return false;

    // we only have texture formats for grayscale, BGR and BGRA images
    const auto channels = mat.channels();
    if (channels != 1 && channels != 3 && channels != 4)
        return false;

#ifdef USE_GLES
    // On GLES, we need to convert BGR/BGRA to RGBA on the CPU since GLES doesn't
    // support GL_BGR/GL_BGRA formats (and GPUs have no 3-byte pixel format anyway)
    if (mat.channels() == 3) {
        cv::cvtColor(mat, d->glImage, cv::COLOR_BGR2RGBA);
    } else if (mat.channels() == 4) {
        cv::cvtColor(mat, d->glImage, cv::COLOR_BGRA2RGBA);
    } else {
        d->glImage = mat;
    }
    // GLES lacks 16-bit normalized texture support without extensions; downconvert to 8-bit
    {
        const auto depth = d->glImage.depth();
        if (depth == CV_16U || depth == CV_16S)
            d->glImage.convertTo(d->glImage, CV_8U, 1.0 / 256.0);
        else if (depth != CV_8U)
            d->glImage.convertTo(d->glImage, CV_8U);
    }
#else
    // Store reference to the original image (its data *must* not be touched externally at this point,
    // which it won't - but sadly OpenCV doesn't allow us to lock this matrix or describe immutability
    // in the type system, so we need to assume all upstream components play nice).
    // Fortunately, on desktop OpenGL, we can let the GPU handle BGR/BGRA conversion.
    d->glImage = mat;

    // Normalize to a GL-uploadable depth: CV_8U and CV_16U are rendered natively;
    // everything else is converted to the closest sensible target.
    {
        const auto depth = d->glImage.depth();
        if (depth == CV_16S || depth == CV_32S) {
            // Shift signed -> unsigned 16-bit
            d->glImage.convertTo(d->glImage, CV_16U, 1.0, 32768.0);
        } else if (depth == CV_32F || depth == CV_64F) {
            // Assume values in [0, 1]; scale to full 16-bit range
            d->glImage.convertTo(d->glImage, CV_16U, 65535.0);
        } else if (depth != CV_8U && depth != CV_16U) {
            d->glImage.convertTo(d->glImage, CV_8U);
        }
    }

    // GPUs have no 3-byte pixel format, so drivers expand 3-channel images on upload, and at least
    // Mesa does that with slow generic code. OpenCV's SIMD conversion is several times faster.
    // This only appends an alpha channel and keeps the BGR channel order.
    if (d->glImage.channels() == 3) {
        cv::cvtColor(d->glImage, d->bgraImage, cv::COLOR_BGR2BGRA);
        d->glImage = d->bgraImage;
    }
#endif

    // We only upload on the next paint, but producers never modify an image they have published
    // (they recycle buffers only via matEnsureExclusive()), so even a view into a larger image stays
    // valid until then. OpenGL can skip padding between rows, but only if the row stride is a whole
    // number of pixels, which is not guaranteed for images that wrap external memory.
    if (d->glImage.step[0] % d->glImage.elemSize() != 0)
        d->glImage = d->glImage.clone();

    d->imageChanged = true;
    update();
    return true;
}

cv::Mat ImageViewWidget::currentImage() const
{
    return d->glImage;
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

bool ImageViewWidget::usesGLES() const
{
#ifdef USE_GLES
    return true;
#else
    return false;
#endif
}
