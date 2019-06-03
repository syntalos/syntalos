/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VIDEOVIEWWIDGET_H
#define VIDEOVIEWWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <opencv2/core/core.hpp>

class VideoViewWidget: public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit VideoViewWidget(QWidget *parent = nullptr);

public slots:
    bool showImage(const cv::Mat& image);

    void setMinimumSize(const QSize& size);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

    void updateScene();
    void renderImage();

private:
    void recalculatePosition();

    QColor m_bgColor;
    cv::Mat m_origImage;

    int m_renderWidth;
    int m_renderHeight;
    int m_renderPosX;
    int m_renderPosY;
};

#endif // VIDEOVIEWWIDGET_H
