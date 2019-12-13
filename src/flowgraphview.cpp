/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2003-2019, rncbc aka Rui Nuno Capela, qjackctl
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

#include "flowgraphview.h"

#include <QGraphicsScene>

#include <QGraphicsDropShadowEffect>

#include <QStyleOptionGraphicsItem>

#include <QTransform>
#include <QRubberBand>
#include <QUndoStack>
#include <QSettings>

#include <QPainter>
#include <QPalette>

#include <QGraphicsProxyWidget>
#include <QLineEdit>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDebug>

#include <algorithm>

#include <math.h>


//----------------------------------------------------------------------------
// FlowGraphItem -- Base graphics item.

// Constructor.
FlowGraphItem::FlowGraphItem ( QGraphicsItem *parent )
    : QGraphicsPathItem(parent), m_marked(false), m_hilite(false)
{
    const QPalette pal;
    m_foreground = pal.buttonText().color();
    m_background = pal.button().color();
}


// Basic color accessors.
void FlowGraphItem::setForeground ( const QColor& color )
{
    m_foreground = color;
}

const QColor& FlowGraphItem::foreground (void) const
{
    return m_foreground;
}


void FlowGraphItem::setBackground ( const QColor& color )
{
    m_background = color;
}

const QColor& FlowGraphItem::background (void) const
{
    return m_background;
}


// Marking methods.
void FlowGraphItem::setMarked ( bool marked )
{
    m_marked = marked;
}


bool FlowGraphItem::isMarked (void) const
{
    return m_marked;
}


// Highlighting methods.
void FlowGraphItem::setHighlight ( bool hilite )
{
    m_hilite = hilite;

    QGraphicsPathItem::update();
}


bool FlowGraphItem::isHighlight (void) const
{
    return m_hilite;
}


// Item-type hash (static)
uint FlowGraphItem::itemType ( const QByteArray& type_name )
{
    return qHash(type_name);
}


// Rectangular editor extents (virtual)
QRectF FlowGraphItem::editorRect (void) const
{
    return QRectF();
}


//----------------------------------------------------------------------------
// FlowGraphNodePort -- Port graphics item.

// Constructor.
FlowGraphNodePort::FlowGraphNodePort ( FlowGraphNode *node,
                                       const QString& name, FlowGraphItem::Mode mode, uint type )
    : FlowGraphItem(node), m_node(node),
      m_name(name), m_mode(mode), m_type(type), m_index(0),
      m_selectx(0), m_hilitex(0)
{
    QGraphicsPathItem::setZValue(+1);

    const QPalette pal;
    setForeground(pal.buttonText().color());
    setBackground(pal.button().color());

    m_text = new QGraphicsTextItem(this);

    QGraphicsPathItem::setFlag(QGraphicsItem::ItemIsSelectable);
    QGraphicsPathItem::setFlag(QGraphicsItem::ItemSendsScenePositionChanges);

    QGraphicsPathItem::setAcceptHoverEvents(true);

    QGraphicsPathItem::setToolTip(m_name);

    setPortTitle(m_name);
}


// Destructor.
FlowGraphNodePort::~FlowGraphNodePort (void)
{
    removeConnects();
}


// Accessors.
FlowGraphNode *FlowGraphNodePort::portNode (void) const
{
    return m_node;
}


void FlowGraphNodePort::setPortName ( const QString& name )
{
    m_name = name;

    QGraphicsPathItem::setToolTip(m_name);
}


const QString& FlowGraphNodePort::portName (void) const
{
    return m_name;
}


void FlowGraphNodePort::setPortMode ( FlowGraphItem::Mode mode )
{
    m_mode = mode;
}


FlowGraphItem::Mode FlowGraphNodePort::portMode (void) const
{
    return m_mode;
}


bool FlowGraphNodePort::isInput (void) const
{
    return (m_mode & Input);
}


bool FlowGraphNodePort::isOutput (void) const
{
    return (m_mode & Output);
}


void FlowGraphNodePort::setPortType ( uint type )
{
    m_type = type;
}


uint FlowGraphNodePort::portType (void) const
{
    return m_type;
}


void FlowGraphNodePort::setPortTitle ( const QString& title )
{
    m_title = (title.isEmpty() ? m_name : title);

    m_text->setPlainText(m_title);

    QPainterPath path;
    const QRectF& rect
            = m_text->boundingRect().adjusted(0, +2, 0, -2);
    path.addRoundedRect(rect, 5, 5);
    QGraphicsPathItem::setPath(path);
}


const QString& FlowGraphNodePort::portTitle (void) const
{
    return m_title;
}


void FlowGraphNodePort::setPortIndex ( int index )
{
    m_index = index;
}


int FlowGraphNodePort::portIndex (void) const
{
    return m_index;
}


QPointF FlowGraphNodePort::portPos (void) const
{
    QPointF pos = QGraphicsPathItem::scenePos();

    const QRectF& rect
            = QGraphicsPathItem::boundingRect();
    if (m_mode == Output)
        pos.setX(pos.x() + rect.width());
    pos.setY(pos.y() + rect.height() / 2);

    return pos;
}


// Connection-list methods.
void FlowGraphNodePort::appendConnect ( FlowGraphEdge *connect )
{
    m_connects.append(connect);
}


void FlowGraphNodePort::removeConnect ( FlowGraphEdge *connect )
{
    m_connects.removeAll(connect);
}


void FlowGraphNodePort::removeConnects (void)
{
    foreach (FlowGraphEdge *connect, m_connects) {
        if (connect->port1() != this)
            connect->setPort1(nullptr);
        if (connect->port2() != this)
            connect->setPort2(nullptr);
    }

    m_connects.clear();
}


FlowGraphEdge *FlowGraphNodePort::findConnect ( FlowGraphNodePort *port ) const
{
    foreach (FlowGraphEdge *connect, m_connects) {
        if (connect->port1() == port || connect->port2() == port)
            return connect;
    }

    return nullptr;
}


void FlowGraphNodePort::paint ( QPainter *painter,
                                const QStyleOptionGraphicsItem *option, QWidget */*widget*/ )
{
    const QPalette& pal = option->palette;

    if (QGraphicsPathItem::isSelected()) {
        m_text->setDefaultTextColor(pal.highlightedText().color());
        painter->setPen(pal.highlightedText().color());
        painter->setBrush(pal.highlight().color());
    } else {
        const QColor& foreground
                = FlowGraphItem::foreground();
        const QColor& background
                = FlowGraphItem::background();
        const bool is_dark
                = (background.value() < 128);
        m_text->setDefaultTextColor(is_dark
                                    ? foreground.lighter()
                                    : foreground.darker());
        if (FlowGraphItem::isHighlight() || QGraphicsPathItem::isUnderMouse()) {
            painter->setPen(foreground.lighter());
            painter->setBrush(background.lighter());
        } else {
            painter->setPen(foreground);
            painter->setBrush(background);
        }
    }

    painter->drawPath(QGraphicsPathItem::path());
}


QVariant FlowGraphNodePort::itemChange (
        GraphicsItemChange change, const QVariant& value )
{
    if (change == QGraphicsItem::ItemScenePositionHasChanged) {
        foreach (FlowGraphEdge *connect, m_connects) {
            connect->updatePath();
        }
    }
    else
        if (change == QGraphicsItem::ItemSelectedHasChanged && m_selectx < 1) {
            const bool is_selected = value.toBool();
            setHighlightEx(is_selected);
            foreach (FlowGraphEdge *connect, m_connects)
                connect->setSelectedEx(this, is_selected);
        }

    return value;
}


// Selection propagation method...
void FlowGraphNodePort::setSelectedEx ( bool is_selected )
{
    if (!is_selected) {
        foreach (FlowGraphEdge *connect, m_connects) {
            if (connect->isSelected()) {
                setHighlightEx(true);
                return;
            }
        }
    }

    ++m_selectx;

    setHighlightEx(is_selected);

    if (QGraphicsPathItem::isSelected() != is_selected)
        QGraphicsPathItem::setSelected(is_selected);

    --m_selectx;
}


// Highlighting propagation method...
void FlowGraphNodePort::setHighlightEx ( bool is_highlight )
{
    if (m_hilitex > 0)
        return;

    ++m_hilitex;

    FlowGraphItem::setHighlight(is_highlight);

    foreach (FlowGraphEdge *connect, m_connects)
        connect->setHighlightEx(this, is_highlight);

    --m_hilitex;
}


// Special port-type color business.
void FlowGraphNodePort::updatePortTypeColors ( FlowGraphView *canvas )
{
    if (canvas) {
        const QColor& color = canvas->portTypeColor(m_type);
        if (color.isValid()) {
            const bool is_dark = (color.value() < 128);
            FlowGraphItem::setForeground(is_dark
                                         ? color.lighter(180)
                                         : color.darker());
            FlowGraphItem::setBackground(color);
            if (m_mode & Output) {
                foreach (FlowGraphEdge *connect, m_connects) {
                    connect->updatePortTypeColors();
                    connect->update();
                }
            }
        }
    }
}



// Port sorting type.
FlowGraphNodePort::SortType  FlowGraphNodePort::g_sort_type  = FlowGraphNodePort::PortName;

void FlowGraphNodePort::setSortType ( SortType sort_type )
{
    g_sort_type = sort_type;
}

FlowGraphNodePort::SortType FlowGraphNodePort::sortType (void)
{
    return g_sort_type;
}


// Port sorting order.
FlowGraphNodePort::SortOrder FlowGraphNodePort::g_sort_order = FlowGraphNodePort::Ascending;

void FlowGraphNodePort::setSortOrder( SortOrder sort_order )
{
    g_sort_order = sort_order;
}

FlowGraphNodePort::SortOrder FlowGraphNodePort::sortOrder (void)
{
    return g_sort_order;
}


// Natural decimal sorting comparator (static)
bool FlowGraphNodePort::lessThan ( FlowGraphNodePort *port1, FlowGraphNodePort *port2 )
{
    const int port_type_diff
            = int(port1->portType()) - int(port2->portType());
    if (port_type_diff)
        return (port_type_diff > 0);

    if (g_sort_order == Descending) {
        FlowGraphNodePort *port = port1;
        port1 = port2;
        port2 = port;
    }

    if (g_sort_type == PortIndex) {
        const int port_index_diff
                = port1->portIndex() - port2->portIndex();
        if (port_index_diff)
            return (port_index_diff < 0);
    }

    switch (g_sort_type) {
    case PortTitle:
        return FlowGraphNodePort::lessThan(port1->portTitle(), port2->portTitle());
    case PortName:
    default:
        return FlowGraphNodePort::lessThan(port1->portName(), port2->portName());
    }
}

bool FlowGraphNodePort::lessThan ( const QString& s1, const QString& s2 )
{
    const int n1 = s1.length();
    const int n2 = s2.length();

    int i1, i2;

    for (i1 = i2 = 0; i1 < n1 && i2 < n2; ++i1, ++i2) {

        // Skip (white)spaces...
        while (s1.at(i1).isSpace())
            ++i1;
        while (s2.at(i2).isSpace())
            ++i2;

        // Normalize (to uppercase) the next characters...
        QChar c1 = s1.at(i1).toUpper();
        QChar c2 = s2.at(i2).toUpper();

        if (c1.isDigit() && c2.isDigit()) {
            // Find the whole length numbers...
            int j1 = i1++;
            while (i1 < n1 && s1.at(i1).isDigit())
                ++i1;
            int j2 = i2++;
            while (i2 < n2 && s2.at(i2).isDigit())
                ++i2;
            // Compare as natural decimal-numbers...
            j1 = s1.mid(j1, i1 - j1).toInt();
            j2 = s2.mid(j2, i2 - j2).toInt();
            if (j1 != j2)
                return (j1 < j2);
            // Never go out of bounds...
            if (i1 >= n1 || i2 >= n2)
                break;
            // Go on with this next char...
            c1 = s1.at(i1).toUpper();
            c2 = s2.at(i2).toUpper();
        }

        // Compare this char...
        if (c1 != c2)
            return (c1 < c2);
    }

    // Probable exact match.
    return false;
}


// Rectangular editor extents.
QRectF FlowGraphNodePort::editorRect (void) const
{
    return QGraphicsPathItem::sceneBoundingRect();
}


//----------------------------------------------------------------------------
// FlowGraphNode -- Node graphics item.

// Constructor.
FlowGraphNode::FlowGraphNode (
        const QString& name, FlowGraphItem::Mode mode, uint type )
    : FlowGraphItem(nullptr),
      m_name(name), m_mode(mode), m_type(type)
{
    QGraphicsPathItem::setZValue(0);

    const QPalette pal;
    const int base_value = pal.base().color().value();
    const bool is_dark = (base_value < 128);

    const QColor& text_color = pal.text().color();
    QColor foreground_color(is_dark
                            ? text_color.darker()
                            : text_color);
    FlowGraphItem::setForeground(foreground_color);

    const QColor& window_color = pal.window().color();
    QColor background_color(is_dark
                            ? window_color.lighter()
                            : window_color);
    background_color.setAlpha(160);
    FlowGraphItem::setBackground(background_color);

    m_pixmap = new QGraphicsPixmapItem(this);
    m_statusPix = new QGraphicsPixmapItem(this);
    m_titleText = new QGraphicsTextItem(this);
    m_statusText = new QGraphicsTextItem(this);
    m_infoText = new QGraphicsTextItem(this);

    QGraphicsPathItem::setFlag(QGraphicsItem::ItemIsMovable);
    QGraphicsPathItem::setFlag(QGraphicsItem::ItemIsSelectable);

    QGraphicsPathItem::setToolTip(m_name);
    setNodeTitle(m_name);

    const bool is_darkest = (base_value < 24);
    m_shadowColor = (is_darkest ? Qt::white : Qt::black);
    m_shadowColor.setAlpha(180);

    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
    effect->setColor(m_shadowColor);
    effect->setBlurRadius(is_darkest ? 8 : 16);
    effect->setOffset(is_darkest ? 0 : 2);
    QGraphicsPathItem::setGraphicsEffect(effect);

    updateNodeState(ModuleState::READY);
}


// Destructor.
FlowGraphNode::~FlowGraphNode (void)
{
    removePorts();
}


// accessors.
void FlowGraphNode::setNodeName ( const QString& name )
{
    m_name = name;

    QGraphicsPathItem::setToolTip(m_name);
}


const QString& FlowGraphNode::nodeName (void) const
{
    return m_name;
}


void FlowGraphNode::setNodeMode ( FlowGraphItem::Mode mode )
{
    m_mode = mode;
}


FlowGraphItem::Mode FlowGraphNode::nodeMode (void) const
{
    return m_mode;
}


void FlowGraphNode::setNodeType ( uint type )
{
    m_type = type;
}


uint FlowGraphNode::nodeType (void) const
{
    return m_type;
}


void FlowGraphNode::setNodeIcon (const QIcon& icon )
{
    m_icon = icon;

    m_pixmap->setPixmap(m_icon.pixmap(24, 24));
}


const QIcon& FlowGraphNode::nodeIcon (void) const
{
    return m_icon;
}


void FlowGraphNode::setNodeTitle ( const QString& title )
{
    const QFont& font = m_titleText->font();
    m_titleText->setFont(QFont(font.family(), font.pointSize(), QFont::Bold));
    m_titleText->setPlainText(title.isEmpty() ? m_name : title);
}


QString FlowGraphNode::nodeTitle (void) const
{
    return m_titleText->toPlainText();
}

void FlowGraphNode::updateNodeState(ModuleState state)
{
    const auto effect = static_cast<QGraphicsDropShadowEffect*>(this->graphicsEffect());
    effect->setColor(m_shadowColor);

    switch (state) {
    case ModuleState::INITIALIZING:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/preparing")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Initializing..."));
        break;
    case ModuleState::PREPARING:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/preparing")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Preparing..."));
        break;
    case ModuleState::WAITING:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/ready")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Waiting..."));
        break;
    case ModuleState::READY:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/ready")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Ready."));
        break;
    case ModuleState::RUNNING:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/running")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Running..."));
        break;
    case ModuleState::ERROR:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/error")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Error!"));
        effect->setColor(Qt::red);
        break;
    default:
        m_statusPix->setPixmap(QIcon(QPixmap(":/status/preparing")).pixmap(24, 24));
        m_statusText->setPlainText(QStringLiteral("Module is in an unknown state."));
        effect->setColor(Qt::red);
        break;
    }

    updatePath();
}

void FlowGraphNode::setNodeInfoText(const QString &info)
{
    m_infoText->setHtml(info);
}

QString FlowGraphNode::nodeInfoText() const
{
    return m_infoText->toPlainText();
}


// Port-list methods.
FlowGraphNodePort *FlowGraphNode::addPort (
        const QString& name, FlowGraphItem::Mode mode, uint type )
{
    FlowGraphNodePort *port = new FlowGraphNodePort(this, name, mode, type);

    m_ports.append(port);
    m_portkeys.insert(FlowGraphNodePort::PortKey(port), port);

    updatePath();

    return port;
}


FlowGraphNodePort *FlowGraphNode::addInputPort ( const QString& name, uint type )
{
    return addPort(name, FlowGraphItem::Input, type);
}


FlowGraphNodePort *FlowGraphNode::addOutputPort ( const QString& name, uint type )
{
    return addPort(name, FlowGraphItem::Output, type);
}


void FlowGraphNode::removePort ( FlowGraphNodePort *port )
{
    m_portkeys.remove(FlowGraphNodePort::PortKey(port));
    m_ports.removeAll(port);

    updatePath();
}


void FlowGraphNode::removePorts (void)
{
    foreach (FlowGraphNodePort *port, m_ports)
        port->removeConnects();

    m_ports.clear();
    m_portkeys.clear();
}


// Port finder (by name, mode and type)
FlowGraphNodePort *FlowGraphNode::findPort (
        const QString& name, FlowGraphItem::Mode mode, uint type )
{
    return static_cast<FlowGraphNodePort *> (
                m_portkeys.value(FlowGraphNodePort::ItemKey(name, mode, type), nullptr));
}


// Reset port markings, destroy if unmarked.
void FlowGraphNode::resetMarkedPorts (void)
{
    QList<FlowGraphNodePort *> ports;

    foreach (FlowGraphNodePort *port, m_ports) {
        if (port->isMarked()) {
            port->setMarked(false);
        } else {
            ports.append(port);
        }
    }

    foreach (FlowGraphNodePort *port, ports) {
        port->removeConnects();
        removePort(port);
        delete port;
    }
}


// Path/shape updater.
void FlowGraphNode::updatePath (void)
{
    const QRectF& titleRect = m_titleText->boundingRect();
    const QRectF& statusRect = m_statusText->boundingRect();
    const QRectF& infoRect = m_infoText->boundingRect();
    int width = static_cast<int>((titleRect.width() / 2) + (24 * 2));
    int wi, wo;
    wi = wo = width;
    foreach (FlowGraphNodePort *port, m_ports) {
        const int w = port->boundingRect().width();
        if (port->isOutput()) {
            if (wo < w) wo = w;
        } else {
            if (wi < w) wi = w;
        }
    }
    width = wi + wo;

    std::sort(m_ports.begin(), m_ports.end(), FlowGraphNodePort::Compare());

    int height = titleRect.height() + statusRect.height() + infoRect.height() + 2;
    int type = 0;
    int yi, yo;
    yi = yo = height;
    foreach (FlowGraphNodePort *port, m_ports) {
        const QRectF& port_rect = port->boundingRect();
        const int w = port_rect.width();
        const int h = port_rect.height() + 1;
        if (type - port->portType()) {
            type = port->portType();
            height += 2;
            yi = yo = height;
        }
        if (port->isOutput()) {
            port->setPos(+width / 2 + 6 - w, yo);
            yo += h;
            if (height < yo)
                height = yo;
        } else {
            port->setPos(-width / 2 - 6, yi);
            yi += h;
            if (height < yi)
                height = yi;
        }
    }

    m_infoText->setTextWidth(width);
    QPainterPath path;
    path.addRoundedRect(-width / 2, 0, width, height + 6, 5, 5);
    QGraphicsPathItem::setPath(path);
}


void FlowGraphNode::paint ( QPainter *painter,
                            const QStyleOptionGraphicsItem *option, QWidget */*widget*/ )
{
    const QPalette& pal = option->palette;

    if (QGraphicsPathItem::isSelected()) {
        const QColor& hilitetext_color = pal.highlightedText().color();
        m_titleText->setDefaultTextColor(hilitetext_color);
        m_statusText->setDefaultTextColor(hilitetext_color);
        m_infoText->setDefaultTextColor(hilitetext_color);
        painter->setPen(hilitetext_color);
        QColor hilite_color(pal.highlight().color());
        hilite_color.setAlpha(180);
        painter->setBrush(hilite_color);
    } else {
        const QColor& foreground
                = FlowGraphItem::foreground();
        const QColor& background
                = FlowGraphItem::background();
        const bool is_dark
                = (background.value() < 192);
        const auto defaultColor = is_dark
                ? foreground.lighter()
                : foreground.darker();
        m_titleText->setDefaultTextColor(defaultColor);
        m_statusText->setDefaultTextColor(defaultColor);
        m_infoText->setDefaultTextColor(defaultColor);
        painter->setPen(foreground);
        painter->setBrush(background);
    }

    painter->drawPath(QGraphicsPathItem::path());

    const QRectF& node_rect = QGraphicsPathItem::boundingRect();
    m_pixmap->setPos(node_rect.x() + 4, node_rect.y() + 4);
    m_statusPix->setPos(node_rect.x() + node_rect.width() - m_statusPix->pixmap().width() - 4, node_rect.y() + 4);

    const QRectF& title_rect = m_titleText->boundingRect();
    m_titleText->setPos(- title_rect.width() / 2, node_rect.y() + 2);

    const QRectF& status_rect = m_statusText->boundingRect();
    m_statusText->setPos(node_rect.x() + 4, title_rect.y() + title_rect.height());

    m_infoText->setPos(node_rect.x() + 4, status_rect.y() + (status_rect.height() * 2) - 4);
}


QVariant FlowGraphNode::itemChange (
        GraphicsItemChange change, const QVariant& value )
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        const bool is_selected = value.toBool();
        foreach (FlowGraphNodePort *port, m_ports)
            port->setSelected(is_selected);
    }

    return value;
}


// Rectangular editor extents.
QRectF FlowGraphNode::editorRect (void) const
{
    return m_titleText->sceneBoundingRect();
}


//----------------------------------------------------------------------------
// FlowGraphEdge -- Connection-line graphics item.

// Constructor.
FlowGraphEdge::FlowGraphEdge (void)
    : FlowGraphItem(nullptr), m_port1(nullptr), m_port2(nullptr)
{
    QGraphicsPathItem::setZValue(-1);

    QGraphicsPathItem::setFlag(QGraphicsItem::ItemIsSelectable);

    FlowGraphItem::setBackground(FlowGraphItem::foreground());

    const QPalette pal;
    const bool is_darkest = (pal.base().color().value() < 24);
    QColor shadow_color = (is_darkest ? Qt::white : Qt::black);
    shadow_color.setAlpha(220);

    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect();
    effect->setColor(shadow_color);
    effect->setBlurRadius(is_darkest ? 4 : 8);
    effect->setOffset(is_darkest ? 0 : 1);
    QGraphicsPathItem::setGraphicsEffect(effect);

    QGraphicsPathItem::setAcceptHoverEvents(true);
}


// Destructor.
FlowGraphEdge::~FlowGraphEdge (void)
{
    if (m_port1)
        m_port1->removeConnect(this);
    if (m_port2)
        m_port2->removeConnect(this);
}


// Accessors.
void FlowGraphEdge::setPort1 ( FlowGraphNodePort *port )
{
    if (m_port1)
        m_port1->removeConnect(this);

    m_port1 = port;

    if (m_port1)
        m_port1->appendConnect(this);

    if (m_port1 && m_port1->isSelected())
        setSelectedEx(m_port1, true);
}


FlowGraphNodePort *FlowGraphEdge::port1 (void) const
{
    return m_port1;
}


void FlowGraphEdge::setPort2 ( FlowGraphNodePort *port )
{
    if (m_port2)
        m_port2->removeConnect(this);

    m_port2 = port;

    if (m_port2)
        m_port2->appendConnect(this);

    if (m_port2 && m_port2->isSelected())
        setSelectedEx(m_port2, true);
}


FlowGraphNodePort *FlowGraphEdge::port2 (void) const
{
    return m_port2;
}


// Path/shaper updaters.
void FlowGraphEdge::updatePathTo ( const QPointF& pos )
{
    const bool is_out0 = m_port1->isOutput();
    const QPointF pos0 = m_port1->portPos();
    const QPointF pos1 = (is_out0 ? pos0 : pos);
    const QPointF pos4 = (is_out0 ? pos : pos0);

    FlowGraphNode *node1 = m_port1->portNode();
    const QRectF& rect1 = node1->boundingRect();
    const qreal dx = pos4.x() - pos1.x();
    const qreal dy = pos0.y() - node1->scenePos().y() - 0.5 * rect1.height();
    const qreal y_max = rect1.height() + rect1.width();
    const qreal y_min = qMin(y_max, qAbs(dx));
    const qreal x_offset = (dx > 0.0 ? 0.5 : 1.0) * y_min;
    const qreal y_offset = (dx > 0.0 ? 0.0 : (dy > 0.0 ? +y_min : -y_min));

    const QPointF pos2(pos1.x() + x_offset, pos1.y() + y_offset);
    const QPointF pos3(pos4.x() - x_offset, pos4.y() + y_offset);

    QPainterPath path;
    path.moveTo(pos1);
    path.cubicTo(pos2, pos3, pos4);
    const qreal arrow_angle = path.angleAtPercent(0.5) * M_PI / 180.0;
    const QPointF arrow_pos0 = path.pointAtPercent(0.5);
    path.cubicTo(pos3, pos2, pos1);
    const qreal arrow_size = 12.0;
    QVector<QPointF> arrow;
    arrow.append(arrow_pos0);
    arrow.append(arrow_pos0 - QPointF(
                     ::sin(arrow_angle + M_PI / 2.3) * arrow_size,
                     ::cos(arrow_angle + M_PI / 2.3) * arrow_size));
    arrow.append(arrow_pos0 - QPointF(
                     ::sin(arrow_angle + M_PI - M_PI / 2.3) * arrow_size,
                     ::cos(arrow_angle + M_PI - M_PI / 2.3) * arrow_size));
    arrow.append(arrow_pos0);
    path.addPolygon(QPolygonF(arrow));

    QGraphicsPathItem::setPath(path);
}


void FlowGraphEdge::updatePath (void)
{
    updatePathTo(m_port2->portPos());
}


void FlowGraphEdge::paint ( QPainter *painter,
                            const QStyleOptionGraphicsItem *option, QWidget */*widget*/ )
{
    if (QGraphicsPathItem::isSelected()) {
        const QPalette& pal
                = option->palette;
        const QColor& color
                = pal.highlight().color();
        painter->setPen(QPen(color, 2));
        painter->setBrush(color);
    } else {
        const QColor& color
                = FlowGraphItem::foreground();
        if (FlowGraphItem::isHighlight() || QGraphicsPathItem::isUnderMouse())
            painter->setPen(color.lighter());
        else
            painter->setPen(color);
        painter->setBrush(FlowGraphItem::background());
    }

    painter->drawPath(QGraphicsPathItem::path());
}


QVariant FlowGraphEdge::itemChange (
        GraphicsItemChange change, const QVariant& value )
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        const bool is_selected = value.toBool();
        FlowGraphItem::setHighlight(is_selected);
        if (m_port1)
            m_port1->setSelectedEx(is_selected);
        if (m_port2)
            m_port2->setSelectedEx(is_selected);
    }

    return value;
}


// Selection propagation method...
void FlowGraphEdge::setSelectedEx ( FlowGraphNodePort *port, bool is_selected )
{
    setHighlightEx(port, is_selected);

    if (QGraphicsPathItem::isSelected() != is_selected) {
        if (!is_selected || (m_port1 && m_port2
                             && m_port1->isSelected() && m_port2->isSelected())) {
            QGraphicsPathItem::setSelected(is_selected);
        }
    }
}

// Highlighting propagation method...
void FlowGraphEdge::setHighlightEx ( FlowGraphNodePort *port, bool is_highlight )
{
    FlowGraphItem::setHighlight(is_highlight);

    if (m_port1 && m_port1 != port)
        m_port1->setHighlight(is_highlight);
    if (m_port2 && m_port2 != port)
        m_port2->setHighlight(is_highlight);
}


// Special port-type color business.
void FlowGraphEdge::updatePortTypeColors (void)
{
    if (m_port1) {
        const QColor& color
                = m_port1->background().lighter();
        FlowGraphItem::setForeground(color);
        FlowGraphItem::setBackground(color);
    }
}


//----------------------------------------------------------------------------
// FlowGraphView -- Canvas graphics scene/view.

// Local constants.
static const char *CanvasGroup   = "/GraphCanvas";
static const char *CanvasRectKey = "/CanvasRect";
static const char *CanvasZoomKey = "/CanvasZoom";

static const char *NodePosGroup  = "/GraphNodePos";

static const char *ColorsGroup      = "/GraphColors";


// Constructor.
FlowGraphView::FlowGraphView ( QWidget *parent )
    : QGraphicsView(parent), m_state(DragNone), m_item(nullptr),
      m_connect(nullptr), m_rubberband(nullptr),
      m_zoom(1.0), m_zoomrange(false),
      m_settings(nullptr),
      m_selected_nodes(0), m_edit_item(nullptr),
      m_editor(nullptr), m_edited(0)
{
    m_scene = new QGraphicsScene;
    QGraphicsView::setScene(m_scene);

    QGraphicsView::setRenderHint(QPainter::Antialiasing);
    QGraphicsView::setRenderHint(QPainter::SmoothPixmapTransform);

    QGraphicsView::setResizeAnchor(QGraphicsView::NoAnchor);
    QGraphicsView::setDragMode(QGraphicsView::NoDrag);

    m_editor = new QLineEdit(this);
    m_editor->setFrame(false);

    QObject::connect(m_editor,
                     SIGNAL(textChanged(const QString&)),
                     SLOT(textChanged(const QString&)));
    QObject::connect(m_editor,
                     SIGNAL(editingFinished()),
                     SLOT(editingFinished()));

    m_editor->setEnabled(false);
    m_editor->hide();
}


// Destructor.
FlowGraphView::~FlowGraphView (void)
{
    delete m_editor;
    delete m_scene;
}


// Accessors.
QGraphicsScene *FlowGraphView::scene (void) const
{
    return m_scene;
}

void FlowGraphView::setSettings ( QSettings *settings )
{
    m_settings = settings;
}

QSettings *FlowGraphView::settings (void) const
{
    return m_settings;
}


// Canvas methods.
void FlowGraphView::addItem ( FlowGraphItem *item )
{
    m_scene->addItem(item);

    if (item->type() == FlowGraphNode::Type) {
        FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
        if (node) {
            m_nodes.append(node);
            m_nodekeys.insert(FlowGraphNode::NodeKey(node), node);
            if (!restoreNodePos(node))
                emit added(node);
        }
    }
}


void FlowGraphView::removeItem ( FlowGraphItem *item )
{
    clearSelection();

    if (item->type() == FlowGraphNode::Type) {
        FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
        if (node) {
            emit removed(node);
            node->removePorts();
            m_nodekeys.remove(FlowGraphNode::NodeKey(node));
            m_nodes.removeAll(node);
        }
    }
}


// Current item accessor.
FlowGraphItem *FlowGraphView::currentItem (void) const
{
    FlowGraphItem *item = m_item;

    if (item == nullptr) {
        const QList<QGraphicsItem *>& list
                = m_scene->selectedItems();
        if (!list.isEmpty())
            item = static_cast<FlowGraphItem *> (list.first());
    }

    return item;
}


// Connection predicates.
bool FlowGraphView::canConnect (void) const
{
    int nins = 0;
    int nouts = 0;

    foreach (QGraphicsItem *item, m_scene->selectedItems()) {
        if (item->type() == FlowGraphNode::Type) {
            FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
            if (node) {
                if (node->nodeMode() & FlowGraphItem::Input)
                    ++nins;
                else
                    //	if (node->nodeMode() & FlowGraphItem::Output)
                    ++nouts;
            }
        }
        else
            if (item->type() == FlowGraphNodePort::Type) {
                FlowGraphNodePort *port = static_cast<FlowGraphNodePort *> (item);
                if (port) {
                    if (port->isInput())
                        ++nins;
                    else
                        //	if (port->isOutput())
                        ++nouts;
                }
            }
        if (nins > 0 && nouts > 0)
            return true;
    }

    return false;
}


bool FlowGraphView::canDisconnect (void) const
{
    foreach (QGraphicsItem *item, m_scene->selectedItems()) {
        if (item->type() == FlowGraphEdge::Type)
            return true;
    }

    return false;
}


// Edit predicates.
bool FlowGraphView::canRenameItem (void) const
{
    FlowGraphItem *item = currentItem();

    return (item && (
                item->type() == FlowGraphNode::Type ||
                item->type() == FlowGraphNodePort::Type));
}


// Zooming methods.
void FlowGraphView::setZoom ( qreal zoom )
{
    if (zoom < 0.1)
        zoom = 0.1;
    else
        if (zoom > 2.0)
            zoom = 2.0;

    const qreal scale = zoom / m_zoom;
    QGraphicsView::scale(scale, scale);

    QFont font = m_editor->font();
    font.setPointSizeF(scale * font.pointSizeF());
    m_editor->setFont(font);
    updateEditorGeometry();

    m_zoom = zoom;

    emit changed();
}


qreal FlowGraphView::zoom (void) const
{
    return m_zoom;
}


void FlowGraphView::setZoomRange ( bool zoomrange )
{
    m_zoomrange = zoomrange;
}


bool FlowGraphView::isZoomRange (void) const
{
    return m_zoomrange;
}


// Clean-up all un-marked nodes...
void FlowGraphView::resetNodes ( uint node_type )
{
    QList<FlowGraphNode *> nodes;

    foreach (FlowGraphNode *node, m_nodes) {
        if (node->nodeType() == node_type) {
            if (node->isMarked()) {
                node->resetMarkedPorts();
                node->setMarked(false);
            } else {
                removeItem(node);
                nodes.append(node);
            }
        }
    }

    qDeleteAll(nodes);
}


void FlowGraphView::clearNodes ( uint node_type )
{
    QList<FlowGraphNode *> nodes;

    foreach (FlowGraphNode *node, m_nodes) {
        if (node->nodeType() == node_type) {
            m_nodekeys.remove(FlowGraphNode::NodeKey(node));
            m_nodes.removeAll(node);
            nodes.append(node);
        }
    }

    qDeleteAll(nodes);
}


// Special node finder.
FlowGraphNode *FlowGraphView::findNode (
        const QString& name, FlowGraphItem::Mode mode, uint type ) const
{
    return static_cast<FlowGraphNode *> (
                m_nodekeys.value(FlowGraphNode::ItemKey(name, mode, type), nullptr));
}


// Item finder (internal).
FlowGraphItem *FlowGraphView::itemAt ( const QPointF& pos ) const
{
    const QList<QGraphicsItem *>& items
            = m_scene->items(QRectF(pos - QPointF(2, 2), QSizeF(5, 5)));

    foreach (QGraphicsItem *item, items) {
        if (item->type() >= QGraphicsItem::UserType)
            return static_cast<FlowGraphItem *> (item);
    }

    return nullptr;
}


// Port (dis)connection command.
void FlowGraphView::connectPorts (
        FlowGraphNodePort *port1, FlowGraphNodePort *port2, bool is_connect )
{
    const bool is_connected // already connected?
            = (port1->findConnect(port2) != nullptr);
    if (( is_connect &&  is_connected) ||
            (!is_connect && !is_connected))
        return;

    FlowGraphNode *node1
            = findNode(
                port1->portNode()->nodeName(),
                FlowGraphItem::Duplex,
                port1->portNode()->nodeType());
    if (node1 == nullptr)
        node1 = findNode(
                    port1->portNode()->nodeName(),
                    FlowGraphItem::Output,
                    port1->portNode()->nodeType());
    if (node1 == nullptr)
        return;

    FlowGraphNode *node2
            = findNode(
                port2->portNode()->nodeName(),
                FlowGraphItem::Duplex,
                port2->portNode()->nodeType());
    if (node2 == nullptr)
        node2 = findNode(
                    port2->portNode()->nodeName(),
                    FlowGraphItem::Input,
                    port2->portNode()->nodeType());
    if (node2 == nullptr)
        return;

    if (is_connect)
        emit connected(port1, port2);
    else
        emit disconnected(port1, port2);
}


// Mouse event handlers.
void FlowGraphView::mousePressEvent ( QMouseEvent *event )
{
    m_state = DragNone;
    m_item = nullptr;
    m_pos = QGraphicsView::mapToScene(event->pos());

    FlowGraphItem *item = itemAt(m_pos);
    if (item && item->type() >= QGraphicsItem::UserType)
        m_item = static_cast<FlowGraphItem *> (item);

    if (event->button() == Qt::LeftButton)
        m_state = DragStart;

    if (m_state == DragStart && m_item == nullptr
            && (event->modifiers() & Qt::ControlModifier)
            && m_scene->selectedItems().isEmpty()) {
        QGraphicsView::setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
        m_state = DragScroll;
    }
}


void FlowGraphView::mouseMoveEvent ( QMouseEvent *event )
{
    int nchanged = 0;

    const QPointF& pos
            = QGraphicsView::mapToScene(event->pos());

    switch (m_state) {
    case DragStart:
        if ((pos - m_pos).manhattanLength() > 8.0) {
            m_state = DragMove;
            if (m_item) {
                // Start new connection line...
                if (m_item->type() == FlowGraphNodePort::Type) {
                    FlowGraphNodePort *port = static_cast<FlowGraphNodePort *> (m_item);
                    if (port) {
                        QGraphicsView::setCursor(Qt::DragLinkCursor);
                        m_selected_nodes = 0;
                        m_scene->clearSelection();
                        m_connect = new FlowGraphEdge();
                        m_connect->setPort1(port);
                        m_connect->setSelected(true);
                        m_scene->addItem(m_connect);
                        m_item = nullptr;
                        ++m_selected_nodes;
                        ++nchanged;
                    }
                }
                else
                    // Start moving nodes around...
                    if (m_item->type() == FlowGraphNode::Type) {
                        QGraphicsView::setCursor(Qt::SizeAllCursor);
                        if (!m_item->isSelected()) {
                            if ((event->modifiers()
                                 & (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
                                m_selected_nodes = 0;
                                m_scene->clearSelection();
                            }
                            m_item->setSelected(true);
                            ++nchanged;
                        }
                        // Original node position (for move command)...
                        QPointF pos1 = m_pos;
                        pos1.setX(4.0 * ::round(0.25 * pos1.x()));
                        pos1.setY(4.0 * ::round(0.25 * pos1.y()));
                        m_pos1 = pos1;
                    }
                    else m_item = nullptr;
            }
            // Otherwise start lasso rubber-banding...
            if (m_rubberband == nullptr && m_item == nullptr && m_connect == nullptr) {
                QGraphicsView::setCursor(Qt::CrossCursor);
                m_rubberband = new QRubberBand(QRubberBand::Rectangle, this);
            }
        }
        break;
    case DragMove:
        QGraphicsView::ensureVisible(QRectF(pos, QSizeF(2, 2)), 8, 8);
        // Move new connection line...
        if (m_connect)
            m_connect->updatePathTo(pos);
        // Move rubber-band lasso...
        if (m_rubberband) {
            const QRect rect(
                        QGraphicsView::mapFromScene(m_pos),
                        QGraphicsView::mapFromScene(pos));
            m_rubberband->setGeometry(rect.normalized());
            m_rubberband->show();
            if (!m_zoomrange) {
                if (event->modifiers()
                        & (Qt::ControlModifier | Qt::ShiftModifier)) {
                    foreach (QGraphicsItem *item, m_selected) {
                        item->setSelected(!item->isSelected());
                        ++nchanged;
                    }
                    m_selected.clear();
                } else {
                    m_selected_nodes = 0;
                    m_scene->clearSelection();
                    ++nchanged;
                }
                const QRectF range_rect(m_pos, pos);
                foreach (QGraphicsItem *item,
                         m_scene->items(range_rect.normalized())) {
                    if (item->type() >= QGraphicsItem::UserType) {
                        if (item->type() != FlowGraphNode::Type)
                            ++m_selected_nodes;
                        else
                            if (m_selected_nodes > 0)
                                continue;
                        const bool is_selected = item->isSelected();
                        if (event->modifiers() & Qt::ControlModifier) {
                            m_selected.append(item);
                            item->setSelected(!is_selected);
                        }
                        else
                            if (!is_selected) {
                                if (event->modifiers() & Qt::ShiftModifier)
                                    m_selected.append(item);
                                item->setSelected(true);
                            }
                        ++nchanged;
                    }
                }
            }
        }
        // Move current selected nodes...
        if (m_item && m_item->type() == FlowGraphNode::Type) {
            QPointF pos2 = pos;
            pos2.setX(4.0 * ::round(0.25 * pos2.x()));
            pos2.setY(4.0 * ::round(0.25 * pos2.y()));
            const QPointF delta = (pos2 - m_pos);
            foreach (QGraphicsItem *item, m_scene->selectedItems()) {
                if (item->type() == FlowGraphNode::Type) {
                    FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
                    if (node)
                        node->setPos(node->pos() + delta);
                }
            }
            m_pos = pos2;
        }
        else
            if (m_connect) {
                // Hovering ports high-lighting...
                QGraphicsItem *item = itemAt(pos);
                if (item && item->type() == FlowGraphNodePort::Type) {
                    FlowGraphNodePort *port1 = m_connect->port1();
                    FlowGraphNodePort *port2 = static_cast<FlowGraphNodePort *> (item);
                    if (port1 && port2 &&
                            port1->portType() == port2->portType() &&
                            port1->portMode() != port2->portMode()) {
                        port2->update();
                    }
                }
            }
        break;
    case DragScroll:
    default:
        QGraphicsView::mouseMoveEvent(event);
        break;
    }

    if (nchanged > 0)
        emit changed();
}


void FlowGraphView::mouseReleaseEvent ( QMouseEvent *event )
{
    int nchanged = 0;

    switch (m_state) {
    case DragStart:
        // Make individual item (de)selections...
        if ((event->modifiers()
             & (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
            m_selected_nodes = 0;
            m_scene->clearSelection();
            ++nchanged;
        }
        if (m_item) {
            bool is_selected = true;
            if (event->modifiers() & Qt::ControlModifier)
                is_selected = !m_item->isSelected();
            m_item->setSelected(is_selected);
            if (m_item->type() != FlowGraphNode::Type && is_selected)
                ++m_selected_nodes;
            m_item = nullptr; // Not needed anymore!
            ++nchanged;
        }

        /* fall through */
    case DragMove:
        // Close new connection line...
        if (m_connect) {
            const QPointF& pos
                    = QGraphicsView::mapToScene(event->pos());
            FlowGraphItem *item = itemAt(pos);
            if (item && item->type() == FlowGraphNodePort::Type) {
                FlowGraphNodePort *port1 = m_connect->port1();
                FlowGraphNodePort *port2 = static_cast<FlowGraphNodePort *> (item);
                if (port1 && port2
                        //	&& port1->portNode() != port2->portNode()
                        && port1->portMode() != port2->portMode()
                        && port1->portType() == port2->portType()
                        && port1->findConnect(port2) == nullptr) {
                    port2->setSelected(true);
                    //#if 0 // Sure the sect will commit to this instead...
                    m_connect->setPort2(port2);
                    m_connect->updatePathTo(port2->portPos());
                    m_connect = nullptr;
                    ++m_selected_nodes;
                    //#else
                    //	m_selected_nodes = 0;
                    //	m_scene->clearSelection();
                    //  #endif
                    // Submit command; notify eventual observers...
                    connectPorts(port1, port2, true);
                    ++nchanged;
                }
            }
            if (m_connect) {
                delete m_connect;
                m_connect = nullptr;
            }
        }
        // Maybe some node(s) were moved...
        if (m_item && m_item->type() == FlowGraphNode::Type) {
            QList<FlowGraphNode *> nodes;
            foreach (QGraphicsItem *item, m_scene->selectedItems()) {
                if (item->type() == FlowGraphNode::Type) {
                    FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
                    if (node)
                        nodes.append(node);
                }
            }
        }
        // Close rubber-band lasso...
        if (m_rubberband) {
            delete m_rubberband;
            m_rubberband = nullptr;
            m_selected.clear();
            // Zooming in range?...
            if (m_zoomrange) {
                const QRectF range_rect(m_pos,
                                        QGraphicsView::mapToScene(event->pos()));
                zoomFitRange(range_rect);
                nchanged = 0;
            }
        }
        break;
    case DragScroll:
    default:
        QGraphicsView::mouseReleaseEvent(event);
        QGraphicsView::setDragMode(QGraphicsView::NoDrag);
        break;
    }

    m_state = DragNone;
    m_item = nullptr;

    // Reset cursor...
    QGraphicsView::setCursor(Qt::ArrowCursor);

    if (nchanged > 0)
        emit changed();
}


void FlowGraphView::mouseDoubleClickEvent ( QMouseEvent *event )
{
    m_pos  = QGraphicsView::mapToScene(event->pos());
    m_item = itemAt(m_pos);

    if (m_item && canRenameItem()) {
        renameItem();
    } else {
        QGraphicsView::centerOn(m_pos);
    }
}


void FlowGraphView::wheelEvent ( QWheelEvent *event )
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta
        #if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                = event->delta();
#else
                = event->angleDelta().y();
#endif
        setZoom(zoom() + qreal(delta) / 1200.0);
    }
    else QGraphicsView::wheelEvent(event);
}


// Keyboard event handler.
void FlowGraphView::keyPressEvent ( QKeyEvent *event )
{
    if (event->key() == Qt::Key_Escape) {
        m_selected_nodes = 0;
        m_scene->clearSelection();
        if (m_rubberband) {
            delete m_rubberband;
            m_rubberband = nullptr;
            m_selected.clear();
        }
        if (m_connect) {
            delete m_connect;
            m_connect = nullptr;
        }
        if (m_state == DragScroll)
            QGraphicsView::setDragMode(QGraphicsView::NoDrag);
        m_state = DragNone;
        m_item = nullptr;
        m_edit_item = nullptr;
        m_editor->setEnabled(false);
        m_editor->hide();
        m_edited = 0;
        emit changed();
        // Reset cursor...
        QGraphicsView::setCursor(Qt::ArrowCursor);
    }
}


// Connect selected items.
void FlowGraphView::connectItems (void)
{
    QList<FlowGraphNodePort *> outs;
    QList<FlowGraphNodePort *> ins;

    foreach (QGraphicsItem *item, m_scene->selectedItems()) {
        if (item->type() == FlowGraphNodePort::Type) {
            FlowGraphNodePort *port = static_cast<FlowGraphNodePort *> (item);
            if (port) {
                if (port->isOutput())
                    outs.append(port);
                else
                    ins.append(port);
            }
        }
    }

    if (outs.isEmpty() || ins.isEmpty())
        return;

    //	m_selected_nodes = 0;
    //	m_scene->clearSelection();

    std::sort(outs.begin(), outs.end(), FlowGraphNodePort::ComparePos());
    std::sort(ins.begin(),  ins.end(),  FlowGraphNodePort::ComparePos());

    QListIterator<FlowGraphNodePort *> iter1(outs);
    QListIterator<FlowGraphNodePort *> iter2(ins);

    const int nports = qMax(outs.count(), ins.count());
    for (int n = 0; n < nports; ++n) {
        // Wrap a'round...
        if (!iter1.hasNext())
            iter1.toFront();
        if (!iter2.hasNext())
            iter2.toFront();
        // Submit command; notify eventual observers...
        FlowGraphNodePort *port1 = iter1.next();
        FlowGraphNodePort *port2 = iter2.next();
        if (port1 && port2 && port1->portType() == port2->portType())
            connectPorts(port1, port2, true);
    }
}


// Disconnect selected items.
void FlowGraphView::disconnectItems (void)
{
    QList<FlowGraphEdge *> connects;

    foreach (QGraphicsItem *item, m_scene->selectedItems()) {
        if (item->type() == FlowGraphEdge::Type) {
            FlowGraphEdge *connect = static_cast<FlowGraphEdge *> (item);
            if (connect)
                connects.append(connect);
        }
    }

    //	m_selected_nodes = 0;
    //	m_scene->clearSelection();

    foreach (FlowGraphEdge *connect, connects) {
        // Submit command; notify eventual observers...
        FlowGraphNodePort *port1 = connect->port1();
        FlowGraphNodePort *port2 = connect->port2();
        if (port1 && port2)
            connectPorts(port1, port2, false);
    }
}


// Select actions.
void FlowGraphView::selectAll (void)
{
    foreach (QGraphicsItem *item, m_scene->items()) {
        if (item->type() == FlowGraphNode::Type)
            item->setSelected(true);
        else
            ++m_selected_nodes;
    }

    emit changed();
}


void FlowGraphView::selectNone (void)
{
    m_selected_nodes = 0;
    m_scene->clearSelection();

    emit changed();
}


void FlowGraphView::selectInvert (void)
{
    foreach (QGraphicsItem *item, m_scene->items()) {
        if (item->type() == FlowGraphNode::Type)
            item->setSelected(!item->isSelected());
        else
            ++m_selected_nodes;
    }

    emit changed();
}


// Edit actions.
void FlowGraphView::renameItem (void)
{
    FlowGraphItem *item = currentItem();

    if (item && item->type() == FlowGraphNode::Type) {
        FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
        if (node) {
            QPalette pal;
            const QColor& foreground
                    = node->foreground();
            QColor background = node->background();
            const bool is_dark
                    = (background.value() < 192);
            pal.setColor(QPalette::Text, is_dark
                         ? foreground.lighter()
                         : foreground.darker());
            background.setAlpha(255);
            pal.setColor(QPalette::Base, background);
            m_editor->setPalette(pal);
            QFont font = m_editor->font();
            font.setBold(true);
            m_editor->setFont(font);
            m_editor->setPlaceholderText(node->nodeName());
            m_editor->setText(node->nodeTitle());
        }
    }
    else
        if (item && item->type() == FlowGraphNodePort::Type) {
            FlowGraphNodePort *port = static_cast<FlowGraphNodePort *> (item);
            if (port) {
                QPalette pal;
                const QColor& foreground
                        = port->foreground();
                const QColor& background
                        = port->background();
                const bool is_dark
                        = (background.value() < 128);
                pal.setColor(QPalette::Text, is_dark
                             ? foreground.lighter()
                             : foreground.darker());
                pal.setColor(QPalette::Base, background.lighter());
                m_editor->setPalette(pal);
                QFont font = m_editor->font();
                font.setBold(false);
                m_editor->setFont(font);
                m_editor->setPlaceholderText(port->portName());
                m_editor->setText(port->portTitle());
            }
        }
        else return;

    m_editor->show();
    m_editor->setEnabled(true);
    m_editor->selectAll();
    m_editor->setFocus();
    m_edited = 0;

    m_edit_item = item;

    updateEditorGeometry();
}


// Renaming editor position and size updater.
void FlowGraphView::updateEditorGeometry (void)
{
    if (m_edit_item && m_editor->isEnabled() && m_editor->isVisible()) {
        const QRectF& rect
                = m_edit_item->editorRect().adjusted(+2.0, +2.0, -2.0, -2.0);
        const QPoint& pos1
                = QGraphicsView::mapFromScene(rect.topLeft());
        const QPoint& pos2
                = QGraphicsView::mapFromScene(rect.bottomRight());
        m_editor->setGeometry(
                    pos1.x(),  pos1.y(),
                    pos2.x() - pos1.x(),
                    pos2.y() - pos1.y());
    }
}


// Discrete zooming actions.
void FlowGraphView::zoomIn (void)
{
    setZoom(zoom() + 0.1);
}


void FlowGraphView::zoomOut (void)
{
    setZoom(zoom() - 0.1);
}


void FlowGraphView::zoomFit (void)
{
    zoomFitRange(m_scene->itemsBoundingRect());
}


void FlowGraphView::zoomReset (void)
{
    setZoom(1.0);
}


// Update all nodes.
void FlowGraphView::updateNodes (void)
{
    foreach (QGraphicsItem *item, m_scene->items()) {
        if (item->type() == FlowGraphNode::Type) {
            FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
            if (node)
                node->updatePath();
        }
    }
}


// Zoom in rectangle range.
void FlowGraphView::zoomFitRange ( const QRectF& range_rect )
{
    QGraphicsView::fitInView(
                range_rect, Qt::KeepAspectRatio);

    const QTransform& transform
            = QGraphicsView::transform();
    if (transform.isScaling()) {
        qreal zoom = transform.m11();
        if (zoom < 0.1) {
            const qreal scale = 0.1 / zoom;
            QGraphicsView::scale(scale, scale);
            zoom = 0.1;
        }
        else
            if (zoom > 2.0) {
                const qreal scale = 2.0 / zoom;
                QGraphicsView::scale(scale, scale);
                zoom = 2.0;
            }
        m_zoom = zoom;
    }

    emit changed();
}


// Graph node position methods.
bool FlowGraphView::restoreNodePos ( FlowGraphNode *node )
{
    if (m_settings == nullptr || node == nullptr)
        return false;

    m_settings->beginGroup(NodePosGroup);
    const QPointF& node_pos
            = m_settings->value('/' + nodeKey(node)).toPointF();
    m_settings->endGroup();

    if (node_pos.isNull())
        return false;

    node->setPos(node_pos);
    return true;
}


bool FlowGraphView::saveNodePos ( FlowGraphNode *node ) const
{
    if (m_settings == nullptr || node == nullptr)
        return false;

    m_settings->beginGroup(NodePosGroup);
    m_settings->setValue('/' + nodeKey(node), node->pos());
    m_settings->endGroup();

    return true;
}


bool FlowGraphView::restoreState (void)
{
    if (m_settings == nullptr)
        return false;

    m_settings->beginGroup(ColorsGroup);
    const QRegExp rx("^0x");
    QStringListIterator key(m_settings->childKeys());
    while (key.hasNext()) {
        const QString& sKey = key.next();
        const QColor& color = QString(m_settings->value(sKey).toString());
        if (color.isValid()) {
            QString sx(sKey);
            bool ok = false;
            const uint port_type = sx.remove(rx).toUInt(&ok, 16);
            if (ok) m_port_colors.insert(port_type, color);
        }
    }
    m_settings->endGroup();

    m_settings->beginGroup(CanvasGroup);
    m_settings->setValue(CanvasRectKey, QGraphicsView::sceneRect());
    const QRectF& rect = m_settings->value(CanvasRectKey).toRectF();
    const qreal zoom = m_settings->value(CanvasZoomKey, 1.0).toReal();
    m_settings->endGroup();

    if (rect.isValid())
        QGraphicsView::setSceneRect(rect);

    setZoom(zoom);

    return true;
}


bool FlowGraphView::saveState (void) const
{
    if (m_settings == nullptr)
        return false;

    m_settings->beginGroup(NodePosGroup);
    const QList<QGraphicsItem *> items(m_scene->items());
    foreach (QGraphicsItem *item, items) {
        if (item->type() == FlowGraphNode::Type) {
            FlowGraphNode *node = static_cast<FlowGraphNode *> (item);
            if (node)
                m_settings->setValue('/' + nodeKey(node), node->pos());
        }
    }
    m_settings->endGroup();

    m_settings->beginGroup(CanvasGroup);
    m_settings->setValue(CanvasZoomKey, zoom());
    m_settings->setValue(CanvasRectKey, QGraphicsView::sceneRect());
    m_settings->endGroup();

    m_settings->beginGroup(ColorsGroup);
    QStringListIterator key(m_settings->childKeys());
    while (key.hasNext()) m_settings->remove(key.next());
    QHash<uint, QColor>::ConstIterator iter = m_port_colors.constBegin();
    const QHash<uint, QColor>::ConstIterator& iter_end = m_port_colors.constEnd();
    for ( ; iter != iter_end; ++iter) {
        const uint port_type = iter.key();
        const QColor& color = iter.value();
        m_settings->setValue("0x" + QString::number(port_type, 16), color.name());
    }
    m_settings->endGroup();

    return true;
}


// Graph node key mangler.
QString FlowGraphView::nodeKey ( FlowGraphNode *node ) const
{
    QString node_key = node->nodeName();

    switch (node->nodeMode()) {
    case FlowGraphItem::Input:
        node_key += ":Input";
        break;
    case FlowGraphItem::Output:
        node_key += ":Output";
        break;
    default:
        break;
    }

    return node_key;
}


// Graph port colors management.
void FlowGraphView::setPortTypeColor (
        uint port_type, const QColor& port_color )
{
    m_port_colors.insert(port_type, port_color);
}


const QColor& FlowGraphView::portTypeColor ( uint port_type )
{
    return m_port_colors[port_type];
}


void FlowGraphView::updatePortTypeColors ( uint port_type )
{
    foreach (QGraphicsItem *item, m_scene->items()) {
        if (item->type() == FlowGraphNodePort::Type) {
            FlowGraphNodePort *port = static_cast<FlowGraphNodePort *> (item);
            if (port && (0 >= port_type || port->portType() == port_type)) {
                port->updatePortTypeColors(this);
                port->update();
            }
        }
    }
}


void FlowGraphView::clearPortTypeColors (void)
{
    m_port_colors.clear();
}


// Clear all selection.
void FlowGraphView::clearSelection (void)
{
    m_item = nullptr;
    m_selected_nodes = 0;
    m_scene->clearSelection();

    m_edit_item = nullptr;
    m_editor->setEnabled(false);
    m_editor->hide();
    m_edited = 0;
}

QList<FlowGraphNode *> FlowGraphView::selectedNodes() const
{
    QList<FlowGraphNode*> nodes;
    foreach (auto item, m_scene->selectedItems()) {
        if (item->type() == FlowGraphNode::Type) {
            auto node = static_cast<FlowGraphNode *> (item);
            nodes.append(node);
        }
    }

    return nodes;
}

// Rename item slots.
void FlowGraphView::textChanged ( const QString& /* text */)
{
    if (m_edit_item && m_editor->isEnabled() && m_editor->isVisible())
        ++m_edited;
}


void FlowGraphView::editingFinished (void)
{
    if (m_edit_item && m_editor->isEnabled() && m_editor->isVisible()) {
        emit renamed(m_edit_item, m_editor->text());
        // Reset all renaming stuff...
        m_edit_item = nullptr;
        m_editor->setEnabled(false);
        m_editor->hide();
        m_edited = 0;
    }
}
