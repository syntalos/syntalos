/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "videotransform.h"

#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <opencv2/opencv.hpp>

VideoTransform::VideoTransform()
    : QObject()
{
    m_originalSize = QSize(INT_MAX, INT_MAX);
}

void VideoTransform::setOriginalSize(const QSize &size)
{
    m_originalSize = size;
}

QSize VideoTransform::resultSize()
{
    return m_originalSize;
}

bool VideoTransform::allowOnlineModify() const
{
    return false;
}

void VideoTransform::start() {}

void VideoTransform::stop() {}

QVariantHash VideoTransform::toVariantHash()
{
    return {};
}

void VideoTransform::fromVariantHash(const QVariantHash &) {}

CropTransform::CropTransform()
    : VideoTransform()
{
}

QString CropTransform::name() const
{
    return QStringLiteral("Crop Frames");
}

QIcon CropTransform::icon() const
{
    return QIcon::fromTheme("transform-crop");
}

void CropTransform::createSettingsUi(QWidget *parent)
{
    if (m_sizeInfoLabel != nullptr) {
        qCritical().noquote() << "Tried to create CropTransform UI twice. This is not allowed!";
        return;
    }
    m_sizeInfoLabel = new QLabel(parent);
    connect(m_sizeInfoLabel, &QWidget::destroyed, [this]() {
        m_sizeInfoLabel = nullptr;
        m_sbWidth = nullptr;
        m_sbHeight = nullptr;
        m_sbX = nullptr;
        m_sbY = nullptr;
    });

    m_sbX = new QSpinBox(parent);
    m_sbX->setSuffix("px");
    m_sbX->setValue(m_roi.x);
    m_sbX->setMinimumWidth(100);

    m_sbWidth = new QSpinBox(parent);
    m_sbWidth->setSuffix("px");
    m_sbWidth->setValue(m_roi.width);
    m_sbWidth->setMinimumWidth(100);

    m_sbY = new QSpinBox(parent);
    m_sbY->setSuffix("px");
    m_sbY->setValue(m_roi.y);
    m_sbY->setMinimumWidth(100);

    m_sbHeight = new QSpinBox(parent);
    m_sbHeight->setSuffix("px");
    m_sbHeight->setValue(m_roi.height);
    m_sbHeight->setMinimumWidth(100);

    connect(m_sbX, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        QTimer::singleShot(300, m_sbX, &QSpinBox::editingFinished);
    });
    connect(m_sbX, &QSpinBox::editingFinished, [this]() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_roi.x = m_sbX->value();
        m_onlineModified = true;
        checkAndUpdateRoi();
    });

    connect(m_sbWidth, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        QTimer::singleShot(300, m_sbWidth, &QSpinBox::editingFinished);
    });
    connect(m_sbWidth, &QSpinBox::editingFinished, [this]() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_roi.width = m_sbWidth->value();
        m_onlineModified = true;
        checkAndUpdateRoi();
    });

    connect(m_sbY, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        QTimer::singleShot(300, m_sbY, &QSpinBox::editingFinished);
    });
    connect(m_sbY, &QSpinBox::editingFinished, [this]() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_roi.y = m_sbY->value();
        m_onlineModified = true;
        checkAndUpdateRoi();
    });

    connect(m_sbHeight, qOverload<int>(&QSpinBox::valueChanged), [this](int) {
        QTimer::singleShot(300, m_sbHeight, &QSpinBox::editingFinished);
    });
    connect(m_sbHeight, &QSpinBox::editingFinished, [this]() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_roi.height = m_sbHeight->value();
        m_onlineModified = true;
        checkAndUpdateRoi();
    });

    auto formLayout = new QFormLayout(parent);
    formLayout->addRow(QStringLiteral("Start X:"), m_sbX);
    formLayout->addRow(QStringLiteral("Width:"), m_sbWidth);
    formLayout->addRow(QStringLiteral("Start Y:"), m_sbY);
    formLayout->addRow(QStringLiteral("Height:"), m_sbHeight);
    formLayout->addWidget(m_sizeInfoLabel);
    parent->setLayout(formLayout);

    // Update initial values and safe ranges
    checkAndUpdateRoi();
}

bool CropTransform::allowOnlineModify() const
{
    return true;
}

QSize CropTransform::resultSize()
{
    if (m_activeRoi.empty())
        return m_originalSize;

    const std::lock_guard<std::mutex> lock(m_mutex);
    checkAndUpdateRoi();
    return {m_activeRoi.width, m_activeRoi.height};
}

void CropTransform::start()
{
    if (m_roi.empty()) {
        m_roi.width = m_originalSize.width();
        m_roi.height = m_originalSize.height();
    }

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        checkAndUpdateRoi();
    }
    m_activeRoi = m_roi;

    const auto rSize = resultSize();
    m_activeOutSize = cv::Size(rSize.width(), rSize.height());
    m_onlineModified = false;
}

void CropTransform::process(cv::Mat &image)
{
    // handle the simple case: no online modifications
    if (!m_onlineModified) {
        image = image(m_activeRoi);
        return;
    }

    // online modification: since we are not allowed to alter the image dimensions,
    // we add black bars for now or scale accordingly
    const std::lock_guard<std::mutex> lock(m_mutex);

    // actually do the "fake resizing"
    cv::Mat cropScaleMat(image, m_roi);
    cv::Mat outMat = cv::Mat::zeros(m_activeOutSize, image.type());

    if ((m_roi.width <= m_activeOutSize.width) && (m_roi.height <= m_activeOutSize.height)) {
        // the crop dimensions are smaller than or equal to our output, so we can simply center it
        const int offsetX = (m_activeOutSize.width - m_roi.width) / 2;
        const int offsetY = (m_activeOutSize.height - m_roi.height) / 2;
        cv::Rect targetRect(offsetX, offsetY, m_roi.width, m_roi.height);
        cropScaleMat.copyTo(outMat(targetRect));
    } else {
        // the crop dimensions are larger than our output, we need some scaling
        double scaleFactor = 1;
        if (cropScaleMat.cols > outMat.cols)
            scaleFactor = (double)outMat.cols / (double)cropScaleMat.cols;
        if (cropScaleMat.rows > outMat.rows) {
            double scale = (double)outMat.rows / (double)cropScaleMat.rows;
            scaleFactor = (scale < scaleFactor) ? scale : scaleFactor;
        }

        cv::resize(cropScaleMat, cropScaleMat, cv::Size(), scaleFactor, scaleFactor);
        cropScaleMat.copyTo(outMat(cv::Rect(
            (outMat.cols - cropScaleMat.cols) / 2,
            (outMat.rows - cropScaleMat.rows) / 2,
            cropScaleMat.cols,
            cropScaleMat.rows)));
    }

    image = outMat;
}

QVariantHash CropTransform::toVariantHash()
{
    QVariantHash var;
    var.insert("crop_x", m_roi.x);
    var.insert("crop_y", m_roi.y);
    var.insert("crop_width", m_roi.width);
    var.insert("crop_height", m_roi.height);
    return var;
}

void CropTransform::fromVariantHash(const QVariantHash &settings)
{
    m_roi.x = settings.value("crop_x", 0).toInt();
    m_roi.y = settings.value("crop_y", 0).toInt();
    m_roi.width = settings.value("crop_width", 0).toInt();
    m_roi.height = settings.value("crop_height", 0).toInt();
    checkAndUpdateRoi();
}

void CropTransform::checkAndUpdateRoi()
{
    // Ensure ROI stays within bounds and has valid dimensions
    m_roi.x = std::max(0, std::min(m_roi.x, m_originalSize.width() - 1));
    m_roi.y = std::max(0, std::min(m_roi.y, m_originalSize.height() - 1));

    // Adjust width and height to stay within bounds
    m_roi.width = std::max(1, std::min(m_roi.width, m_originalSize.width() - m_roi.x));
    m_roi.height = std::max(1, std::min(m_roi.height, m_originalSize.height() - m_roi.y));

    // give user some info as to what we are actually doing, if the GUI is set up
    if (m_sizeInfoLabel != nullptr) {
        m_sizeInfoLabel->setText(QStringLiteral("Result size: %1x%2px (x%3 - w%4; y%5 - h%6)\n"
                                                "Original size: %7x%8px")
                                     .arg(m_roi.width)
                                     .arg(m_roi.height)
                                     .arg(m_roi.x)
                                     .arg(m_roi.width + m_roi.x)
                                     .arg(m_roi.y)
                                     .arg(m_roi.height + m_roi.y)
                                     .arg(m_originalSize.width())
                                     .arg(m_originalSize.height()));

        m_sbX->setRange(0, std::max(0, m_originalSize.width() - m_sbWidth->value()));
        m_sbWidth->setRange(1, std::max(1, m_originalSize.width() - m_sbX->value()));
        m_sbY->setRange(0, std::max(0, m_originalSize.height() - m_sbHeight->value()));
        m_sbHeight->setRange(1, std::max(1, m_originalSize.height() - m_sbY->value()));

        // Update spinboxes with the correct values - now they represent actual width/height, not end coordinates
        if (m_sbWidth->value() != m_roi.width) {
            m_sbWidth->blockSignals(true);
            m_sbWidth->setValue(m_roi.width);
            m_sbWidth->blockSignals(false);
        }
        if (m_sbHeight->value() != m_roi.height) {
            m_sbHeight->blockSignals(true);
            m_sbHeight->setValue(m_roi.height);
            m_sbHeight->blockSignals(false);
        }
        if (m_sbX->value() != m_roi.x) {
            m_sbX->blockSignals(true);
            m_sbX->setValue(m_roi.x);
            m_sbX->blockSignals(false);
        }
        if (m_sbY->value() != m_roi.y) {
            m_sbY->blockSignals(true);
            m_sbY->setValue(m_roi.y);
            m_sbY->blockSignals(false);
        }
    }
}

ScaleTransform::ScaleTransform()
    : VideoTransform()
{
}

QString ScaleTransform::name() const
{
    return QStringLiteral("Scale Frames");
}

QIcon ScaleTransform::icon() const
{
    return QIcon::fromTheme("transform-scale");
}

void ScaleTransform::createSettingsUi(QWidget *parent)
{
    auto *sbSymScale = new QDoubleSpinBox(parent);
    sbSymScale->setRange(0.01, 10);
    sbSymScale->setValue(m_scaleFactor);
    connect(sbSymScale, qOverload<double>(&QDoubleSpinBox::valueChanged), [this](double value) {
        m_scaleFactor = value;
    });

    auto formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Scale Factor:"), sbSymScale);
    parent->setLayout(formLayout);
}

QSize ScaleTransform::resultSize()
{
    return {
        static_cast<int>(std::round(m_originalSize.width() * m_scaleFactor)),
        static_cast<int>(std::round(m_originalSize.height() * m_scaleFactor))};
}

void ScaleTransform::process(cv::Mat &image)
{
    // Only process if scale factor is not 1.0 to avoid unnecessary work
    if (std::abs(m_scaleFactor - 1.0) < 1e-6)
        return;

    cv::Mat outMat;
    cv::resize(image, outMat, cv::Size(), m_scaleFactor, m_scaleFactor, cv::INTER_LINEAR);
    image = outMat;
}

QVariantHash ScaleTransform::toVariantHash()
{
    QVariantHash var;
    var.insert("scale_factor", m_scaleFactor);
    return var;
}

void ScaleTransform::fromVariantHash(const QVariantHash &settings)
{
    m_scaleFactor = settings.value("scale_factor", 1).toDouble();
}

FalseColorTransform::FalseColorTransform()
    : VideoTransform()
{
}

QString FalseColorTransform::name() const
{
    return QStringLiteral("False Colors");
}

QIcon FalseColorTransform::icon() const
{
    return QIcon::fromTheme("color-profile");
}

void FalseColorTransform::createSettingsUi(QWidget *parent)
{
    auto formLayout = new QFormLayout(parent);
    formLayout->addRow(QStringLiteral("This transformation has no settings."), new QLabel(parent));
    parent->setLayout(formLayout);
}

void FalseColorTransform::process(cv::Mat &image)
{
    // convert the image to grayscale if it's not already
    cv::Mat gray;
    if (image.channels() >= 3)
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else
        gray = image;

    // apply a colormap to create a false color image
    cv::applyColorMap(gray, image, cv::COLORMAP_JET);
}

HistNormTransform::HistNormTransform()
    : VideoTransform()
{
}

QString HistNormTransform::name() const
{
    return QStringLiteral("Normalize Histogram");
}

QIcon HistNormTransform::icon() const
{
    return QIcon::fromTheme("histogram-symbolic");
}

void HistNormTransform::createSettingsUi(QWidget *parent)
{
    auto formLayout = new QFormLayout(parent);
    formLayout->addRow(QStringLiteral("This transformation has no settings."), new QLabel(parent));
    parent->setLayout(formLayout);
}

void HistNormTransform::process(cv::Mat &image)
{
    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    // apply histogram equalization to each channel
    for (auto &channel : channels)
        cv::equalizeHist(channel, channel);

    // merge the channels back together
    cv::merge(channels, image);
}
