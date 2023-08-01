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

#pragma once

#include <QDialog>

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
namespace Ui
{
class PortEditorDialog;
}
namespace Syntalos
{
class AbstractModule;
}
#endif // DOXYGEN_SHOULD_SKIP_THIS

using namespace Syntalos;

class PortEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PortEditorDialog(AbstractModule *mod, QWidget *parent = nullptr);
    ~PortEditorDialog();

    void updatePortLists();

private slots:
    void on_tbAddInputPort_clicked();
    void on_tbAddOutputPort_clicked();
    void on_tbRemoveInputPort_clicked();
    void on_lwInputPorts_currentRowChanged(int currentRow);
    void on_lwOutputPorts_currentRowChanged(int currentRow);
    void on_tbRemoveOutputPort_clicked();

private:
    Ui::PortEditorDialog *ui;
    AbstractModule *m_mod;
};
