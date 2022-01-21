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

#pragma once

#include <QObject>
#include <QWidget>
#include <QIcon>
#include "streams/frametype.h"

/**
 * @brief Interface for all transformation classes
 */
class VideoTransform : public QObject
{
public:
    explicit VideoTransform();

    virtual QString name() const = 0;
    virtual QIcon icon() const { return QIcon::fromTheme("view-filter"); };
    virtual void createSettingsUi(QWidget *parent) = 0;

    void setOriginalSize(const cv::Size &size);
    virtual cv::Size resultSize() const;

    virtual bool allowOnlineModify() const;

    virtual void start();
    virtual void process(Frame &frame) = 0;
    virtual void stop();

    virtual QVariantHash toVariantHash();
    virtual void fromVariantHash(const QVariantHash &settings);

protected:
    cv::Size m_originalSize;
};

/**
 * @brief Crop frames to match a certain size
 */
class CropTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit CropTransform();

    QString name() const override;
    QIcon icon() const override;
    void createSettingsUi(QWidget *parent) override;

    bool allowOnlineModify() const override;
    cv::Size resultSize() const override;

    void start() override;
    void process(Frame &frame) override;

    QVariantHash toVariantHash() override;
    void fromVariantHash(const QVariantHash &settings) override;

private:
    std::mutex m_mutex;

    cv::Size m_maxima;
    cv::Rect m_roi;
    cv::Rect m_activeRoi;
    cv::Size m_activeOutSize;
    std::atomic_bool m_onlineModified;
};

/**
 * @brief Crop frames to match a certain size
 */
class ScaleTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit ScaleTransform();

    QString name() const override;
    QIcon icon() const override;
    void createSettingsUi(QWidget *parent) override;

    cv::Size resultSize() const override;
    void process(Frame &frame) override;

    QVariantHash toVariantHash() override;
    void fromVariantHash(const QVariantHash &settings) override;

private:
    double m_scaleFactor;
};
