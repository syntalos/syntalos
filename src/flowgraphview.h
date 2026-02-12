/*
 * Copyright (C) 2019-2025 Matthias Klumpp <matthias@tenstral.net>
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

#include <QHash>
#include <QList>

#include "fabric/moduleapi.h"

class FlowGraphView;
class FlowGraphNode;
class FlowGraphNodePort;
class FlowGraphEdge;

class QStyleOptionGraphicsItem;
class QRubberBand;

class QGraphicsProxyWidget;
class QLineEdit;

class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
namespace Syntalos
{
class AbstractModule;
class AbstractStreamPort;
} // namespace Syntalos
#endif // DOXYGEN_SHOULD_SKIP_THIS

using namespace Syntalos;

/**
 * @brief The FlowGraphItem class
 *
 * Base graphics item.
 */
class FlowGraphItem : public QGraphicsPathItem
{
public:
    FlowGraphItem(QGraphicsItem *parent = nullptr);

    void setForeground(const QColor &color);
    const QColor &foreground() const;

    void setBackground(const QColor &color);
    const QColor &background() const;

    void setMarked(bool marked);
    bool isMarked() const;

    void setHighlight(bool hilite);
    bool isHighlight() const;

    enum Mode {
        None = 0,
        Input = 1,
        Output = 2,
        Duplex = Input | Output
    };

    // Item hash/map key.
    class ItemKey
    {
    public:
        ItemKey(const QString &name, Mode mode, uint type = 0)
            : m_name(name),
              m_mode(mode),
              m_type(type)
        {
        }
        ItemKey(const ItemKey &key)
            : m_name(key.name()),
              m_mode(key.mode()),
              m_type(key.type())
        {
        }

        const QString &name() const
        {
            return m_name;
        }
        Mode mode() const
        {
            return m_mode;
        }
        uint type() const
        {
            return m_type;
        }

        // Hash/map key comparators.
        bool operator==(const ItemKey &key) const
        {
            return ItemKey::type() == key.type() && ItemKey::mode() == key.mode() && ItemKey::name() == key.name();
        }

    private:
        QString m_name;
        Mode m_mode;
        uint m_type;
    };

    typedef QHash<ItemKey, FlowGraphItem *> ItemKeys;

    // Item-type hash (static)
    static uint itemType(const QByteArray &type_name);

    // Rectangular editor extents.
    virtual QRectF editorRect() const;

private:
    QColor m_foreground;
    QColor m_background;

    bool m_marked;
    bool m_hilite;
};

/**
 * @brief Flow graph item hash function
 */
inline uint qHash(const FlowGraphItem::ItemKey &key)
{
    return qHash(key.name()) ^ qHash(uint(key.mode())) ^ qHash(key.type());
}

/**
 * @brief The FlowGraphNodePort class
 *
 * A port graphics item.
 */
class FlowGraphNodePort : public FlowGraphItem
{
public:
    FlowGraphNodePort(FlowGraphNode *node);
    FlowGraphNodePort(FlowGraphNode *node, std::shared_ptr<AbstractStreamPort> port);

    ~FlowGraphNodePort() override;

    enum {
        Type = QGraphicsItem::UserType + 2
    };

    int type() const override
    {
        return Type;
    }

    FlowGraphNode *portNode() const;

    const QString &portId() const;

    void setPortMode(Mode mode);
    Mode portMode() const;

    bool isInput() const;
    bool isOutput() const;

    uint portType() const;

    void setPortTitle(const QString &title);
    const QString &portTitle() const;

    void setPortIndex(int index);
    int portIndex() const;

    std::shared_ptr<AbstractStreamPort> streamPort();

    QPointF portPos() const;

    bool appendConnect(FlowGraphEdge *connect);
    void removeConnect(FlowGraphEdge *connect);
    void removeConnects();
    QList<FlowGraphEdge *> connects() const;

    FlowGraphEdge *findConnect(FlowGraphNodePort *port) const;

    void setSelectedEx(bool is_selected);
    void setHighlightEx(bool is_highlight);

    void updatePortTypeColors(FlowGraphView *canvas);

    // Port hash/map key.
    class PortKey : public ItemKey
    {
    public:
        // Constructors.
        PortKey(FlowGraphNodePort *port)
            : ItemKey(port->portId(), port->portMode(), port->portType())
        {
        }
    };

    // Port sorting type.
    enum SortType {
        PortName = 0,
        PortTitle,
        PortIndex
    };

    static void setSortType(SortType sort_type);
    static SortType sortType();

    // Port sorting order.
    enum SortOrder {
        Ascending = 0,
        Descending
    };

    static void setSortOrder(SortOrder sort_order);
    static SortOrder sortOrder();

    // Port sorting comparators.
    struct Compare {
        bool operator()(FlowGraphNodePort *port1, FlowGraphNodePort *port2) const
        {
            return FlowGraphNodePort::lessThan(port1, port2);
        }
    };

    struct ComparePos {
        bool operator()(FlowGraphNodePort *port1, FlowGraphNodePort *port2) const
        {
            return (port1->scenePos().y() < port2->scenePos().y());
        }
    };

    QRectF editorRect() const override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    // Natural decimal sorting comparators.
    static bool lessThan(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    static bool lessThan(const QString &s1, const QString &s2);

private:
    // instance variables.
    FlowGraphNode *m_node;

    QString m_id;
    Mode m_mode;

    QString m_title;
    int m_index;

    QGraphicsTextItem *m_text;

    QList<FlowGraphEdge *> m_connects;

    int m_selectx;
    int m_hilitex;

    std::shared_ptr<AbstractStreamPort> m_streamPort;

    static SortType g_sort_type;
    static SortOrder g_sort_order;
};

/**
 * @brief The FlowGraphNode class
 *
 * Node graphics item for a module.
 */
class FlowGraphNode : public FlowGraphItem
{
public:
    FlowGraphNode(AbstractModule *module, uint type = 0);
    ~FlowGraphNode() override;

    enum {
        Type = QGraphicsItem::UserType + 1
    };

    int type() const override
    {
        return Type;
    }

    void setNodeName(const QString &name);
    const QString nodeName() const;

    void setNodeType(uint type);
    uint nodeType() const;

    void setNodeIcon(const QIcon &icon);
    const QIcon &nodeIcon() const;

    void setNodeTitle(const QString &title);
    QString nodeTitle() const;

    AbstractModule *module() const;

    void setShadowColor(const QColor &color);

    void updateNodeState(ModuleState state);

    void setNodeInfoText(const QString &info);
    QString nodeInfoText() const;

    void setStopOnErrorAttribute(bool moduleFailureCritical);
    void setDisabledAttribute(bool disabled);

    FlowGraphNodePort *addPort(std::shared_ptr<AbstractStreamPort> port);
    QList<FlowGraphNodePort *> ports() const;
    void removePort(FlowGraphNodePort *port);
    void removePorts();
    FlowGraphNodePort *findPort(const QString &name, Mode mode, uint type = 0);
    void resetMarkedPorts();

    void updatePath();

    // Node hash key.
    class NodeKey : public ItemKey
    {
    public:
        NodeKey(FlowGraphNode *node)
            : ItemKey(node->nodeName(), FlowGraphItem::Duplex, node->nodeType())
        {
        }
    };

    QRectF editorRect() const override;

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    AbstractModule *m_module;
    uint m_type;
    QColor m_shadowColor;

    QIcon m_icon;

    QGraphicsPixmapItem *m_pixmap;
    QGraphicsPixmapItem *m_statusPix;
    QGraphicsPixmapItem *m_canFailPix;
    QGraphicsTextItem *m_titleText;
    QGraphicsTextItem *m_statusText;
    QGraphicsTextItem *m_infoText;
    bool m_hasDisabledMark;

    FlowGraphNodePort::ItemKeys m_portkeys;
    QList<FlowGraphNodePort *> m_ports;
};

/**
 * @brief The FlowGraphEdge class
 *
 * Connection-line graphics item.
 */
class FlowGraphEdge : public FlowGraphItem
{
public:
    FlowGraphEdge();
    ~FlowGraphEdge() override;

    enum {
        Type = QGraphicsItem::UserType + 3
    };
    int type() const override
    {
        return Type;
    }

    bool setPort1(FlowGraphNodePort *port);
    FlowGraphNodePort *port1() const;

    bool setPort2(FlowGraphNodePort *port);
    FlowGraphNodePort *port2() const;

    void updatePathTo(const QPointF &pos);
    void updatePath();

    void setSelectedEx(FlowGraphNodePort *port, bool is_selected);
    void setHighlightEx(FlowGraphNodePort *port, bool is_highlight);

    void updatePortTypeColors();

    void setHeatLevel(ConnectionHeatLevel hlevel);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    FlowGraphNodePort *m_port1;
    FlowGraphNodePort *m_port2;
};

/**
 * @brief The FlowGraphView class
 *
 * Canvas graphics scene/view.
 */
class FlowGraphView : public QGraphicsView
{
    Q_OBJECT

public:
    FlowGraphView(QWidget *parent = nullptr);
    ~FlowGraphView() override;

    QGraphicsScene *scene() const;

    bool allowEdit() const;
    void setAllowEdit(bool allowed);

    void addItem(FlowGraphItem *item);
    void removeItem(FlowGraphItem *item);

    FlowGraphItem *currentItem() const;

    bool canConnect() const;
    bool canDisconnect() const;

    bool canRenameItem() const;

    void setZoom(qreal zoom);
    qreal zoom() const;

    void setZoomRange(bool zoomrange);
    bool isZoomRange() const;

    void resetNodes(uint node_type);
    void clearNodes(uint node_type);

    FlowGraphNode *findNode(const QString &name, FlowGraphItem::Mode mode, uint type = 0) const;

    bool saveState();
    bool restoreState();

    QVariantHash settings() const;
    void setSettings(const QVariantHash &settings);

    void setPortTypeColor(uint port_type, const QColor &color);
    const QColor &portTypeColor(uint port_type);
    void updatePortTypeColors(uint port_type = 0);
    void clearPortTypeColors();

    void clearSelection();

    QList<FlowGraphNode *> selectedNodes() const;

signals:
    void added(FlowGraphNode *node);
    void removed(FlowGraphNode *node);

    void connected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void disconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);

    void changed();

    void renamed(FlowGraphItem *item, const QString &name);

public slots:
    void connectItems(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void connectItems();

    void disconnectItems(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void disconnectItems();

    void selectAll();
    void selectNone();
    void selectInvert();

    void renameItem(FlowGraphItem *item = nullptr);

    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomReset();

    void updateNodes();

protected slots:
    void textChanged(const QString &);
    void editingFinished();

protected:
    FlowGraphItem *itemAt(const QPointF &pos) const;

    void connectPorts(FlowGraphNodePort *port1, FlowGraphNodePort *port2);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    QString nodeKey(FlowGraphNode *node) const;

    void zoomFitRange(const QRectF &range_rect);

    bool restoreNodePos(FlowGraphNode *node);
    bool saveNodePos(FlowGraphNode *node);

    void updateEditorGeometry();

private:
    void adjustSceneRect();

    QPointF findOptimalNodePosition(FlowGraphNode *node);
    bool isPositionOccupied(const QPointF &pos, const QSizeF &size, FlowGraphNode *excludeNode = nullptr);

    enum DragState {
        DragNone = 0,
        DragStart,
        DragMove,
        DragScroll
    };

    QGraphicsScene *m_scene;
    DragState m_state;
    QPointF m_pos;
    FlowGraphItem *m_item;
    FlowGraphEdge *m_connect;
    QRubberBand *m_rubberband;
    qreal m_zoom;
    bool m_zoomrange;

    FlowGraphNode::ItemKeys m_nodekeys;
    QList<FlowGraphNode *> m_nodes;

    QList<QGraphicsItem *> m_selected;
    int m_selected_nodes;

    QHash<uint, QColor> m_port_colors;

    bool m_allowEdit;
    FlowGraphItem *m_edit_item;
    QLineEdit *m_editor;
    int m_edited;

    QVariantHash m_settings;
};

#endif // FLOWGRAPHVIEW_H
