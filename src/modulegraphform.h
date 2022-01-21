/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MODULEGRAPHFORM_H
#define MODULEGRAPHFORM_H

#include <QWidget>
#include "flowgraphview.h"

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
namespace Syntalos {
class Engine;
class ModuleInfo;
class AbstractModule;
class VarStreamInputPort;
class StreamOutputPort;

Q_DECLARE_LOGGING_CATEGORY(logGraphUi)
}
#endif // DOXYGEN_SHOULD_SKIP_THIS

namespace Ui {
class ModuleGraphForm;
}

namespace Syntalos {
Q_DECLARE_LOGGING_CATEGORY(logGraphUi)
}

using namespace Syntalos;

class ModuleGraphForm : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleGraphForm(QWidget *parent = nullptr);
    ~ModuleGraphForm();

    void updateIconStyles();

    FlowGraphView *graphView() const;
    Engine *engine() const;

    bool modifyPossible() const;
    void setModifyPossible(bool allowModify);

    FlowGraphEdge *updateConnectionHeat(const VarStreamInputPort *inPort, const StreamOutputPort *outPort,
                                        ConnectionHeatLevel hlevel);

private slots:
    void on_actionAddModule_triggered();
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionSettings_triggered();
    void on_actionDisplay_triggered();
    void on_actionRemove_triggered();
    void on_selectionChanged();
    void on_graphPortsConnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void on_graphPortsDisconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void on_modulePreRemove(AbstractModule *mod);
    void on_portsConnected(const VarStreamInputPort *inPort, const StreamOutputPort *outPort);
    void on_modulePortConfigChanged();

    void moduleAdded(ModuleInfo *info, AbstractModule *mod);
    void receiveStateChange(ModuleState state);
    void receiveErrorMessage(const QString &message);
    void receiveMessage(const QString &message);
    void itemRenamed(FlowGraphItem *item, const QString &name);

signals:
    void busyStart();
    void busyEnd();

private:
    Ui::ModuleGraphForm *ui;

    Engine *m_engine;
    bool m_modifyPossible;
    bool m_shutdown;
    QHash<AbstractModule*, FlowGraphNode*> m_modNodeMap;
    QMenu *m_menu;

    QHash<QString, QPair<FlowGraphNode*, QString>> m_connMemory;

    FlowGraphNode *selectedSingleNode() const;
};

#endif // MODULEGRAPHFORM_H
