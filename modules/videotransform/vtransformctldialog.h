/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include "vtransformlistmodel.h"
#include <QDialog>

namespace Ui
{
class VTransformCtlDialog;
}

class VTransformCtlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VTransformCtlDialog(QWidget *parent = nullptr);
    ~VTransformCtlDialog();

    void setRunning(bool running);
    void updateUi();
    void resetSettingsPanel();

    QList<std::shared_ptr<VideoTransform>> transformList();
    QVariantHash serializeSettings() const;
    void loadSettings(const QVariantHash &settings);

private slots:
    void on_btnAdd_clicked();
    void on_btnRemove_clicked();
    void on_btnMoveUp_clicked();
    void on_btnMoveDown_clicked();

    void on_activeTFListView_activated(const QModelIndex &index);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void transformListViewSelectionChanged(const QModelIndex &index);

private:
    Ui::VTransformCtlDialog *ui;

    VTransformListModel *m_vtfListModel;
    QWidget *m_curSettingsPanel;
    bool m_running;
};
