/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "canvaswindow.h"
#include "imageviewwidget.h"

#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

CanvasWindow::CanvasWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Canvas");
    setMinimumSize(QSize(320, 256));
    setWindowIcon(QIcon(":/module/canvas"));

    m_imgView = new ImageViewWidget(this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(QStringLiteral("Empty"));

    auto container = new QWidget(this);
    auto clayout = new QHBoxLayout;
    clayout->setMargin(0);
    clayout->setSpacing(0);
    clayout->addWidget(m_statusLabel);
    clayout->addStretch();
    container->setLayout(clayout);

    auto layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(m_imgView, 1);
    layout->addWidget(container);
    setLayout(layout);

    setContentsMargins(0, 0, 0, 0);

    auto pal = palette();
    pal.setColor(QPalette::Background, QColor::fromRgb(150, 150, 150).darker());
    setPalette(pal);

    pal = m_statusLabel->palette();
    pal.setColor(QPalette::Foreground, Qt::white);
    m_statusLabel->setPalette(pal);
}

void CanvasWindow::showImage(const cv::Mat &image)
{
    m_imgView->showImage(image);
}

void CanvasWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}
