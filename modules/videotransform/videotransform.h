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

#pragma once

#include "datactl/frametype.h"
#include <QObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;

/**
 * @brief Interface for all transformation classes
 */
class VideoTransform : public QObject
{
public:
    explicit VideoTransform();

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QIcon icon() const
    {
        return QIcon::fromTheme("view-filter");
    };
    virtual void createSettingsUi(QWidget *parent) = 0;

    void setOriginalSize(const QSize &size);
    virtual QSize resultSize();

    [[nodiscard]] virtual bool allowOnlineModify() const;

    virtual void setUiDisplayed(bool visible);

    virtual void start();
    virtual void process(cv::Mat &image) = 0;
    virtual void stop();

    /**
     * @brief Indicates if this transform needs the caller to provide an independent copy
     * @return true if the transform needs an independent copy of the image data,
     *         false if the transform will create its own copy during processing
     *
     * Note: If this returns false, the transform MUST ensure that it creates
     * an independent copy of the image data during process() to avoid modifying
     * the original data that may be shared with other modules/threads.
     */
    [[nodiscard]] virtual bool needsIndependentCopy() const;

    virtual QVariantHash toVariantHash();
    virtual void fromVariantHash(const QVariantHash &settings);

protected:
    QSize m_originalSize;
};

/**
 * @brief Crop frames to match a certain size
 */
class CropTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit CropTransform();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QIcon icon() const override;
    void createSettingsUi(QWidget *parent) override;

    [[nodiscard]] bool allowOnlineModify() const override;
    QSize resultSize() override;

    void start() override;
    void process(cv::Mat &image) override;

    QVariantHash toVariantHash() override;
    void fromVariantHash(const QVariantHash &settings) override;

    bool needsIndependentCopy() const override;
    void setUiDisplayed(bool visible) override;

private:
    void checkAndUpdateRoi();

    QLabel *m_sizeInfoLabel{nullptr};
    QPushButton *m_btnSelectRegion{nullptr};
    QSpinBox *m_sbWidth{nullptr};
    QSpinBox *m_sbHeight{nullptr};
    QSpinBox *m_sbX{nullptr};
    QSpinBox *m_sbY{nullptr};

    std::mutex m_mutex;

    QSize m_maxima;
    cv::Size m_activeOutSize{0, 0};
    cv::Rect m_roi{0, 0, 0, 0};
    cv::Rect m_activeRoi{0, 0, 0, 0};
    std::atomic_bool m_onlineModified{false};

    cv::Mat m_cachedFrame;
    std::atomic_bool m_hasCachedFrame{false};
    std::atomic_bool m_settingsVisible{false};
    int m_frameCacheCounter{0};
};

/**
 * @brief Scale frames by a factor
 */
class ScaleTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit ScaleTransform();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QIcon icon() const override;
    void createSettingsUi(QWidget *parent) override;

    QSize resultSize() override;
    void process(cv::Mat &image) override;

    QVariantHash toVariantHash() override;
    void fromVariantHash(const QVariantHash &settings) override;

    [[nodiscard]] bool needsIndependentCopy() const override;

private:
    double m_scaleFactor{1.0};
};

/**
 * @brief Apply a false-color transformation to the video
 */
class FalseColorTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit FalseColorTransform();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QIcon icon() const override;

    void createSettingsUi(QWidget *parent) override;

    void process(cv::Mat &image) override;
};

/**
 * @brief Apply a histogram normalization transformation to the video
 */
class HistNormTransform : public VideoTransform
{
    Q_OBJECT
public:
    explicit HistNormTransform();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QIcon icon() const override;

    void createSettingsUi(QWidget *parent) override;

    void process(cv::Mat &image) override;
};
