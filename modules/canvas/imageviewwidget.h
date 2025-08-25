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

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <opencv2/core.hpp>

class ImageViewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit ImageViewWidget(QWidget *parent = nullptr);
    ~ImageViewWidget();

public slots:
    bool showImage(const cv::Mat &mat);
    cv::Mat currentRawImage() const;

    void setMinimumSize(const QSize &size);
    void setHighlightSaturation(bool enabled);
    bool highlightSaturation() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void renderImage();

private:
    class Private;
    Q_DISABLE_COPY(ImageViewWidget)
    std::unique_ptr<Private> d;
};
