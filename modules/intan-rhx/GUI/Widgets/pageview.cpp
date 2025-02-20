//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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

#include "pageview.h"

PageView::PageView(SystemState* state_, int pageIndex, ViewMode viewMode_, QWidget *parent) :
    QWidget(parent),
    state(state_),
    viewMode(viewMode_),
    page(state_->probeMapSettings.pages[pageIndex]),
    backgroundColor(state_->probeMapSettings.backgroundColor),
    lineColor(state_->probeMapSettings.lineColor),
    siteOutlineColor(state_->probeMapSettings.siteOutlineColor),
    fontHeight(state_->probeMapSettings.fontHeight),
    fontColor(state_->probeMapSettings.fontColor),
    textAlignment(state_->probeMapSettings.textAlignment),
    siteShape(state_->probeMapSettings.siteShape),
    siteWidth(state_->probeMapSettings.siteWidth),
    siteHeight(state_->probeMapSettings.siteHeight)
{
    // Initialize state variables and coordinates.
    mousePresent = false;
    mouseX = 0;
    mouseY = 0;
    mouseMode = Select;

    scrolling = false;
    scrollStartX = 0;
    scrollStartY = 0;

    resizing = false;
    resizingStartX = 0;
    resizingStartY = 0;
    resizeRect = QRect();

    selecting = None;
    selectingStartX = 0;
    selectingStartY = 0;
    selectRect = QRect();

    // Initialize spike time to decay time (so that each site's initial state is fully decayed, i.e. not spiking)
    for (int i = 0; i < page.ports.size(); ++i) {
        for (int j = 0; j < page.ports[i].electrodeSites.size(); ++j) {
            page.ports[i].electrodeSites[j].spikeTime = state->getDecayTime();
        }
    }

    setBackgroundRole(QPalette::Window);
    setAutoFillBackground(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);

    qreal dpr = devicePixelRatioF();
    pixmap = QPixmap(width() * dpr, height() * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    // If there are no ports present, warn user and exit.
    if (page.ports.size() < 1) {
        QMessageBox::critical(this, "Error", "No ports are present on this page. At least one port with an electrode site must be present to be displayed");
        return;
    }

    // If there are no electrode sites present, warn user and exit
    int numSites = 0;
    for (int port = 0; port < page.ports.size(); ++port) {
        numSites += page.ports[port].electrodeSites.size();
    }
    if (numSites < 1) {
        QMessageBox::critical(this, "Error", "No electrode sites are present on this page. At least one port with an electrode site must be present to be displayed");
        return;
    }

    // Populate each electrode site's boundary rectangle attributes.
    for (int port = 0; port < page.ports.size(); ++port) {
        for (int site = 0; site < page.ports[port].electrodeSites.size(); ++site) {
            ElectrodeSite* thisSite = &page.ports[port].electrodeSites[site];
            thisSite->boundXmin = thisSite->x - getSiteWidth(port, site)/2;
            thisSite->boundXmax = thisSite->x + getSiteWidth(port, site)/2;
            thisSite->boundYmin = thisSite->y - getSiteHeight(port, site)/2;
            thisSite->boundYmax = thisSite->y + getSiteHeight(port, site)/2;
        }
    }

    float boundXmin = 0.0f, boundXmax = 0.0f, boundYmin = 0.0f, boundYmax = 0.0f;

    // Initialize bounds around the first electrode site.
    for (int port = 0; port < page.ports.size(); ++port) {
        if (page.ports[port].electrodeSites.size() > 0) {
            boundXmin = page.ports[port].electrodeSites[0].boundXmin;
            boundXmax = page.ports[port].electrodeSites[0].boundXmax;
            boundYmin = page.ports[port].electrodeSites[0].boundYmin;
            boundYmax = page.ports[port].electrodeSites[0].boundYmax;
        }
    }

    // Cycle through all electrode sites, getting bounds.
    for (int port = 0; port < page.ports.size(); ++port) {
        for (int electrodeSite = 0; electrodeSite < page.ports[port].electrodeSites.size(); ++electrodeSite) {
            boundXmin = std::min(page.ports[port].electrodeSites[electrodeSite].boundXmin, boundXmin);
            boundXmax = std::max(page.ports[port].electrodeSites[electrodeSite].boundXmax, boundXmax);
            boundYmin = std::min(page.ports[port].electrodeSites[electrodeSite].boundYmin, boundYmin);
            boundYmax = std::max(page.ports[port].electrodeSites[electrodeSite].boundYmax, boundYmax);
        }
    }

    // Cycle through all lines, getting bounds.
    for (int line = 0; line < page.lines.size(); ++line) {
        boundXmin = std::min({page.lines[line].x1, page.lines[line].x2, boundXmin});
        boundXmax = std::max({page.lines[line].x1, page.lines[line].x2, boundXmax});
        boundYmin = std::min({page.lines[line].y1, page.lines[line].y2, boundYmin});
        boundYmax = std::max({page.lines[line].y1, page.lines[line].y2, boundYmax});
    }

    // Cycle through all texts, getting bounds.
    for (int text = 0; text < page.texts.size(); ++text) {

        // Create dummy QFont (that won't get used) just to get the ratio of width-to-height for this machine's 'TypeWriter' style font.
        QFont thisText("Courier New");
        int dummyHeight = 100; // Arbitrary height for dummy text
        thisText.setPixelSize(dummyHeight);
        QFontMetrics metrics(thisText);

        float dummyWidth = metrics.horizontalAdvance('A'); // Arbitrary character. Assuming this is a monospace font, any one character should have the same width.
        float widthToHeightRatio = dummyWidth / dummyHeight;

        /* A ------------ B
         * |              |
         * |   text ....  |
         * |              |
         * D ------------ C */

        // Calculate the coordinates of four points defining the (possibly rotated) rectangle containing the text -
        // their coordinates where origin defined by alignment is (0, 0)
        float aX0 = 0.0f, aY0 = 0.0f, bX0 = 0.0f, bY0 = 0.0f, cX0 = 0.0f, cY0 = 0.0f, dX0 = 0.0f, dY0 = 0.0f;
        QString alignment = getTextAlignment(text);
        float textHeight = getFontHeight(text);
        float textWidth = widthToHeightRatio * textHeight * page.texts[text].text.length();
        float rotation = page.texts[text].rotation * DegreesToRadiansF;
        calculateCorners(alignment, textHeight, textWidth, rotation, aX0, aY0, bX0, bY0, cX0, cY0, dX0, dY0);

        // Calculate the actual coordinates defining the (possibly rotated) rectangle containing the text.
        float aX = page.texts[text].x + aX0;
        float aY = page.texts[text].y + aY0;
        float bX = page.texts[text].x + bX0;
        float bY = page.texts[text].y + bY0;
        float cX = page.texts[text].x + cX0;
        float cY = page.texts[text].y + cY0;
        float dX = page.texts[text].x + dX0;
        float dY = page.texts[text].y + dY0;

        // Store the coordinates of the bottom-left corner of the rectangle (from the perspective of the text) for later drawing of the text.
        page.texts[text].bottomLeftX = dX;
        page.texts[text].bottomLeftX = dY;

        page.texts[text].topLeftX = aX;
        page.texts[text].topLeftY = aY;

        page.texts[text].bottomRightX = cX;
        page.texts[text].bottomRightY = cY;

        // Determine minimum and maximum x and y values of this rectangle.
        boundXmin = std::min({aX, bX, cX, dX, boundXmin});
        boundXmax = std::max({aX, bX, cX, dX, boundXmax});
        boundYmin = std::min({aY, bY, cY, dY, boundYmin});
        boundYmax = std::max({aY, bY, cY, dY, boundYmax});
    }

    // Make the visible rectangle BESTFIT_BUFFER (120%) larger than the bounding rectangle.
    float boundingRectWidth = boundXmax - boundXmin;
    float boundingRectHeight = boundYmax - boundYmin;
    float visibleRectWidth = BESTFIT_BUFFER * boundingRectWidth;
    float visibleRectHeight = BESTFIT_BUFFER * boundingRectHeight;
    float diffX = visibleRectWidth - boundingRectWidth;
    float diffY = visibleRectHeight - boundingRectHeight;

    // Store minimum coordinates and dimensions of visible rectangle.
    bestfitXmin = boundXmin - diffX/2;
    float bestfitXmax = boundXmax + diffX/2;
    bestfitYmin = boundYmin - diffY/2;
    float bestfitYmax = boundYmax + diffY/2;

    bestfitWidth = (float) (bestfitXmax - bestfitXmin);
    bestfitHeight = (float) (bestfitYmax - bestfitYmin);

    // Initialize widget to be a reasonable size.
    resize(PageViewXSize, PageViewYSize);

    // Scale pixels to best-fit at the default size.
    //bestFit();

    // Allow for mouse.
    setMouseTracking(true);
}

// Change if this page should color sites based on impedance, spike activity, or default color.
void PageView::changeView(ViewMode viewMode_)
{
    viewMode = viewMode_;
}

// Catch signal from ProbeMapWindow signifying that the view should be best-fit.
// Resize visible rectangle to best-fit so that all sites, lines, and texts can be viewed.
void PageView::bestFit()
{
    // Determine what boundaries of widget are in USER UNITS upon resizing */
    float widgetAspectRatio = float(height()) / float(width());
    float bestfitAspectRatio = bestfitHeight/bestfitWidth;

    if (widgetAspectRatio < bestfitAspectRatio) {
        // If widget aspect ratio is lower than best fit aspect ratio, then lock UnitsToPixel ratio based on height.
        visibleFrameHeight = bestfitHeight;
        visibleFrameYmin = bestfitYmin;

        // Get UnitsToPixel ratio from y.
        unitsToPixel = bestfitHeight/height();

        // Use UnitsToPixel to determine x limits.
        visibleFrameWidth = unitsToPixel * width();
        visibleFrameXmin = (bestfitXmin + (bestfitWidth/2)) - visibleFrameWidth/2;
    } else {
        // If widget aspect ratio is higher than best fit aspect ratio, then lock UnitsToPixel ratio based on width.
        visibleFrameWidth = bestfitWidth;
        visibleFrameXmin = bestfitXmin;

        // Get UnitsToPixel ratio from x.
        unitsToPixel = bestfitWidth/width();

        // Use UnitsToPixel to determine y limits.
        visibleFrameHeight = unitsToPixel * height();
        visibleFrameYmin = (bestfitYmin + (bestfitHeight/2)) - visibleFrameHeight/2;
    }

    // Determine coordinates for center of visible frame.
    visibleFrameCenterX = visibleFrameXmin + visibleFrameWidth/2;
    visibleFrameCenterY = visibleFrameYmin + visibleFrameHeight/2;

    // Determine width and height of current zoom.
    currentZoomWidth = visibleFrameWidth;
    currentZoomHeight = visibleFrameHeight;

    update();

    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should scroll up. Scroll up by a factor of SCROLL_CONSTANT (5%).
void PageView::scrollUp()
{
    scrollOffset(0, SCROLL_CONSTANT * currentZoomHeight);
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should scroll down. Scroll down by a factor of SCROLL_CONSTANT (5%).
void PageView::scrollDown()
{
    scrollOffset(0, -1 * SCROLL_CONSTANT * currentZoomHeight);
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should scroll left. Scroll left by a factor of SCROLL_CONSTANT (5%).
void PageView::scrollLeft()
{
    scrollOffset(-1 * SCROLL_CONSTANT * currentZoomWidth, 0);
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should scroll right. Scroll right by a factor of SCROLL_CONSTANT (5%).
void PageView::scrollRight()
{
    scrollOffset(SCROLL_CONSTANT * currentZoomWidth, 0);
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should zoom in.
// If the view is not already zoom-in too much, zoom in by a factor of ZOOM_CONSTANT (70%).
void PageView::zoomIn()
{
    // Compare areas of visible rectangle and best-fit rectangle. If visible rectangle's area is too small, no more zooming in.
    float visibleArea = visibleFrameWidth * visibleFrameHeight;
    float bestfitArea = bestfitWidth * bestfitHeight;
    if (visibleArea < bestfitArea / 400.0) {
        return;
    }

    // If mouse cursor is within the map, offset the center of the next frame so that the cursor is positioned over the same coordinates.
    if (mousePresent) {
        float xRatio = (xPixelsToUnitValue(mouseX) - visibleFrameXmin) / currentZoomWidth;
        currentZoomWidth = currentZoomWidth * ZOOM_CONSTANT;
        float newXmin = xPixelsToUnitValue(mouseX) - xRatio * currentZoomWidth;
        visibleFrameCenterX = (newXmin + (newXmin + currentZoomWidth))/2;


        float yRatio = (yPixelsToUnitValue(mouseY) - visibleFrameYmin) / currentZoomHeight;
        currentZoomHeight = currentZoomHeight * ZOOM_CONSTANT;
        float newYmin = yPixelsToUnitValue(mouseY) - yRatio * currentZoomHeight;
        visibleFrameCenterY = (newYmin + (newYmin + currentZoomHeight))/2;
    } else {  // Otherwise, keep the same center.
        currentZoomHeight = currentZoomHeight * ZOOM_CONSTANT;
        currentZoomWidth = currentZoomWidth * ZOOM_CONSTANT;
    }
    unitsToPixel = unitsToPixel * ZOOM_CONSTANT;

    update();
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the view should zoom out.
// If the view is not already zoom-out too much, zoom out by a factor of ZOOM_CONSTANT (70%).
void PageView::zoomOut()
{
    // Compare areas of visible rectangle and best-fit rectangle.
    // If visible rectangle's area is too large, no more zooming out.
    float visibleArea = visibleFrameWidth * visibleFrameHeight;
    float bestfitArea = bestfitWidth * bestfitHeight;
    if (visibleArea > bestfitArea * 100.0) {
        return;
    }

    // If mouse cursor is within the map, offset the center of the next frame
    // so that the cursor is positioned over the same coordinates.
    if (mousePresent) {
        float xRatio = (xPixelsToUnitValue(mouseX) - visibleFrameXmin) / currentZoomWidth;
        currentZoomWidth = currentZoomWidth / ZOOM_CONSTANT;
        float newXmin = xPixelsToUnitValue(mouseX) - xRatio * currentZoomWidth;
        visibleFrameCenterX = (newXmin + (newXmin + currentZoomWidth))/2;

        float yRatio = (yPixelsToUnitValue(mouseY) - visibleFrameYmin) / currentZoomHeight;
        currentZoomHeight = currentZoomHeight / ZOOM_CONSTANT;
        float newYmin = yPixelsToUnitValue(mouseY) - yRatio * currentZoomHeight;
        visibleFrameCenterY = (newYmin + (newYmin + currentZoomHeight))/2;
    } else {  // Otherwise, keep the same center
        currentZoomHeight = currentZoomHeight / ZOOM_CONSTANT;
        currentZoomWidth = currentZoomWidth / ZOOM_CONSTANT;
    }
    unitsToPixel = unitsToPixel / ZOOM_CONSTANT;

    update();
    updateMousePosition();
}

// Catch signal from ProbeMapWindow signifying that the MouseMode should change.
// Switch the role that the mouse plays, between "Select", "Scroll", and "Resize".
void PageView::changeMode(MouseMode mode)
{
    mouseMode = mode;

    if (mouseMode == Select) {
        setCursor(Qt::ArrowCursor);
    } else if (mouseMode == Scroll) {
        setCursor(Qt::OpenHandCursor);
    } else if (mouseMode == Resize) {
        setCursor(Qt::CrossCursor);
    }
}

void PageView::paintEvent(QPaintEvent * /* event */)
{
    initializeDisplay();

    QPainter sPainter(this);
    sPainter.setRenderHint(QPainter::Antialiasing);
    sPainter.drawPixmap(0, 0, pixmap);
}

void PageView::closeEvent(QCloseEvent *event)
{
    // Perform any clean-up here before application closes.
    event->accept();
}

void PageView::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);

    // If mouse wheel is clicked (regardless of mouse mode), start scrolling.
    if (event->button() == Qt::MiddleButton) {
        scrolling = true;
        scrollStartX = xPixelsToUnitValue(event->pos().x());
        scrollStartY = yPixelsToUnitValue(event->pos().y());
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    // If mouse mode is 'Select', allow for individual CTRL+LeftClick selecting, individual LeftClick selecting,
    // group CTRL+LeftClick selecting, or group LeftClick selecting.
    if (mouseMode == Select) {

        // If CTRL+LeftClick occurs, start SelectAdding.
        if ((event->button() == Qt::LeftButton) && (event->modifiers() & Qt::ControlModifier)) {
            selecting = SelectAdd;
            selectingStartX = event->pos().x();
            selectingStartY = event->pos().y();
            selectingEndX = event->pos().x();
            selectingEndY = event->pos().y();
            return;
        } else if (event->button() == Qt::LeftButton) {  // If just LeftClick occurs, start SelectReplacing.
            selecting = SelectReplace;
            selectingStartX = event->pos().x();
            selectingStartY = event->pos().y();
            selectingEndX = event->pos().x();
            selectingEndY = event->pos().y();
            return;
        }
    } else if (mouseMode == Scroll) {
        // Set scrolling mode, get starting coordinates, and change the cursor to reflect that scrolling has begun
        scrolling = true;
        scrollStartX = xPixelsToUnitValue(event->pos().x());
        scrollStartY = yPixelsToUnitValue(event->pos().y());
        setCursor(Qt::ClosedHandCursor);
        return;
    } else if (mouseMode == Resize) {  // Set resizing mode and get starting coordinates.
        resizing = true;
        resizingStartX = event->pos().x();
        resizingStartY = event->pos().y();
        resizingEndX = event->pos().x();
        resizingEndY = event->pos().y();
        return;
    }
}

void PageView::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);

    // If mouse wheel was unclicked (regardless of mouse mode), end scrolling.
    if (event->button() == Qt::MiddleButton) {
        scrolling = false;
        if (mouseMode == Scroll) {
            setCursor(Qt::OpenHandCursor);
        } else if (mouseMode == Select) {
            setCursor(Qt::ArrowCursor);
        } else if (mouseMode == Resize) {
            setCursor(Qt::CrossCursor);
        }
    }


    // If mouse mode is 'Select', end selecting.
    if (mouseMode == Select) {

        selectingEndX = event->pos().x();
        selectingEndY = event->pos().y();

        // If the user hasn't actually created a rectangle, just SelectAdd or SelectReplace that one point.
        if (selectingEndX == selectingStartX || selectingEndY == selectingStartY) {

            // Check if the mouse is hovering over a site. If it is, toggle 'highlighted' on that site
            // (and maybe the other sites, depending on if this is SelectAdd or SelectReplace).
            QVector<ElectrodeSite*> selectedSites = getSitesWithName(hoveredSiteName);

            // For SelectReplace, begin by deselecting all linked and enabled sites.
            if (selecting == SelectReplace) {
                emit deselectAllSites();

                // Then, highlight the selected sites.
                for (int site = 0; site < selectedSites.size(); ++site) {
                    if (selectedSites[site]->linked && selectedSites[site]->enabled) {
                        selectedSites[site]->highlighted = true;
                        emit siteHighlighted(hoveredSiteName, true);
                    }
                }
            } else if (selecting == SelectAdd) {  // For SelectAdd, toggle all linked and enabled sites.

                bool selected = false;
                if (selectedSites.size() > 0) {
                    selected = !selectedSites[0]->highlighted;
                }

                for (int site = 0; site < selectedSites.size(); ++site) {
                    if (selectedSites[site]->linked && selectedSites[site]->enabled) {
                        selectedSites[site]->highlighted = selected;
                        emit siteHighlighted(hoveredSiteName, selected);
                    }
                }
            }
        } else {  // Otherwise SelectAdd or SelectReplace on sites within the rectangle.

            // Get a list of sites within the rectangle. Highlight those sites
            // (and maybe un-highlight the other sites, depending on if this is SelectAdd or SelectReplace).
            QVector<ElectrodeSite*> selectedSites = getSitesWithinSelectRect();

            // For SelectReplace, begin by deselecting all linked and enabled sites present in the page.
            if (selecting == SelectReplace) {
                emit deselectAllSites();
            }

            // For both SelectReplace and SelectAll, highlight all included linked and enabled sites present in the page.
            if (selecting != None) {
                for (int site = 0; site < selectedSites.size(); ++site) {
                    if (selectedSites[site]->linked && selectedSites[site]->enabled) {
                        selectedSites[site]->highlighted = true;
                        emit siteHighlighted(selectedSites[site]->nativeName, true);
                    }
                }
            }
        }
        selecting = None;
        update();
    }


    // Unset scrolling mode, and change the cursor to reflect that scrolling has ended.
    if (mouseMode == Scroll) {
        scrolling = false;
        setCursor(Qt::OpenHandCursor);
    } else if (mouseMode == Resize) {
        // Unset resizing mode, get ending coordinates, zoom to desired size, and emit signal that resize has finished.
        resizing = false;
        resizingEndX = event->pos().x();
        resizingEndY = event->pos().y();

        // If the user hasn't actually created a rectangle, just return without zooming.
        if (resizingEndX == resizingStartX || resizingEndY == resizingStartY) {
            update();
            return;
        }

        // Otherwise, actually zoom.
        float desiredNewXmin, desiredNewXmax, desiredNewYmin, desiredNewYmax;
        getDesiredSize(desiredNewXmin, desiredNewXmax, desiredNewYmin, desiredNewYmax);
        zoomToDesiredSize(desiredNewXmin, desiredNewXmax, desiredNewYmin, desiredNewYmax);
        emit resizeComplete();
    }
}

void PageView::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);

    // Get mouse position coordinates.
    mouseX = event->pos().x();
    mouseY = event->pos().y();

    updateMousePosition();
}

void PageView::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);
    int delta = event->angleDelta().y();

    if (event->modifiers() & Qt::ShiftModifier) {  // Shift + MouseWheel: scroll up and down.
        if (delta > 0) scrollUp();
        else scrollDown();
    } else if (event->modifiers() & Qt::ControlModifier) {  // Control + MouseWheel: scroll left and right.
        if (delta > 0) scrollRight();
        else scrollLeft();
    } else {  // MouseWheel: zoom in and out.
        if (delta > 0) zoomIn();
        else zoomOut();
    }
}

void PageView::resizeEvent(QResizeEvent* /* event */)
{
    // Pixel map used for double buffering - why is this needed?
    pixmap = QPixmap(size());
    pixmap.fill();
    update();
}

void PageView::enterEvent(QEnterEvent* /* event */)
{
    mousePresent = true;

    mouseX = 0;
    mouseY = 0;

    emit mouseEnter();
}

void PageView::leaveEvent(QEvent* /* event */)
{
    mousePresent = false;

    mouseX = -1;
    mouseY = -1;

    emit mouseExit();
}

QSize PageView::minimumSizeHint() const
{
    return QSize(PageViewXSize / 10, PageViewYSize / 10);
}

QSize PageView::sizeHint() const
{
    return QSize(PageViewXSize, PageViewYSize);
}

void PageView::initializeDisplay()
{
    // Get limits of frame.
    visibleFrameHeight = unitsToPixel * height();
    visibleFrameWidth = unitsToPixel * width();

    // Set frame around center.
    visibleFrameXmin = visibleFrameCenterX - (visibleFrameWidth/2);
    visibleFrameYmin = visibleFrameCenterY - (visibleFrameHeight/2);

    currentZoomWidth = visibleFrameWidth;
    currentZoomHeight = visibleFrameHeight;

    drawBackground();
    drawLines();
    drawTexts();
    drawSites();
    if (resizing || (selecting != None)) drawRect();
}

int PageView::xUnitsToPixelValue(float xUnit)
{
    // Equation for ratio: P = (U - visibleFrameXmin) / visibleFrameWidth
    float ratio = ((xUnit - visibleFrameXmin) / visibleFrameWidth);
    return (int) (ratio * width());
}

int PageView::yUnitsToPixelValue(float yUnit)
{
    // Equation for ratio: P = (U - visibleFrameYmin) / visibleFrameHeight
    float ratio = ((yUnit - visibleFrameYmin) / visibleFrameHeight);

    // Flip, to be consistent with user coordinate system in which y increases north (in software, y increases south).
    ratio = 1 - ratio;

    return (int) (ratio * height());
}

float PageView::xPixelsToUnitValue(int xPixel)
{
    float ratio = (float) xPixel / (float) width();
    return (ratio * visibleFrameWidth) + visibleFrameXmin;
}

float PageView::yPixelsToUnitValue(int yPixel)
{
    float ratio = (float) yPixel / (float) height();
    ratio = 1 - ratio;

    return (ratio * visibleFrameHeight) + visibleFrameYmin;
}

int PageView::xSizeToPixelSize(float xSize)
{
    // Equation for ratio: P = S / visibleFrameWidth
    float ratio = xSize / visibleFrameWidth;
    return (int) round(ratio * width());
}

int PageView::ySizeToPixelSize(float ySize)
{
    // Equation for ratio: P = S / visibleFrameHeight
    float ratio = ySize / visibleFrameHeight;
    return (int) round(ratio * height());
}

void PageView::getDesiredSize(float &newXMin, float &newXMax, float &newYMin, float &newYMax)
{
    float x1 = xPixelsToUnitValue(resizeRect.topLeft().x());
    float y1 = yPixelsToUnitValue(resizeRect.topLeft().y());
    float x2 = xPixelsToUnitValue(resizeRect.bottomRight().x());
    float y2 = yPixelsToUnitValue(resizeRect.bottomRight().y());

    newXMin = std::min(x1, x2);
    newYMin = std::min(y1, y2);
    newXMax = std::max(x1, x2);
    newYMax = std::max(y1, y2);
}

void PageView::zoomToDesiredSize(float newXMin, float newXMax, float newYMin, float newYMax)
{
    // Compare areas of visible rectangle and best-fit rectangle.
    // If visible rectangle's area is too small, no more zooming in.
    float visibleArea = visibleFrameWidth * visibleFrameHeight;
    float bestfitArea = bestfitWidth * bestfitHeight;
    if (visibleArea < bestfitArea / 400.0) {
        update();
        return;
    }

    // Determine what boundaries of widget are in USER UNITS.
    float widgetAspectRatio = float(height()) / float(width());
    float desiredAspectRatio = (newYMax - newYMin) / (newXMax - newXMin);

    if (widgetAspectRatio < desiredAspectRatio) {
        // Height of desired rectangle is the limit.
        visibleFrameHeight = newYMax - newYMin;
        visibleFrameYmin = newYMin;

        unitsToPixel = (newYMax - newYMin)/height();

        visibleFrameWidth = unitsToPixel * width();
        visibleFrameCenterX = (newXMin + newXMax) / 2;
        visibleFrameXmin = visibleFrameCenterX - visibleFrameWidth/2;
        visibleFrameCenterY = visibleFrameYmin + visibleFrameHeight/2;
    } else {
        // Width of desired rectangle is the limit.
        visibleFrameWidth = newXMax - newXMin;
        visibleFrameXmin = newXMin;

        unitsToPixel = (newXMax - newXMin)/width();

        visibleFrameHeight = unitsToPixel * height();
        visibleFrameCenterY = (newYMin + newYMax) / 2;
        visibleFrameYmin = visibleFrameCenterY - visibleFrameHeight/2;
        visibleFrameCenterX = visibleFrameXmin + visibleFrameWidth/2;
    }
    currentZoomWidth = visibleFrameWidth;
    currentZoomHeight = visibleFrameHeight;

    update();

    initializeDisplay();
    updateMousePosition();
}

void PageView::scrollOffset(float xOffset, float yOffset)
{
    visibleFrameCenterX += xOffset;
    visibleFrameCenterY += yOffset;

    update();
}

QString PageView::siteAtCoordinates(float xCoord, float yCoord)
{
    // Cycle through all ports.
    for (int port = 0; port < page.ports.size(); ++port) {
        // Cycle through all sites in this port.
        for (int site = 0; site < page.ports[port].electrodeSites.size(); ++site) {
            bool match = siteIncludesCoordinates(port, site, xCoord, yCoord);
            if (match) return page.ports[port].electrodeSites[site].nativeName;
        }
    }
    return QString("");
}

bool PageView::siteIncludesCoordinates(int port, int site, float xCoord, float yCoord)
{
    // If given coordinates lie outside of bounding rectangle, just return false.
    if (!(xCoord >= page.ports[port].electrodeSites[site].boundXmin && xCoord <= page.ports[port].electrodeSites[site].boundXmax &&
          yCoord >= page.ports[port].electrodeSites[site].boundYmin && yCoord <= page.ports[port].electrodeSites[site].boundYmax)) {
        return false;
    }

    if (getSiteShape(port, site) == "Rectangle") {  // If site shape is rectangle, then return true.
        return true;
    } else if (getSiteShape(port, site) == "Ellipse") {  // If site shape is ellipse, then do ellipse calculation.
        // Ellipse equation: (x^2)/(with/2)^2 + (y^2)/(height/2)^2 = 1
        float x = xCoord - page.ports[port].electrodeSites[site].x;
        float y = yCoord - page.ports[port].electrodeSites[site].y;

        // If left side of equation > 1, given point is outside ellipse. If left side of equation <= 1, given point is inside ellipse.
        float leftSide = pow(x, 2) / pow((getSiteWidth(port, site)/2), 2);
        leftSide += pow(y, 2) / pow((getSiteHeight(port, site)/2), 2);
        return leftSide <= 1;
    }

    // Shouldn't happen - just return false.
    return false;
}

void PageView::drawBackground()
{
    QPainter painter;
    painter.begin(&pixmap);

    // Clear entire Widget display area.
    painter.eraseRect(rect());

    // Draw border around Widget display area.
    painter.setPen(Qt::darkGray);
    QRect rect(0, 0, width() - 1, height() - 1);

    painter.fillRect(rect, getBackgroundColor());
    painter.drawRect(rect);
}

void PageView::drawLines()
{
    QPainter painter;
    painter.begin(&pixmap);

    // Draw lines where they are specified.
    for (int line = 0; line < page.lines.size(); ++line) {
        int x1 = xUnitsToPixelValue(page.lines[line].x1);
        int y1 = yUnitsToPixelValue(page.lines[line].y1);
        int x2 = xUnitsToPixelValue(page.lines[line].x2);
        int y2 = yUnitsToPixelValue(page.lines[line].y2);

        // Set pen to correct color.
        painter.setPen(getLineColor(line));

        // Draw line.
        painter.drawLine(x1, y1, x2, y2);
    }
}

void PageView::drawTexts()
{
    QPainter painter;
    painter.begin(&pixmap);

    // Draw texts where they are specified.
    for (int text = 0; text < page.texts.size(); ++text) {
        int topLeftX = xUnitsToPixelValue(page.texts[text].topLeftX);
        int topLeftY = yUnitsToPixelValue(page.texts[text].topLeftY);
        float rotation = -1 * page.texts[text].rotation;
        int fontSizePixels = ySizeToPixelSize(getFontHeight(text));
        QString textContent = page.texts[text].text;

        // Set pen to correct color.
        painter.setPen(getFontColor(text));

        // Draw text.
        QFont thisText("Courier New");
        thisText.setPixelSize(fontSizePixels);
        QFontMetrics metrics(thisText);
        painter.setFont(thisText);

        painter.translate(topLeftX, topLeftY);
        painter.rotate(rotation);

        QPointF topLeft(0, 0);
        QPointF bottomRight(metrics.horizontalAdvance(textContent), metrics.height());
        QRectF thisRect(topLeft, bottomRight);
        painter.drawText(thisRect, textContent);
        painter.rotate(-1 * rotation);
        painter.translate(-1 * topLeftX, -1 * topLeftY);
    }
}

void PageView::calculateCorners(const QString alignment, const float height, const float width, const float rotation, float &aX0, float &aY0, float &bX0, float &bY0, float &cX0, float &cY0, float &dX0, float &dY0)
{
    /* A ---------- B
     * |            |
     * |  text ...  |
     * |            |
     * D -----------C */

    if (alignment == "BottomLeft") {
        dX0 = 0;
        dY0 = 0;
        aX0 = height * std::cos((Pi/2) + rotation);
        aY0 = height * std::sin((Pi/2) + rotation);
        cX0 = width * std::cos(rotation);
        cY0 = width * std::sin(rotation);
        bX0 = cX0 + aX0;
        bY0 = cY0 + aY0;
    } else if (alignment == "TopLeft") {
        aX0 = 0;
        aY0 = 0;
        dX0 = height * std::cos((3 * Pi / 2) + rotation);
        dY0 = height * std::sin((3 * Pi / 2) + rotation);
        bX0 = width * std::cos(rotation);
        bY0 = width * std::sin(rotation);
        cX0 = bX0 + dX0;
        cY0 = bY0 + dY0;
    } else if (alignment == "TopRight") {
        bX0 = 0;
        bY0 = 0;
        cX0 = height * std::cos((3 * Pi / 2) + rotation);
        cY0 = height * std::sin((3 * Pi / 2) + rotation);
        aX0 = width * std::cos((Pi) + rotation);
        aY0 = width * std::sin((Pi) + rotation);
        dX0 = aX0 + cX0;
        dY0 = aY0 + cY0;
    } else if (alignment == "BottomRight") {
        cX0 = 0;
        cY0 = 0;
        bX0 = height * std::cos((Pi / 2) + rotation);
        bY0 = height * std::sin((Pi / 2) + rotation);
        dX0 = width * std::cos((Pi) + rotation);
        dY0 = width * std::sin((Pi) + rotation);
        aX0 = dX0 + bX0;
        aY0 = dY0 + bY0;
    } else if (alignment == "CenterLeft") {
        aX0 = 0.5 * height * std::cos((Pi / 2) + rotation);
        aY0 = 0.5 * height * std::sin((Pi / 2) + rotation);
        dX0 = 0.5 * height * std::cos((3 * Pi / 2) + rotation);
        dY0 = 0.5 * height * std::sin((3 * Pi / 2) + rotation);
        bX0 = width * std::cos(rotation) + aX0;
        bY0 = width * std::sin(rotation) + aY0;
        cX0 = width * std::cos(rotation) + dX0;
        cY0 = width * std::sin(rotation) + dY0;
    } else if (alignment == "CenterTop") {
        aX0 = 0.5 * width * std::cos((Pi) + rotation);
        aY0 = 0.5 * width * std::sin((Pi) + rotation);
        bX0 = 0.5 * width * std::cos(rotation);
        bY0 = 0.5 * width * std::sin(rotation);
        cX0 = height * std::cos((3 * Pi / 2) + rotation) + bX0;
        cY0 = height * std::sin((3 * Pi / 2) + rotation) + bY0;
        dX0 = height * std::cos((3 * Pi / 2) + rotation) + aX0;
        dY0 = height * std::sin((3 * Pi / 2) + rotation) + aY0;
    } else if (alignment == "CenterRight") {
        bX0 = 0.5 * height * std::cos((Pi / 2) + rotation);
        bY0 = 0.5 * height * std::sin((Pi / 2) + rotation);
        cX0 = 0.5 * height * std::cos((3 * Pi / 2) + rotation);
        cY0 = 0.5 * height * std::sin((3 * Pi / 2) + rotation);
        aX0 = width * std::cos((Pi) + rotation) + bX0;
        aY0 = width * std::sin((Pi) + rotation) + bY0;
        dX0 = width * std::cos((Pi) + rotation) + cX0;
        dY0 = width * std::sin((Pi) + rotation) + cY0;
    } else if (alignment == "CenterBottom") {
        cX0 = 0.5 * width * std::cos(rotation);
        cY0 = 0.5 * width * std::sin(rotation);
        dX0 = 0.5 * width * std::cos((Pi) + rotation);
        dY0 = 0.5 * width * std::sin((Pi) + rotation);
        bX0 = height * std::cos((Pi / 2) + rotation) + cX0;
        bY0 = height * std::sin((Pi / 2) + rotation) + cY0;
        aX0 = height * std::cos((Pi / 2) + rotation) + dX0;
        aY0 = height * std::sin((Pi / 2) + rotation) + dY0;
    } else if (alignment == "Center") {
        float centerLeftX = 0.5 * width * std::cos((Pi) + rotation);
        float centerLeftY = 0.5 * width * std::sin((Pi) + rotation);
        float centerTopX = 0.5 * height * std::cos((Pi / 2) + rotation);
        float centerTopY = 0.5 * height * std::sin((Pi / 2) + rotation);
        float centerRightX = 0.5 * width * std::cos(rotation);
        float centerRightY = 0.5 * width * std::sin(rotation);
        float centerBottomX = 0.5 * height * std::cos((3 * Pi / 2) + rotation);
        float centerBottomY = 0.5 * height * std::sin((3 * Pi / 2) + rotation);

        aX0 = centerLeftX + centerTopX;
        aY0 = centerLeftY + centerTopY;
        bX0 = centerRightX + centerTopX;
        bY0 = centerRightY + centerTopY;
        cX0 = centerRightX + centerBottomX;
        cY0 = centerRightY + centerBottomY;
        dX0 = centerLeftX + centerBottomX;
        dY0 = centerLeftY + centerBottomY;
    } else {
        qDebug() << "This alignment not supported yet...";
    }
}

void PageView::drawSites()
{
    QPainter painter;
    painter.begin(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    for (int port = 0; port < page.ports.size(); ++port) {
        for (int site = 0; site < page.ports[port].electrodeSites.size(); ++site) {

            // Set pen to correct color.
            painter.setPen(getSiteOutlineColor(port, site));

            // Get correct width and height of site, according to hierarchy from xml, and convert them to pixel size.
            int siteWidth = xSizeToPixelSize(getSiteWidth(port, site));
            int siteHeight = ySizeToPixelSize(getSiteHeight(port, site));

            // Get correct shape.
            QString shape = getSiteShape(port, site);

            // Get x and y coordinates of site in pixels.
            QPointF siteOrigin(xUnitsToPixelValue(page.ports[port].electrodeSites[site].x), yUnitsToPixelValue(page.ports[port].electrodeSites[site].y));

            if (shape != "Ellipse" && shape != "Rectangle") {
                qDebug() << "This electrode site didn't have a valid shape... not plotting";
                return;
            }
            bool ellipse = shape == "Ellipse";
            double a = ((double) siteWidth / 2.0);
            double b = ((double) siteHeight / 2.0);

            // If this site is disabled, draw X at this location, and continue to the next site.
            if (!page.ports[port].electrodeSites[site].enabled) {
                painter.setBrush(Qt::NoBrush);
                if (ellipse) {
                    painter.drawEllipse(siteOrigin, a, b);
                    drawX(painter, siteOrigin, a, b, ellipse);
                } else {
                    painter.drawRect(siteOrigin.x() - a, siteOrigin.y() - b, siteWidth, siteHeight);
                    drawX(painter, siteOrigin, a, b, ellipse);
                }
                continue;
            }

            // If this site is not linked, set brush to black.
            if (!page.ports[port].electrodeSites[site].linked) {
                painter.setBrush(Qt::black);
            }

            switch (viewMode) {
            case DefaultView:
                // Set brush based on enabled and highlighted states.
                // If this site is highlighted, set brush to color.
                if (page.ports[port].electrodeSites[site].highlighted) {
                    painter.setBrush(QColor(page.ports[port].electrodeSites[site].color));
                } else {
                    // Otherwise, just draw the outline.
                    painter.setBrush(Qt::NoBrush);
                }
                break;

            case ImpedanceView:
                // Set brush based on impedance magnitude.
                // If the impedance magnitude hasn't been initialized yet, draw as dark gray.
                if (!page.ports[port].electrodeSites[site].impedanceValid) {
                    painter.setBrush(Qt::darkGray);
                } else {
                    // Otherwise, represent the magnitude on a log scale from
                    // 0 to 1 where 0 is 10k (and lower) and 1 is 10M (and higher).
                    float magnitudeScaled = (log10(page.ports[port].electrodeSites[site].impedanceMag) - 4) / 3.0;
                    if (magnitudeScaled < 0) magnitudeScaled = 0;
                    else if (magnitudeScaled > 1) magnitudeScaled = 1;
                    painter.setBrush(ImpedanceGradient::getColor(magnitudeScaled));
                }
                break;

            case SpikeView:
                // Set brush based on spike activity
                painter.setBrush(SpikeGradient::getColor(page.ports[port].electrodeSites[site].spikeTime, (float) state->getDecayTime()));
                break;
            }

            // Draw shape.
            if (ellipse) {
                painter.drawEllipse(siteOrigin, a, b);
            } else {
                painter.drawRect(siteOrigin.x() - a, siteOrigin.y() - b, siteWidth, siteHeight);
            }
        }
    }
}

void PageView::drawX(QPainter &painter, QPointF siteOrigin, double a, double b, bool ellipse)
{
    // For ellipses, a and b inputs are the semi-axes that are used to calculate the offset
    // from the origin to a point along the ellipse at 45 degrees.
    // Calculate these offsets and populate a and b with these values
    if (ellipse) {
        // Calculate d as the distance from the center of the ellipse to a point 45 degrees from horizontal
        double numerator = sqrt(2) * a * b;
        double denominator = sqrt(a * a + b * b);
        double d = numerator / denominator;

        // Calculate offset from origin that determines the x/y coordinate of a point along the ellipse at 45 degrees
        double offset = d / sqrt(2);

        // At 45 degrees, the x and y offsets are the same
        a = offset;
        b = offset;
    }

    // For rectangles, a and b are simply the offsets (horizontal and vertical)
    // from the origin to get the corners, and don't need to be changed.

    // Determine corner coordinates
    QPointF topLeft(siteOrigin.x() - a, siteOrigin.y() - b);
    QPointF topRight(siteOrigin.x() + a, siteOrigin.y() - b);
    QPointF bottomLeft(siteOrigin.x() - a, siteOrigin.y() + b);
    QPointF bottomRight(siteOrigin.x() + a, siteOrigin.y() + b);

    // Draw 2 lines to form X
    painter.drawLine(topLeft, bottomRight);
    painter.drawLine(bottomLeft, topRight);
}

void PageView::drawRect()
{
    QPainter painter;
    painter.begin(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(Qt::white);
    painter.setBrush(QColor(200, 200, 200, 100));

    if (resizing) {
        painter.drawRect(resizeRect);
    } else if (selecting != None) {
        painter.drawRect(selectRect);
    }
}

QColor PageView::getBackgroundColor()
{
    // Start with default color (black).
    QColor backgroundcolor("Black");

    // If global color exists and is valid, overwrite previous color.
    overwriteColor(backgroundcolor, backgroundColor);

    // If page-level color exists and is valid, overwrite previous color.
    overwriteColor(backgroundcolor, page.backgroundColor);

    return backgroundcolor;
}

QColor PageView::getLineColor(int line)
{
    // Start with default color (white).
    QColor line_color("White");

    // If global color exists and is valid, overwrite previous color.
    overwriteColor(line_color, lineColor);

    // If page-level color exists and is valid, overwrite previous color.
    overwriteColor(line_color, page.lineColor);

    // If line-level color exists and is valid, overwrite previous color.
    overwriteColor(line_color, page.lines[line].lineColor);

    return line_color;
}

QColor PageView::getSiteOutlineColor(int port, int site)
{
    // Start with default color (gray).
    QColor site_outline_color("Gray");

    // If global color exists and is valid, overwrite previous color.
    overwriteColor(site_outline_color, siteOutlineColor);

    // If page-level color exists and is valid, overwrite previous color.
    overwriteColor(site_outline_color, page.siteOutlineColor);

    // If port-level color exists and is valid, overwrite previous color.
    overwriteColor(site_outline_color, page.ports[port].siteOutlineColor);

    // If site-level color exists and is valid, overwrite previous color.
    overwriteColor(site_outline_color, page.ports[port].electrodeSites[site].siteOutlineColor);

    return site_outline_color;
}

QColor PageView::getFontColor(int text)
{
    // Start with default color (white).
    QColor font_color("White");

    // If global color exists and is valid, overwrite previous color.
    overwriteColor(font_color, fontColor);

    // If page-level color exists and is valid, overwrite previous color.
    overwriteColor(font_color, page.fontColor);

    // If text-level color exists and is valid, overwrite previous color.
    overwriteColor(font_color, page.texts[text].fontColor);

    return font_color;
}

float PageView::getFontHeight(int text)
{
    // Start with default height (0).
    float font_height = 0;

    // If global font height exists and is valid, overwrite previous font height.
    overwriteFloat(font_height, fontHeight);

    // If page-level font height exists and is valid, overwrite previous font height.
    overwriteFloat(font_height, page.fontHeight);

    // If text-level font height exists and is valid, overwrite previous font height.
    overwriteFloat(font_height, page.texts[text].fontHeight);

    return font_height;
}

QString PageView::getTextAlignment(int text)
{
    // Start with default text alignment (BottomLeft).
    QString text_alignment("BottomLeft");

    // If global text alignment exists and is valid, overwrite previous text alignment.
    overwriteAlignment(text_alignment, textAlignment);

    // If page-level text alignment exists and is valid, overwrite previous text alignment.
    overwriteAlignment(text_alignment, page.textAlignment);

    // If text-level text alignment exists and is valid, overwrite previous text alignment.
    overwriteAlignment(text_alignment, page.texts[text].textAlignment);

    return text_alignment;
}

float PageView::getSiteWidth(int port, int site)
{
    // Start with default width (5).
    float site_width = 5;

    // If global width is specified and valid (not 0.0), overwrite previous width.
    overwriteFloat(site_width, siteWidth);

    // If page-level width is specified and valid (not 0.0), overwrite previous width.
    overwriteFloat(site_width, page.siteWidth);

    // If port-level width is specified and valid (not 0.0), overwrite previous width.
    overwriteFloat(site_width, page.ports[port].siteWidth);

    // If site-level width is specified and valid (not 0.0), overwrite previous width.
    overwriteFloat(site_width, page.ports[port].electrodeSites[site].siteWidth);

    return site_width;
}

float PageView::getSiteHeight(int port, int site)
{
    // Start with default height (5).
    float site_height = 5;

    // If global height is specified and valid (not 0.0), overwrite previous height.
    overwriteFloat(site_height, siteHeight);

    // If page-level height is specified and valid (not 0.0), overwrite previous height.
    overwriteFloat(site_height, page.siteHeight);

    // If port-level height is specified and valid (not 0.0), overwrite previous height.
    overwriteFloat(site_height, page.ports[port].siteHeight);

    // If site-level height is specified and valid (not 0.0), overwrite previous height.
    overwriteFloat(site_height, page.ports[port].electrodeSites[site].siteHeight);

    return site_height;
}

QString PageView::getSiteShape(int port, int site)
{
    // Start with default shape (Rectangle).
    QString site_shape = "Rectangle";

    // If global shape is specified and valid (not ""), overwrite previous shape.
    overwriteShape(site_shape, siteShape);

    // If page-level shape is specified and valid (not ""), overwrite previous shape.
    overwriteShape(site_shape, page.siteShape);

    // If port-level shape is specified and valid (not ""), overwrite previous shape.
    overwriteShape(site_shape, page.ports[port].siteShape);

    // If site-level shape is specified and valid (not ""), overwrite previous shape.
    overwriteShape(site_shape, page.ports[port].electrodeSites[site].siteShape);

    return site_shape;
}

void PageView::overwriteColor(QColor &color, const QString& colorName)
{
    if (color.isValidColorName(colorName))
        color = QColor::fromString(colorName);
}

void PageView::overwriteFloat(float &original, float newValue) {
    if (newValue > 0.0)
        original = newValue;
}

void PageView::overwriteShape(QString &original, const QString& newString)
{
    if ((newString == "Rectangle") || (newString == "Ellipse")) {
        original = newString;
    }
}

void PageView::overwriteAlignment(QString &original, const QString& newString)
{
    if ((newString == "BottomLeft") || (newString == "TopLeft") || (newString == "TopRight") || (newString == "BottomRight") ||
            (newString == "CenterLeft") || (newString == "CenterTop") || (newString == "CenterRight") || (newString == "CenterBottom") || (newString == "Center")) {
        original = newString;
    }
}

QVector<ElectrodeSite*> PageView::getSitesWithName(const QString& nativeName)
{
    QString name = nativeName.left(1);
    int channelNum = QStringView{nativeName}.right(nativeName.length() - 2).toInt();
    QVector<ElectrodeSite*> sites;

    for (int portIndex = 0; portIndex < page.ports.size(); ++portIndex) {
        if (page.ports[portIndex].name != name) {
            continue;
        }
        for (int siteIndex = 0; siteIndex < page.ports[portIndex].electrodeSites.size(); ++siteIndex) {
            if (page.ports[portIndex].electrodeSites[siteIndex].channelNumber == channelNum) {
                sites.append(&page.ports[portIndex].electrodeSites[siteIndex]);
            }
        }
    }
    return sites;
}

QVector<ElectrodeSite*> PageView::getSitesWithinSelectRect()
{
    QVector<ElectrodeSite*> sites;

    // Find the sites within the select rect.
    for (int portIndex = 0; portIndex < page.ports.size(); ++portIndex) {
        for (int siteIndex = 0; siteIndex < page.ports[portIndex].electrodeSites.size(); ++siteIndex) {
            QPoint thisSiteOrigin(xUnitsToPixelValue(page.ports[portIndex].electrodeSites[siteIndex].x), yUnitsToPixelValue(page.ports[portIndex].electrodeSites[siteIndex].y));

            // If this site is within the select rect, append it.
            if (selectRect.contains(thisSiteOrigin)) {
                sites.append(&page.ports[portIndex].electrodeSites[siteIndex]);
            }
        }
    }
    return sites;
}

void PageView::updateMousePosition()
{
    if (scrolling) {  // Scroll corresponding to the mouse movement.
        scrollOffset(scrollStartX - xPixelsToUnitValue(mouseX), scrollStartY - yPixelsToUnitValue(mouseY));
    } else if (selecting != None) {  // Update the select rect's end coordinates with current mouse position.
        selectingEndX = mouseX;
        selectingEndY = mouseY;
        selectRect.setCoords(selectingStartX, selectingStartY, selectingEndX, selectingEndY);
        update();
    } else if (resizing) {  // Update the resizing rect's end coordinates with current mouse position.
        resizingEndX = mouseX;
        resizingEndY = mouseY;
        resizeRect.setCoords(resizingStartX, resizingStartY, resizingEndX, resizingEndY);
        update();
    } else if (mouseMode == Select) {  // Determine if the user is hovering over a site.
        QString siteName = siteAtCoordinates(xPixelsToUnitValue(mouseX), yPixelsToUnitValue(mouseY));
        if (siteName.length() > 0) hoveredSiteName = siteName;
        else hoveredSiteName.clear();
    }

    // Signal that mouse has moved, in user units.
    emit mouseMoved(xPixelsToUnitValue(mouseX), yPixelsToUnitValue(mouseY), mousePresent, hoveredSiteName);
}
