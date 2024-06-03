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

#pragma once

#include <QWidget>
#include "datactl/vips8-q.h"

class ImageViewWidget;
class ToolsOverlayWidget;
class HistogramWidget;
class QLabel;
class QTimer;

class CanvasWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CanvasWindow(QWidget *parent = nullptr);

    void showImage(const vips::VImage &image);
    void setStatusText(const QString &text);

    bool highlightSaturation() const;
    void setHighlightSaturation(bool enabled);

    bool histogramVisible() const;
    void setHistogramVisible(bool show);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void updateHistogram();

private:
    ImageViewWidget *m_imgView;
    QLabel *m_statusLabel;
    ToolsOverlayWidget *m_toolsOverlay;

    QTimer *m_histTimer;
    HistogramWidget *m_histogramWidget;
};
