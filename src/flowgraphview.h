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

#ifndef FLOWGRAPHVIEW_H
#define FLOWGRAPHVIEW_H

#include <QGraphicsView>

#include <QGraphicsPathItem>

#include <QColor>
#include <QIcon>

#include <QList>
#include <QHash>

#include "streams/datatypes.h"

class FlowGraphView;
class FlowGraphNode;
class FlowGraphNodePort;
class FlowGraphEdge;

class QStyleOptionGraphicsItem;

class QRubberBand;
class QUndoCommand;
class QSettings;

class QGraphicsProxyWidget;
class QLineEdit;

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

class AbstractStreamPort;


//----------------------------------------------------------------------------
// FlowGraphItem -- Base graphics item.

class FlowGraphItem : public QGraphicsPathItem
{
public:

    // Constructor.
    FlowGraphItem(QGraphicsItem *parent = nullptr);

    // Basic color accessors.
    void setForeground(const QColor& color);
    const QColor& foreground() const;

    void setBackground(const QColor& color);
    const QColor& background() const;

    // Marking methods.
    void setMarked(bool marked);
    bool isMarked() const;

    // Highlighting methods.
    void setHighlight(bool hilite);
    bool isHighlight() const;

    // Item modes.
    enum Mode { None = 0,
                Input = 1, Output = 2,
                Duplex = Input | Output };

    // Item hash/map key.
    class ItemKey
    {
    public:

        // Constructors.
        ItemKey (const QString& name, Mode mode, uint type = 0)
            : m_name(name), m_mode(mode), m_type(type) {}
        ItemKey (const ItemKey& key)
            : m_name(key.name()), m_mode(key.mode()), m_type(key.type()) {}

        // Key accessors.
        const QString& name() const
        { return m_name; }
        Mode mode() const
        { return m_mode; }
        uint type() const
        { return m_type; }

        // Hash/map key comparators.
        bool operator== (const ItemKey& key) const
        {
            return ItemKey::type() == key.type()
                    && ItemKey::mode() == key.mode()
                    && ItemKey::name() == key.name();
        }

    private:

        // Key fields.
        QString m_name;
        Mode    m_mode;
        uint    m_type;
    };

    typedef QHash<ItemKey, FlowGraphItem *> ItemKeys;

    // Item-type hash (static)
    static uint itemType(const QByteArray& type_name);

    // Rectangular editor extents.
    virtual QRectF editorRect() const;

private:

    // Instance variables.
    QColor m_foreground;
    QColor m_background;

    bool m_marked;
    bool m_hilite;
};


// Item hash function.
inline uint qHash ( const FlowGraphItem::ItemKey& key )
{
    return qHash(key.name()) ^ qHash(uint(key.mode())) ^ qHash(key.type());
}


//----------------------------------------------------------------------------
// FlowGraphNodePort -- Port graphics item.

class FlowGraphNodePort : public FlowGraphItem
{
public:

    // Constructors.
    FlowGraphNodePort(FlowGraphNode *node);
    FlowGraphNodePort(FlowGraphNode *node,
                      std::shared_ptr<AbstractStreamPort> port);

    // Destructor.
    ~FlowGraphNodePort() override;

    // Graphics item type.
    enum { Type = QGraphicsItem::UserType + 2 };

    int type() const override { return Type; }

    // Accessors.
    FlowGraphNode *portNode() const;

    void setPortName(const QString& name);
    const QString& portName() const;

    void setPortMode(Mode mode);
    Mode portMode() const;

    bool isInput() const;
    bool isOutput() const;

    void setPortType(uint type);
    uint portType() const;

    void setPortTitle(const QString& title);
    const QString& portTitle() const;

    void setPortIndex(int index);
    int portIndex() const;

    std::shared_ptr<AbstractStreamPort> streamPort();

    QPointF portPos() const;

    // Connection-list methods.
    void appendConnect(FlowGraphEdge *connect);
    void removeConnect(FlowGraphEdge *connect);
    void removeConnects();

    FlowGraphEdge *findConnect(FlowGraphNodePort *port) const;

    // Selection propagation method...
    void setSelectedEx(bool is_selected);

    // Highlighting propagation method...
    void setHighlightEx(bool is_highlight);

    // Special port-type color business.
    void updatePortTypeColors(FlowGraphView *canvas);

    // Port hash/map key.
    class PortKey : public ItemKey
    {
    public:
        // Constructors.
        PortKey(FlowGraphNodePort *port)
            : ItemKey(port->portName(), port->portMode(), port->portType()) {}
    };

    // Port sorting type.
    enum SortType { PortName = 0, PortTitle, PortIndex };

    static void setSortType(SortType sort_type);
    static SortType sortType();

    // Port sorting order.
    enum SortOrder { Ascending = 0, Descending };

    static void setSortOrder(SortOrder sort_order);
    static SortOrder sortOrder();

    // Port sorting comparators.
    struct Compare {
        bool operator()(FlowGraphNodePort *port1, FlowGraphNodePort *port2) const
        { return FlowGraphNodePort::lessThan(port1, port2); }
    };

    struct ComparePos {
        bool operator()(FlowGraphNodePort *port1, FlowGraphNodePort *port2) const
        { return (port1->scenePos().y() < port2->scenePos().y()); }
    };

    // Rectangular editor extents.
    QRectF editorRect() const override;

protected:

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    // Natural decimal sorting comparators.
    static bool lessThan(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    static bool lessThan(const QString& s1, const QString& s2);

private:

    // instance variables.
    FlowGraphNode *m_node;

    QString m_name;
    Mode    m_mode;
    uint    m_type;

    QString m_title;
    int     m_index;

    QGraphicsTextItem *m_text;

    QList<FlowGraphEdge *> m_connects;

    int m_selectx;
    int m_hilitex;

    std::shared_ptr<AbstractStreamPort> m_streamPort;

    static SortType  g_sort_type;
    static SortOrder g_sort_order;
};


//----------------------------------------------------------------------------
// FlowGraphNode -- Node graphics item for a module.

class FlowGraphNode : public FlowGraphItem
{
public:

    // Constructor.
    FlowGraphNode(const QString& name, Mode mode, uint type = 0);

    // Destructor..
    ~FlowGraphNode() override;
    // Graphics item type.
    enum { Type = QGraphicsItem::UserType + 1 };

    int type() const override { return Type; }

    // Accessors.
    void setNodeName(const QString& name);
    const QString& nodeName() const;

    void setNodeMode(Mode mode);
    Mode nodeMode() const;

    void setNodeType(uint type);
    uint nodeType() const;

    void setNodeIcon(const QIcon& icon);
    const QIcon& nodeIcon() const;

    void setNodeTitle(const QString& title);
    QString nodeTitle() const;

    void updateNodeState(ModuleState state);

    void setNodeInfoText(const QString& info);
    QString nodeInfoText() const;

    // Port-list methods.
    FlowGraphNodePort *addPort(std::shared_ptr<AbstractStreamPort> port);

    void removePort(FlowGraphNodePort *port);
    void removePorts();

    // Port finder (by name, mode and type)
    FlowGraphNodePort *findPort(const QString& name, Mode mode, uint type = 0);

    // Reset port markings, destroy if unmarked.
    void resetMarkedPorts();

    // Path/shape updater.
    void updatePath();

    // Node hash key.
    class NodeKey : public ItemKey
    {
    public:
        // Constructors.
        NodeKey(FlowGraphNode *node)
            : ItemKey(node->nodeName(), node->nodeMode(), node->nodeType()) {}
    };

    // Rectangular editor extents.
    QRectF editorRect() const override;

protected:

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:

    // Instance variables.
    QString m_name;
    Mode    m_mode;
    uint    m_type;
    QColor m_shadowColor;

    QIcon   m_icon;

    QGraphicsPixmapItem *m_pixmap;
    QGraphicsPixmapItem *m_statusPix;
    QGraphicsTextItem   *m_titleText;
    QGraphicsTextItem   *m_statusText;
    QGraphicsTextItem   *m_infoText;

    FlowGraphNodePort::ItemKeys m_portkeys;
    QList<FlowGraphNodePort *>  m_ports;
};


//----------------------------------------------------------------------------
// FlowGraphEdge -- Connection-line graphics item.

class FlowGraphEdge : public FlowGraphItem
{
public:

    // Constructor.
    FlowGraphEdge();

    // Destructor..
    ~FlowGraphEdge() override;

    // Graphics item type.
    enum { Type = QGraphicsItem::UserType + 3 };

    int type() const override { return Type; }

    // Accessors.
    void setPort1(FlowGraphNodePort *port);
    FlowGraphNodePort *port1() const;

    void setPort2(FlowGraphNodePort *port);
    FlowGraphNodePort *port2() const;

    // Path/shaper updaters.
    void updatePathTo(const QPointF& pos);
    void updatePath();

    // Selection propagation method...
    void setSelectedEx(FlowGraphNodePort *port, bool is_selected);

    // Highlighting propagation method...
    void setHighlightEx(FlowGraphNodePort *port, bool is_highlight);

    // Special port-type color business.
    void updatePortTypeColors();

protected:

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:

    // Instance variables.
    FlowGraphNodePort *m_port1;
    FlowGraphNodePort *m_port2;
};


//----------------------------------------------------------------------------
// FlowGraphView -- Canvas graphics scene/view.

class FlowGraphView : public QGraphicsView
{
    Q_OBJECT

public:

    // Constructor.
    FlowGraphView(QWidget *parent = nullptr);

    // Destructor.
    ~FlowGraphView() override;

    // Accessors.
    QGraphicsScene *scene() const;

    void setSettings(QSettings *settings);
    QSettings *settings() const;

    // Canvas methods.
    void addItem(FlowGraphItem *item);
    void removeItem(FlowGraphItem *item);

    // Current item accessor.
    FlowGraphItem *currentItem() const;

    // Connection predicates.
    bool canConnect() const;
    bool canDisconnect() const;

    // Edit predicates.
    bool canRenameItem() const;

    // Zooming methods.
    void setZoom(qreal zoom);
    qreal zoom() const;

    void setZoomRange(bool zoomrange);
    bool isZoomRange() const;

    // Clean-up all un-marked nodes...
    void resetNodes(uint node_type);
    void clearNodes(uint node_type);

    // Special node finder.
    FlowGraphNode *findNode(
            const QString& name, FlowGraphItem::Mode mode, uint type = 0) const;

    // Graph canvas state methods.
    bool restoreState();
    bool saveState() const;

    // Graph colors management.
    void setPortTypeColor(uint port_type, const QColor& color);
    const QColor& portTypeColor(uint port_type);
    void updatePortTypeColors(uint port_type = 0);
    void clearPortTypeColors();

    // Clear all selection.
    void clearSelection();

    QList<FlowGraphNode *> selectedNodes() const;

signals:

    // Node factory notifications.
    void added(FlowGraphNode *node);
    void removed(FlowGraphNode *node);

    // Port (dis)connection notifications.
    void connected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void disconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);

    // Generic change notification.
    void changed();

    // Rename notification.
    void renamed(FlowGraphItem *item, const QString& name);

public slots:

    // Dis/connect selected items.
    void connectItems();
    void disconnectItems();

    // Select actions.
    void selectAll();
    void selectNone();
    void selectInvert();

    // Edit actions.
    void renameItem();

    // Discrete zooming actions.
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();

    // Update all nodes.
    void updateNodes();

protected slots:

    // Rename item slots.
    void textChanged(const QString&);
    void editingFinished();

protected:

    // Item finder (internal).
    FlowGraphItem *itemAt(const QPointF& pos) const;

    // Port (dis)connection commands.
    void connectPorts(
            FlowGraphNodePort *port1, FlowGraphNodePort *port2, bool is_connect);

    // Mouse event handlers.
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    // Keyboard event handler.
    void keyPressEvent(QKeyEvent *event) override;

    // Graph node key helper.
    QString nodeKey(FlowGraphNode *node) const;

    // Zoom in rectangle range.
    void zoomFitRange(const QRectF& range_rect);

    // Graph node position state methods.
    bool restoreNodePos(FlowGraphNode *node);
    bool saveNodePos(FlowGraphNode *node) const;

    // Renaming editor position and size updater.
    void updateEditorGeometry();

private:

    // Mouse pointer dragging states.
    enum DragState { DragNone = 0, DragStart, DragMove, DragScroll };

    // Instance variables.
    QGraphicsScene       *m_scene;
    DragState             m_state;
    QPointF               m_pos;
    FlowGraphItem    *m_item;
    FlowGraphEdge *m_connect;
    QRubberBand          *m_rubberband;
    qreal                 m_zoom;
    bool                  m_zoomrange;

    FlowGraphNode::ItemKeys m_nodekeys;
    QList<FlowGraphNode *>  m_nodes;

    QSettings  *m_settings;

    QList<QGraphicsItem *> m_selected;
    int m_selected_nodes;

    // Graph port colors.
    QHash<uint, QColor> m_port_colors;

    // Item renaming stuff.
    FlowGraphItem *m_edit_item;
    QLineEdit    *m_editor;
    int           m_edited;

    // Original node position (for move command).
    QPointF m_pos1;
};

#endif // FLOWGRAPHVIEW_H
