/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef MODULEGRAPHFORM_H
#define MODULEGRAPHFORM_H

#include <QWidget>
#include "flowgraphview.h"

class ModuleManager;
class ModuleInfo;
class AbstractModule;

namespace Ui {
class ModuleGraphForm;
}

class ModuleGraphForm : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleGraphForm(QWidget *parent = nullptr);
    ~ModuleGraphForm();

    FlowGraphView *graphView() const;
    ModuleManager *moduleManager() const;
    void setModuleManager(ModuleManager *modManager);

    bool modifyPossible() const;
    void setModifyPossible(bool allowModify);

private slots:
    void on_actionAddModule_triggered();
    void on_actionConnect_triggered();
    void on_actionDisconnect_triggered();
    void on_actionSettings_triggered();
    void on_actionDisplay_triggered();
    void on_actionRemove_triggered();
    void on_selectionChanged();
    void on_portsConnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void on_portsDisconnected(FlowGraphNodePort *port1, FlowGraphNodePort *port2);
    void on_modulePreRemove(AbstractModule *mod);

    void moduleAdded(ModuleInfo *info, AbstractModule *mod);
    void receiveStateChange(ModuleState state);
    void receiveErrorMessage(const QString &message);
    void receiveMessage(const QString &message);
    void itemRenamed(FlowGraphItem *item, const QString &name);

private:
    Ui::ModuleGraphForm *ui;

    ModuleManager *m_modManager;
    bool m_modifyPossible;
    bool m_shutdown;
    QHash<AbstractModule*, FlowGraphNode*> m_modNodeMap;
    QMenu *m_menu;

    FlowGraphNode *selectedSingleNode() const;
};

#endif // MODULEGRAPHFORM_H
