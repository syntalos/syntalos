/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "canvaswindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QCheckBox>
#include <QPushButton>
#include <QGraphicsEffect>
#include <QGraphicsOpacityEffect>
#include <QSplitter>
#include <QTimer>
#include <QScreen>

#include <algorithm>

#include "imageviewwidget.h"
#include "histogramwidget.h"

class InvertOpaqueEffect : public QGraphicsEffect
{
    Q_OBJECT
public:
    explicit InvertOpaqueEffect(double opacity, QObject *parent = nullptr)
        : QGraphicsEffect(parent),
          m_opacity(opacity)
    {
    }

    void draw(QPainter *painter) override
    {
        QPoint offset;
        QPixmap pixmap = sourcePixmap(Qt::DeviceCoordinates, &offset, QGraphicsEffect::NoPad);

        // apply opacity by modifying pixmap
        QPixmap semiTransparentPixmap(pixmap.size());
        semiTransparentPixmap.fill(Qt::transparent);
        QPainter p(&semiTransparentPixmap);
        p.setOpacity(m_opacity);
        p.drawPixmap(0, 0, pixmap);
        p.end();

        // apply color inversion by modifying image
        QImage image = semiTransparentPixmap.toImage();
        image.invertPixels(QImage::InvertRgb);

        // draw the final image
        painter->drawImage(offset, image);
    }

private:
    double m_opacity;
};

class ToolsOverlayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolsOverlayWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);

        auto olEffect = new InvertOpaqueEffect(0.8, this);
        setGraphicsEffect(olEffect);

        auto mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(2, 2, 2, 2);
        setLayout(mainLayout);

        // add toggle button
        m_toggleBtn = new QPushButton(this);
        m_toggleBtn->setFlat(true);
        m_toggleBtn->setIcon(QIcon::fromTheme("arrow-right"));
        m_toggleBtn->setCheckable(true);
        mainLayout->addWidget(m_toggleBtn, 0, Qt::AlignLeft);
        connect(m_toggleBtn, &QPushButton::clicked, this, &ToolsOverlayWidget::toggleVisibility);

        // configure controls container
        m_controls = new QWidget(this);
        auto layout = new QHBoxLayout(m_controls);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        m_cbHlSaturation = new QCheckBox("Highlight saturation", m_controls);
        layout->addWidget(m_cbHlSaturation);
        connect(m_cbHlSaturation, &QCheckBox::toggled, this, [this](bool enabled) {
            Q_EMIT highlightSaturationChanged(enabled);
        });

        m_cbShowHistogram = new QCheckBox("Show histogram", m_controls);
        layout->addWidget(m_cbShowHistogram);
        connect(m_cbShowHistogram, &QCheckBox::toggled, this, [this](bool enabled) {
            Q_EMIT showHistogramChanged(enabled);
        });

        mainLayout->addWidget(m_controls);

        // initially hide the controls
        m_controls->setVisible(false);
        adjustSize();
    }

    bool highlightSaturation() const
    {
        return m_cbHlSaturation->isChecked();
    }

    void setHighlightSaturation(bool enabled)
    {
        m_cbHlSaturation->setChecked(enabled);
    }

    bool showHistogram() const
    {
        return m_cbShowHistogram->isChecked();
    }

    void setShowHistogram(bool enabled)
    {
        m_cbShowHistogram->setChecked(enabled);
    }

public slots:
    void toggleVisibility()
    {
        if (m_controls->isVisible()) {
            m_toggleBtn->setIcon(QIcon::fromTheme("arrow-right"));
            m_controls->setVisible(false);
        } else {
            m_toggleBtn->setIcon(QIcon::fromTheme("arrow-left"));
            m_controls->setVisible(true);
        }

        adjustSize();
    }

signals:
    void highlightSaturationChanged(bool enabled);
    void showHistogramChanged(bool enabled);

private:
    QPushButton *m_toggleBtn;
    QWidget *m_controls;

    QCheckBox *m_cbHlSaturation;
    QCheckBox *m_cbShowHistogram;
};

CanvasWindow::CanvasWindow(QuillLogger *logger, QWidget *parent)
    : QWidget(parent),
      m_log(logger)
{
    setWindowTitle("Canvas");

    m_imgView = new ImageViewWidget(this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(QStringLiteral("Empty"));
    QFont font;
    font.setStyleHint(QFont::Monospace, QFont::PreferMatch);
    font.setFamily("Hack, Fira Code, Noto Mono, Monospace");
    m_statusLabel->setFont(font);
    setMinimumSize(m_imgView->minimumSize());
    // sensible default size
    resize(800, 600);

    m_splitter = new QSplitter(this);
    m_splitter->setOrientation(Qt::Vertical);

    m_histogramWidget = new HistogramWidget(this);
    m_histogramWidget->setMinimumHeight(50);
    m_histLogarithmicCb = new QCheckBox("Logarithmic", m_histogramWidget);
    auto hgCtlEffect = new QGraphicsOpacityEffect(m_histLogarithmicCb);
    hgCtlEffect->setOpacity(0.6);
    m_histLogarithmicCb->setGraphicsEffect(hgCtlEffect);
    m_histogramWidget->setVisible(false);

    auto container = new QWidget(this);
    auto clayout = new QHBoxLayout;
    clayout->setContentsMargins(0, 0, 0, 0);
    clayout->setSpacing(0);
    clayout->addWidget(m_statusLabel);
    clayout->addStretch();
    container->setLayout(clayout);

    m_splitter->addWidget(m_imgView);
    m_splitter->addWidget(m_histogramWidget);
    m_splitter->setStretchFactor(0, 4);
    m_splitter->setStretchFactor(1, 8);

    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter);
    layout->addWidget(container);
    setLayout(layout);

    setContentsMargins(0, 0, 0, 0);

    auto pal = palette();
    pal.setColor(QPalette::Window, QColor::fromRgb(150, 150, 150).darker());
    setPalette(pal);

    pal = m_statusLabel->palette();
    pal.setColor(QPalette::WindowText, Qt::white);
    m_statusLabel->setPalette(pal);

    // histogram timer
    m_histTimer = new QTimer(this);
    m_histTimer->setInterval(50);
    connect(m_histTimer, &QTimer::timeout, this, &CanvasWindow::updateHistogram);
    connect(m_histLogarithmicCb, &QCheckBox::toggled, this, [this]() {
        m_histNeedsUpdate = true;
    });

    // construct tools overlay
    m_toolsOverlay = new ToolsOverlayWidget(this);
    m_toolsOverlay->hide();
    setMouseTracking(true);

    connect(m_toolsOverlay, &ToolsOverlayWidget::highlightSaturationChanged, this, [this](bool enabled) {
        m_imgView->setHighlightSaturation(enabled);
    });
    connect(m_toolsOverlay, &ToolsOverlayWidget::showHistogramChanged, this, [this](bool enabled) {
        m_histogramWidget->setVisible(enabled);
        if (enabled) {
            m_histNeedsUpdate = true;
            m_histTimer->start();
        } else {
            m_histTimer->stop();
            m_histogramWidget->setIdle();
        }
    });
}

void CanvasWindow::setLogger(QuillLogger *logger)
{
    m_log = logger;
}

void CanvasWindow::showImage(const cv::Mat &mat)
{
    if (isVisible() && m_imgView->showImage(mat))
        m_histNeedsUpdate = true;
}

void CanvasWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

qreal CanvasWindow::displayRefreshRate() const
{
    qreal hz = 0;
    if (const auto scr = screen())
        hz = scr->refreshRate();

    // some platforms/drivers report 0, fall back to a safe default
    if (hz <= 10.0)
        hz = 60.0;

    // guard against absurd values from a misbehaving driver
    // yes, 480 Hz monitors exist, but we are not designing
    // a video display for dragonflies (and even they would be
    // very happy with a 300 Hz video)
    return std::clamp(hz, 30.0, 360.0);
}

bool CanvasWindow::highlightSaturation() const
{
    return m_imgView->highlightSaturation();
}

void CanvasWindow::setHighlightSaturation(bool enabled)
{
    m_toolsOverlay->setHighlightSaturation(enabled);
}

void CanvasWindow::setHistogramVisible(bool show)
{
    m_toolsOverlay->setShowHistogram(show);
}

bool CanvasWindow::histogramVisible() const
{
    return m_toolsOverlay->showHistogram();
}

bool CanvasWindow::histogramLogarithmic() const
{
    return m_histLogarithmicCb->isChecked();
}

void CanvasWindow::setHistogramLogarithmic(bool logarithmic)
{
    m_histLogarithmicCb->setChecked(logarithmic);
}

QByteArray CanvasWindow::histogramSplitterState() const
{
    return m_splitter->saveState();
}

void CanvasWindow::restoreHistogramSplitterState(const QByteArray &state)
{
    m_splitter->restoreState(state);
}

template<bool depth8, bool swapRedBlue>
static void computeHistogram(const cv::Mat &image, Histograms *hists, bool grayscale, bool logarithmic = false)
{
    typedef typename std::conditional<depth8, uint8_t, uint16_t>::type ImageType;

    if (!hists)
        return;

    // Count in integers and convert to float once: incrementing floats is slow when neighboring pixels
    // share a value (as they do in flat image areas), and floats stop counting beyond 2^24 per bin
    uint32_t counts[3][256] = {};

    const int h = image.rows;
    const int w = image.cols;

    if (grayscale) {
        for (int i = 0; i < h; i++) {
            auto imageLine = image.ptr<ImageType>(i);
            for (int j = 0; j < w; j++) {
                uint8_t gray = depth8 ? imageLine[j] : imageLine[j] >> 8;
                counts[0][gray]++;
            }
        }
    } else {
        // pixels are `channels` elements apart, and we only read the first three (B, G, R),
        // which skips the alpha value of 4-channel images.
        // The channels are written out, because GCC does not unroll a loop over them at -O2 and is twice as slow then.
        const int channels = image.channels();
        for (int i = 0; i < h; i++) {
            auto imageLine = image.ptr<ImageType>(i);
            for (int j = 0; j < w; j++) {
                const auto pixel = imageLine + (j * channels);
                counts[0][depth8 ? pixel[0] : pixel[0] >> 8]++;
                counts[1][depth8 ? pixel[1] : pixel[1] >> 8]++;
                counts[2][depth8 ? pixel[2] : pixel[2] >> 8]++;
            }
        }
    }

    // grayscale counts go to the red histogram, BGR input needs to be swapped to RGB
    float *histograms[3] = {hists->red, hists->green, hists->blue};
    if (swapRedBlue && !grayscale)
        std::swap(histograms[0], histograms[2]);
    for (int ch = 0; ch < 3; ch++) {
        for (int i = 0; i < 256; i++)
            histograms[ch][i] = logarithmic ? std::log2f(counts[ch][i] + 1.0f) : static_cast<float>(counts[ch][i]);
    }
}

void CanvasWindow::updateHistogram()
{
    // only count again if there is a new image, or the histogram was just enabled or switched to logarithmic
    if (!m_histNeedsUpdate)
        return;

    const auto image = m_imgView->currentImage();
    if (image.empty())
        return;
    m_histNeedsUpdate = false;

    // the image is in the format that was uploaded to the GPU: 8-bit or 16-bit unsigned, with 1 or 4 channels
    const bool grayscale = image.channels() == 1;
    const bool logarithmic = m_histLogarithmicCb->isChecked();
    auto hists = m_histogramWidget->unusedHistograms();

    // Determine if we need to swap red and blue channels for correct histogram display.
    // The histogram widget expects RGB order, but we may have BGR data on desktop OpenGL.
    // Sadly, OpenCV provides no way to query the colorspace/order of a cv::Mat.
#if QT_CONFIG(opengles2)
    constexpr bool swapRedBlue = false; // GLES images are already converted to RGB
#else
    constexpr bool swapRedBlue = true; // Desktop GL images are BGR, need to swap for display
#endif

    if (image.depth() == CV_8U)
        computeHistogram<true, swapRedBlue>(image, hists, grayscale, logarithmic);
    else
        computeHistogram<false, swapRedBlue>(image, hists, grayscale, logarithmic);

    m_histogramWidget->swapHistograms(grayscale);
}

void CanvasWindow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_toolsOverlay->show();
}

void CanvasWindow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_toolsOverlay->hide();
}

#include "canvaswindow.moc"
