/*
 * Copyright (C) 2020-2022 Matthias Klumpp <matthias@tenstral.net>
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
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <opencv2/imgproc.hpp>

VideoTransform::VideoTransform()
    : QObject()
{
    m_originalSize.width = 99999;
    m_originalSize.height = 99999;
}

void VideoTransform::setOriginalSize(const cv::Size &size)
{
    m_originalSize = size;
}

cv::Size VideoTransform::resultSize() const
{
    return m_originalSize;
}

bool VideoTransform::allowOnlineModify() const
{
    return false;
}

void VideoTransform::start()
{}

void VideoTransform::stop()
{}

QVariantHash VideoTransform::toVariantHash()
{
    return QVariantHash();
}

void VideoTransform::fromVariantHash(const QVariantHash &)
{}

CropTransform::CropTransform()
    : VideoTransform()
{}

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
    auto sbWidth = new QSpinBox(parent);
    sbWidth->setRange(0, m_originalSize.width);
    sbWidth->setSuffix("px");
    sbWidth->setValue(m_roi.width);
    sbWidth->setMinimumWidth(100);

    auto sbX = new QSpinBox(parent);
    sbX->setRange(0, m_originalSize.width - 1);
    sbX->setSuffix("px");
    sbX->setValue(m_roi.x);
    sbX->setMinimumWidth(100);

    auto sbHeight= new QSpinBox(parent);
    sbHeight->setRange(0, m_originalSize.height);
    sbHeight->setSuffix("px");
    sbHeight->setValue(m_roi.height);
    sbHeight->setMinimumWidth(100);

    auto sbY = new QSpinBox(parent);
    sbY->setRange(0, m_originalSize.height - 1);
    sbY->setSuffix("px");
    sbY->setValue(m_roi.y);
    sbY->setMinimumWidth(100);

    connect(sbWidth, qOverload<int>(&QSpinBox::valueChanged), [=](int value) {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.width = value;
            m_onlineModified = true;
        }
        sbX->setMaximum(m_originalSize.width - value);
        if (sbX->value() == sbX->maximum())
            sbX->setValue(sbX->maximum() - 1);
    });


    connect(sbHeight, qOverload<int>(&QSpinBox::valueChanged), [=](int value) {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.height = value;
            m_onlineModified = true;
        }
        sbY->setMaximum(m_originalSize.height - value);
        if (sbY->value() == sbY->maximum())
            sbY->setValue(sbY->maximum() - 1);
    });


    connect(sbX, qOverload<int>(&QSpinBox::valueChanged), [=](int value) {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.x = value;
            m_onlineModified = true;
        }
        sbWidth->setMaximum(m_originalSize.width - value);
        if (sbWidth->value() == sbWidth->maximum())
            sbWidth->setValue(sbWidth->maximum() - 1);
    });

    connect(sbY, qOverload<int>(&QSpinBox::valueChanged), [=](int value) {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_roi.y = value;
            m_onlineModified = true;
        }
        sbHeight->setMaximum(m_originalSize.height - value);
        if (sbHeight->value() == sbHeight->maximum())
            sbHeight->setValue(sbHeight->maximum() - 1);
    });

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Start X:"), sbX);
    formLayout->addRow(QStringLiteral("Start Y:"), sbY);
    formLayout->addRow(QStringLiteral("Width:"), sbWidth);
    formLayout->addRow(QStringLiteral("Height:"), sbHeight);
    parent->setLayout(formLayout);
}

bool CropTransform::allowOnlineModify() const
{
    return true;
}

cv::Size CropTransform::resultSize() const
{
    if (m_activeRoi.empty())
        return m_originalSize;

    cv::Size size;
    size.width = m_activeRoi.width - m_activeRoi.x;
    size.height = m_activeRoi.height - m_activeRoi.y;
    return size;
}

void CropTransform::start()
{
    if (m_roi.empty()) {
        m_roi.width = m_originalSize.width;
        m_roi.height = m_originalSize.height;
    }

    if (((m_roi.width - m_roi.x) > m_originalSize.width) || ((m_roi.width - m_roi.x) <= 0)) {
        m_roi.x = 0;
        m_roi.width = m_originalSize.width;
    }
    if (((m_roi.height - m_roi.y) > m_originalSize.height) || ((m_roi.height - m_roi.y) <= 0)) {
        m_roi.y = 0;
        m_roi.height = m_originalSize.height;
    }

    m_activeRoi = m_roi;
    m_activeOutSize = resultSize();
    m_onlineModified = false;
}

void CropTransform::process(Frame &frame)
{
    // handle the simple case: no online modifications
    if (!m_onlineModified) {
        // FIXME: frame.mat = frame.mat(m_activeRoi) should work in theory, but OpenCV simply doesn't apply the X and Y limits.
        // A workaround was using ranges
        frame.mat = frame.mat(cv::Range(m_activeRoi.y, m_activeRoi.height), cv::Range(m_activeRoi.x, m_activeRoi.width));
        return;
    }

    // online modification: since we are not allowed to alter the image dimensions, we add black bars for now or scale accordingly
    const std::lock_guard<std::mutex> lock(m_mutex);

    // sanity check
    if (((m_roi.height - m_roi.y) <= 0) || ((m_roi.width - m_roi.x) <= 0)) {
        frame.mat = frame.mat(cv::Range(m_activeRoi.y, m_activeRoi.height), cv::Range(m_activeRoi.x, m_activeRoi.width));
        return;
    }

    // actually to the "fake resizing"
    cv::Mat cropScaleMat = frame.mat(cv::Range(m_roi.y, m_roi.height), cv::Range(m_roi.x, m_roi.width));
    cv::Mat outMat = cv::Mat::zeros(m_activeOutSize, frame.mat.type());

    double scaleFactor = 1;
    if (cropScaleMat.cols > outMat.cols)
        scaleFactor = (double) outMat.cols / (double) cropScaleMat.cols;
    if (cropScaleMat.rows > outMat.rows) {
        double scale = (double) outMat.rows / (double) cropScaleMat.rows;
        scaleFactor = (scale < scaleFactor)? scale : scaleFactor;
    }
    if (scaleFactor == 0) {
        // weird... scale factor should never be zero
        frame.mat = outMat;
        return;
    }

    cv::resize(cropScaleMat, cropScaleMat, cv::Size(), scaleFactor, scaleFactor);
    cropScaleMat.copyTo(outMat(cv::Rect((outMat.cols - cropScaleMat.cols) / 2, (outMat.rows - cropScaleMat.rows) / 2, cropScaleMat.cols, cropScaleMat.rows)));
    frame.mat = outMat;
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
    connect(sbSymScale, qOverload<double>(&QDoubleSpinBox::valueChanged), [=](double value) {
        m_scaleFactor = value;
    });

    QFormLayout *formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("Scale Factor:"), sbSymScale);
    parent->setLayout(formLayout);
}

cv::Size ScaleTransform::resultSize() const
{
    return cv::Size(round(m_originalSize.width * m_scaleFactor), round(m_originalSize.height * m_scaleFactor));
}

void ScaleTransform::process(Frame &frame)
{
    cv::Mat outMat(frame.mat);
    cv::resize(outMat, outMat, cv::Size(), m_scaleFactor, m_scaleFactor);
    frame.mat = outMat;
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
