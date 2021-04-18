//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef PAGEVIEW_H
#define PAGEVIEW_H

#define ZOOM_CONSTANT 0.7
#define BESTFIT_BUFFER 1.2
#define SCROLL_CONSTANT 0.05

#include <QtWidgets>

#include "impedancegradient.h"
#include "spikegradient.h"
#include "systemstate.h"

enum SelectType {
    SelectAdd,
    SelectReplace,
    None
};

enum MouseMode {
    Select,
    Scroll,
    Resize
};

enum ViewMode {
    DefaultView,
    ImpedanceView,
    SpikeView
};


class PageView : public QWidget
{
    Q_OBJECT
public:
    explicit PageView(SystemState* state_, int pageIndex, ViewMode viewMode_, QWidget *parent = nullptr);
    void changeView(ViewMode viewMode_);

signals:
    void mouseMoved(float x, float y, bool mousePresent, QString hoveredSiteName);
    void mouseExit();
    void mouseEnter();
    void resizeComplete();
    void siteHighlighted(QString nativeName, bool highlight);
    void deselectAllSites();

public slots:
    void bestFit();
    void scrollUp();
    void scrollDown();
    void scrollLeft();
    void scrollRight();
    void zoomIn();
    void zoomOut();
    void changeMode(MouseMode mode);

protected:
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // Display size/scaling
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    int xUnitsToPixelValue(float xUnit);
    int yUnitsToPixelValue(float yUnit);
    float xPixelsToUnitValue(int xPixel);
    float yPixelsToUnitValue(int yPixel);
    int xSizeToPixelSize(float xSize);
    int ySizeToPixelSize(float ySize);
    void getDesiredSize(float &newXMin, float &newXMax, float &newYMin, float &newYMax);
    void zoomToDesiredSize(float newXMin, float newXMax, float newYMin, float newYMax);
    void scrollOffset(float xOffset, float yOffset);
    QString siteAtCoordinates(float xCoord, float yCoord);
    bool siteIncludesCoordinates(int port, int site, float xCoord, float yCoord);
    void initializeDisplay();

    // Drawing
    void drawBackground();
    void drawLines();
    void drawTexts();
    void calculateCorners(const QString alignment, const float height, const float width, const float rotation,
                          float &aX0, float &aY0, float &bX0, float &bY0, float &cX0, float &cY0, float &dX0, float &dY0);
    void drawSites();
    void drawX(QPainter &painter, QPointF siteOrigin, double a, double b, bool ellipse);
    void drawRect();

    // Consulting XML for correct parameters
    QColor getBackgroundColor();
    QColor getLineColor(int line);
    QColor getSiteOutlineColor(int port, int site);
    QColor getFontColor(int text);
    float getFontHeight(int text);
    QString getTextAlignment(int text);
    float getSiteWidth(int port, int site);
    float getSiteHeight(int port, int site);
    QString getSiteShape(int port, int site);

    // Overwriting parameters
    void overwriteColor(QColor &color, const QString& colorName);
    void overwriteFloat(float &original, float newValue);
    void overwriteShape(QString &original, const QString& newString);
    void overwriteAlignment(QString &original, const QString& newString);

    // User interacting with PageView
    QVector<ElectrodeSite*> getSitesWithName(const QString& nativeName);
    QVector<ElectrodeSite*> getSitesWithinSelectRect();
    void updateMousePosition();

    SystemState* state;
    ViewMode viewMode;
    Page &page;
    float unitsToPixel;
    bool mousePresent;
    QString hoveredSiteName;
    int mouseX;
    int mouseY;
    MouseMode mouseMode;

    bool scrolling;
    float scrollStartX;
    float scrollStartY;

    bool resizing;
    int resizingStartX;
    int resizingStartY;
    int resizingEndX;
    int resizingEndY;
    QRect resizeRect;

    SelectType selecting;
    int selectingStartX;
    int selectingStartY;
    int selectingEndX;
    int selectingEndY;
    QRect selectRect;

    QString backgroundColor;
    QString lineColor;
    QString siteOutlineColor;
    float fontHeight;
    QString fontColor;
    QString textAlignment;
    QString siteShape;
    float siteWidth;
    float siteHeight;

    QPixmap pixmap;

    float bestfitXmin;
    float bestfitYmin;
    float bestfitWidth;
    float bestfitHeight;

    float visibleFrameXmin;
    float visibleFrameYmin;
    float visibleFrameWidth;
    float visibleFrameHeight;
    float visibleFrameCenterX;
    float visibleFrameCenterY;

    float currentZoomWidth;
    float currentZoomHeight;

    static const int PageViewXSize = 200;
    static const int PageViewYSize = 200;
};

#endif // PAGEVIEW_H
