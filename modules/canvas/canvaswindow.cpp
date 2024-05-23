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

#include "canvaswindow.h"
#include "imageviewwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QCheckBox>
#include <QPushButton>
#include <QGraphicsEffect>

class InvertOpaqueEffect : public QGraphicsEffect
{
    Q_OBJECT
public:
    explicit InvertOpaqueEffect(double opacity, QObject *parent = nullptr)
        : QGraphicsEffect(parent),
          m_opacity(opacity)
    {
    }

    void draw(QPainter *painter) override
    {
        QPoint offset;
        QPixmap pixmap = sourcePixmap(Qt::DeviceCoordinates, &offset, QGraphicsEffect::NoPad);

        // apply opacity by modifying pixmap
        QPixmap semiTransparentPixmap(pixmap.size());
        semiTransparentPixmap.fill(Qt::transparent);
        QPainter p(&semiTransparentPixmap);
        p.setOpacity(m_opacity);
        p.drawPixmap(0, 0, pixmap);
        p.end();

        // apply color inversion by modifying image
        QImage image = semiTransparentPixmap.toImage();
        image.invertPixels(QImage::InvertRgb);

        // draw the final image
        painter->drawImage(offset, image);
    }

private:
    double m_opacity;
};

class ToolsOverlayWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToolsOverlayWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);

        auto olEffect = new InvertOpaqueEffect(0.8, this);
        setGraphicsEffect(olEffect);

        auto mainLayout = new QHBoxLayout(this);
        mainLayout->setMargin(2);
        setLayout(mainLayout);

        // add toggle button
        m_toggleBtn = new QPushButton(this);
        m_toggleBtn->setFlat(true);
        m_toggleBtn->setIcon(QIcon::fromTheme("arrow-right"));
        m_toggleBtn->setCheckable(true);
        mainLayout->addWidget(m_toggleBtn, 0, Qt::AlignLeft);
        connect(m_toggleBtn, &QPushButton::clicked, this, &ToolsOverlayWidget::toggleVisibility);

        // configure controls container
        m_controls = new QWidget(this);
        auto layout = new QHBoxLayout(m_controls);
        layout->setMargin(0);
        layout->setSpacing(4);

        m_cbHlSaturation = new QCheckBox("Highlight saturation", m_controls);
        layout->addWidget(m_cbHlSaturation);
        connect(m_cbHlSaturation, &QCheckBox::toggled, this, [this](bool enabled) {
            Q_EMIT highlightSaturationChanged(enabled);
        });

        m_cbShowHistogram = new QCheckBox("Show histogram", m_controls);
        layout->addWidget(m_cbShowHistogram);
        connect(m_cbShowHistogram, &QCheckBox::toggled, this, [this](bool enabled) {
            Q_EMIT showHistogramChanged(enabled);
        });

        mainLayout->addWidget(m_controls);

        // initially hide the controls
        m_controls->setVisible(false);
        adjustSize();
    }

    bool highlightSaturation() const
    {
        return m_cbHlSaturation->isChecked();
    }

    void setHighlightSaturation(bool enabled)
    {
        m_cbHlSaturation->setChecked(enabled);
    }

    bool showHistogram() const
    {
        return m_cbShowHistogram->isChecked();
    }

    void setShowHistogram(bool enabled)
    {
        m_cbShowHistogram->setChecked(enabled);
    }

public slots:
    void toggleVisibility()
    {
        if (m_controls->isVisible()) {
            m_toggleBtn->setIcon(QIcon::fromTheme("arrow-right"));
            m_controls->setVisible(false);
        } else {
            m_toggleBtn->setIcon(QIcon::fromTheme("arrow-left"));
            m_controls->setVisible(true);
        }

        adjustSize();
    }

signals:
    void highlightSaturationChanged(bool enabled);
    void showHistogramChanged(bool enabled);

private:
    QPushButton *m_toggleBtn;
    QWidget *m_controls;

    QCheckBox *m_cbHlSaturation;
    QCheckBox *m_cbShowHistogram;
};

CanvasWindow::CanvasWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Canvas");

    m_imgView = new ImageViewWidget(this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setText(QStringLiteral("Empty"));
    QFont font;
    font.setStyleHint(QFont::Monospace, QFont::PreferMatch);
    font.setFamily("Hack, Fira Code, Noto Mono, Monospace");
    m_statusLabel->setFont(font);

    setMinimumSize(m_imgView->minimumSize());

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
    pal.setColor(QPalette::Window, QColor::fromRgb(150, 150, 150).darker());
    setPalette(pal);

    pal = m_statusLabel->palette();
    pal.setColor(QPalette::WindowText, Qt::white);
    m_statusLabel->setPalette(pal);

    // construct tools overlay
    m_toolsOverlay = new ToolsOverlayWidget(this);
    m_toolsOverlay->hide();
    setMouseTracking(true);

    connect(m_toolsOverlay, &ToolsOverlayWidget::highlightSaturationChanged, this, [this](bool enabled) {
        m_imgView->setHighlightSaturation(enabled);
    });
}

void CanvasWindow::showImage(const vips::VImage &image)
{
    if (isVisible())
        m_imgView->showImage(image);
}

void CanvasWindow::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

bool CanvasWindow::highlightSaturation() const
{
    return m_imgView->highlightSaturation();
}

void CanvasWindow::setHighlightSaturation(bool enabled)
{
    m_toolsOverlay->setHighlightSaturation(enabled);
}

void CanvasWindow::enterEvent(QEvent *event)
{
    QWidget::enterEvent(event);
    m_toolsOverlay->show();
}

void CanvasWindow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_toolsOverlay->hide();
}

#include "canvaswindow.moc"
