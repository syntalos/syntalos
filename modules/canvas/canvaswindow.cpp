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

#include <QDebug>
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

CanvasWindow::CanvasWindow(QWidget *parent)
    : QWidget(parent)
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

    auto splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Vertical);

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

    splitter->addWidget(m_imgView);
    splitter->addWidget(m_histogramWidget);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 8);

    auto layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(splitter);
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
            m_histTimer->start();
        } else {
            m_histTimer->stop();
            m_histogramWidget->setIdle();
        }
    });
}

void CanvasWindow::showImage(const cv::Mat &mat)
{
    if (isVisible())
        m_imgView->showImage(mat);
}

void CanvasWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
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

template<bool depth8, bool swapRedBlue>
static void computeHistogram(const cv::Mat &image, Histograms *hists, bool grayscale, bool logarithmic = false)
{
    typedef typename std::conditional<depth8, uint8_t, uint16_t>::type ImageType;

    if (!hists)
        return;

    auto histRed = hists->red;
    auto histGreen = hists->green;
    auto histBlue = hists->blue;
    for (int i = 0; i < 256; i++)
        histRed[i] = histGreen[i] = histBlue[i] = 0;

    const int h = image.rows;
    const int w = image.cols;

    if (grayscale) {
        for (int i = 0; i < h; i++) {
            auto imageLine = image.ptr<ImageType>(i);
            for (int j = 0; j < w; j++) {
                uint8_t gray = depth8 ? imageLine[j] : imageLine[j] >> 8;
                histRed[gray]++;
            }
        }
        if (logarithmic) {
            for (int i = 0; i < 256; i++)
                histRed[i] = std::log2f(histRed[i] + 1.0f);
        }
    } else {
        float *histograms[3];
        if constexpr (swapRedBlue) {
            // BGR input -> map to RGB 'histograms' array
            histograms[0] = histBlue;
            histograms[1] = histGreen;
            histograms[2] = histRed;
        } else {
            // RGB input -> no channel swap needed
            histograms[0] = histRed;
            histograms[1] = histGreen;
            histograms[2] = histBlue;
        }

        for (int i = 0; i < h; i++) {
            auto imageLine = image.ptr<cv::Vec<ImageType, 3>>(i);
            for (int j = 0; j < w; j++) {
                auto &pixel = imageLine[j];
                for (int ch = 0; ch < 3; ch++) {
                    uint8_t value = depth8 ? pixel[ch] : pixel[ch] >> 8;
                    histograms[ch][value]++;
                }
            }
        }
        if (logarithmic) {
            for (auto *histogram : histograms) {
                for (int i = 0; i < 256; i++)
                    histogram[i] = std::log2f(histogram[i] + 1.0f);
            }
        }
    }
}

void CanvasWindow::updateHistogram()
{
    auto hists = m_histogramWidget->unusedHistograms();
    const auto image = m_imgView->currentImage();

    if (image.empty())
        return;

    bool grayscale;
    if (image.channels() == 1)
        grayscale = true;
    else if (image.channels() >= 3)
        grayscale = false;
    else
        return;

    const bool logarithmic = m_histLogarithmicCb->isChecked();
    const auto imageDepth = image.depth();

    // Determine if we need to swap red and blue channels for correct histogram display.
    // The histogram widget expects RGB order, but we may have BGR data on desktop OpenGL.
    // Sadly, OpenCV provides no way to query the colorspace/order of a cv::Mat.
#if defined(QT_OPENGL_ES)
    constexpr bool swapRedBlue = false; // GLES images are already converted to RGB
#else
    constexpr bool swapRedBlue = true; // Desktop GL images are BGR, need to swap for display
#endif

    if (imageDepth == CV_8U || imageDepth == CV_8S) {
        computeHistogram<true, swapRedBlue>(image, hists, grayscale, logarithmic);
    } else if (imageDepth == CV_16U || imageDepth == CV_16S) {
        computeHistogram<false, swapRedBlue>(image, hists, grayscale, logarithmic);
    } else {
        m_histTimer->stop();
        qWarning().noquote() << "Unsupported image format for histogram computation, disabling rendering.";
    }

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
