/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <QMouseEvent>
#include <QIcon>

struct Histograms {
    float red[256], green[256], blue[256];
};

class HistogramWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget *parent = nullptr);

    void paintGL() override;
    void setIdle();
    Histograms *unusedHistograms();
    void swapHistograms(bool grayscale);

public slots:
    void setLogarithmic(bool logarithmic);

private:
    QIcon idleImageIcon;
    bool indexed, logarithmic, idle;
    Histograms histograms1, histograms2;
    Histograms *unusedHists;
    float *histRed, *histGreen, *histBlue;
    QBrush backgroundBrush, foregroundBrush;
};
