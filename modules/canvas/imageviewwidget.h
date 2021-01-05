/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include <QOpenGLFunctions_3_0>
#include <opencv2/core/core.hpp>

class ImageViewWidget: public QOpenGLWidget, protected QOpenGLFunctions_3_0
{
    Q_OBJECT
public:
    explicit ImageViewWidget(QWidget *parent = nullptr);
    ~ImageViewWidget();

public slots:
    bool showImage(const cv::Mat& image);

    void setMinimumSize(const QSize& size);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

    void renderImage();

private:
    class Private;
    Q_DISABLE_COPY(ImageViewWidget)
    QScopedPointer<Private> d;

    void recalculatePosition();
    GLuint matToTexture(cv::Mat &mat, GLenum minFilter, GLenum magFilter, GLenum wrapFilter);
};
