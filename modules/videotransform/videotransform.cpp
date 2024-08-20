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

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <opencv2/opencv.hpp>

VideoTransform::VideoTransform()
    : QObject()
{
    m_originalSize = QSize(999999, 999999);
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
    return QVariantHash();
}

void VideoTransform::fromVariantHash(const QVariantHash &) {}

CropTransform::CropTransform()
    : VideoTransform(),
      m_sizeInfoLabel(nullptr)
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
    connect(m_sizeInfoLabel, &QWidget::destroyed, [&]() {
        m_sizeInfoLabel = nullptr;
    });

    auto sbWidth = new QSpinBox(parent);
    sbWidth->setRange((m_originalSize.width() > 10) ? 10 : 0, m_originalSize.width());
    sbWidth->setSuffix("px");
    sbWidth->setValue(m_roi.width);
    sbWidth->setMinimumWidth(100);

    auto sbX = new QSpinBox(parent);
    sbX->setRange(0, m_originalSize.width() - 10);
    sbX->setSuffix("px");
    sbX->setValue(m_roi.x);
    sbX->setMinimumWidth(100);

    auto sbHeight = new QSpinBox(parent);
    sbHeight->setRange((m_originalSize.height() > 10) ? 10 : 0, m_originalSize.height());
    sbHeight->setSuffix("px");
    sbHeight->setValue(m_roi.height);
    sbHeight->setMinimumWidth(100);

    auto sbY = new QSpinBox(parent);
    sbY->setRange(0, m_originalSize.height() - 10);
    sbY->setSuffix("px");
    sbY->setValue(m_roi.y);
    sbY->setMinimumWidth(100);

    connect(sbWidth, qOverload<int>(&QSpinBox::valueChanged), [=](int) {
        QTimer::singleShot(600, sbWidth, &QSpinBox::editingFinished);
    });
    connect(sbWidth, &QSpinBox::editingFinished, [=, this]() {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.width = sbWidth->value() - m_roi.x;
            m_onlineModified = true;

            checkAndUpdateRoi();
        }
        sbHeight->setValue(m_roi.height + m_roi.y);
        sbX->setValue(m_roi.x);
        sbY->setValue(m_roi.y);
    });

    connect(sbHeight, qOverload<int>(&QSpinBox::valueChanged), [=](int) {
        QTimer::singleShot(500, sbHeight, &QSpinBox::editingFinished);
    });
    connect(sbHeight, &QSpinBox::editingFinished, [=, this]() {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.height = sbHeight->value() - m_roi.y;
            m_onlineModified = true;
            checkAndUpdateRoi();
        }
        sbWidth->setValue(m_roi.width + m_roi.x);
        sbX->setValue(m_roi.x);
        sbY->setValue(m_roi.y);
    });

    connect(sbX, qOverload<int>(&QSpinBox::valueChanged), [=](int) {
        QTimer::singleShot(500, sbX, &QSpinBox::editingFinished);
    });
    connect(sbX, &QSpinBox::editingFinished, [=, this]() {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.x = sbX->value();
            m_onlineModified = true;
            checkAndUpdateRoi();
        }
        sbWidth->setValue(m_roi.width + m_roi.x);
        sbHeight->setValue(m_roi.height + m_roi.y);
        sbY->setValue(m_roi.y);
    });

    connect(sbY, qOverload<int>(&QSpinBox::valueChanged), [=](int) {
        QTimer::singleShot(500, sbY, &QSpinBox::editingFinished);
    });
    connect(sbY, &QSpinBox::editingFinished, [=, this]() {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.y = sbY->value();
            m_onlineModified = true;
            checkAndUpdateRoi();
        }
        sbWidth->setValue(m_roi.width + m_roi.x);
        sbHeight->setValue(m_roi.height + m_roi.y);
        sbX->setValue(m_roi.x);
    });

    auto formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Start X:"), sbX);
    formLayout->addRow(QStringLiteral("Width:"), sbWidth);
    formLayout->addRow(QStringLiteral("Start Y:"), sbY);
    formLayout->addRow(QStringLiteral("Height:"), sbHeight);
    formLayout->addWidget(m_sizeInfoLabel);
    parent->setLayout(formLayout);
}

bool CropTransform::allowOnlineModify() const
{
    return true;
}

QSize CropTransform::resultSize()
{
    if (m_activeRoi.empty())
        return m_originalSize;

    checkAndUpdateRoi();
    return {m_activeRoi.width, m_activeRoi.height};
}

void CropTransform::start()
{
    if (m_roi.empty()) {
        m_roi.width = m_originalSize.width();
        m_roi.height = m_originalSize.height();
    }

    checkAndUpdateRoi();
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

    if ((m_roi.width + m_roi.x < m_activeOutSize.width) && (m_roi.height + m_roi.y < m_activeOutSize.height)) {
        // the crop dimensions are smaller than our output, so we can simply cut things
        cropScaleMat.copyTo(outMat(m_roi));
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
    // sanity checks
    if (m_roi.x + m_roi.width > m_originalSize.width() || m_roi.width < 1)
        m_roi.width = m_originalSize.width() - m_roi.x;
    if (m_roi.y + m_roi.height > m_originalSize.height() || m_roi.height < 1)
        m_roi.height = m_originalSize.height() - m_roi.y;
    if (m_roi.width < 1)
        m_roi.width = 1;
    if (m_roi.height < 1)
        m_roi.height = 1;

    // give user some info as to what we are actually doing
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
    };
}

ScaleTransform::ScaleTransform()
    : VideoTransform()
{
    m_scaleFactor = 1;
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
    auto sbSymScale = new QDoubleSpinBox(parent);
    sbSymScale->setRange(0.01, 10);
    sbSymScale->setValue(m_scaleFactor);
    connect(sbSymScale, qOverload<double>(&QDoubleSpinBox::valueChanged), [=, this](double value) {
        m_scaleFactor = value;
    });

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Scale Factor:"), sbSymScale);
    parent->setLayout(formLayout);
}

QSize ScaleTransform::resultSize()
{
    return QSize(round(m_originalSize.width() * m_scaleFactor), round(m_originalSize.height() * m_scaleFactor));
}

void ScaleTransform::process(cv::Mat &image)
{
    cv::Mat outMat(image);
    cv::resize(outMat, outMat, cv::Size(), m_scaleFactor, m_scaleFactor);
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
    QFormLayout *formLayout = new QFormLayout(parent);
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
    QFormLayout *formLayout = new QFormLayout(parent);
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
