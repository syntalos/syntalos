/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QARVCAMERADELEGATE_H
#define QARVCAMERADELEGATE_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include <QStyledItemDelegate>

#pragma GCC visibility push(default)

//! QArvCameraDelegate provides editing widgets to go with the QArvCamera model.
/*!
 * Once a view is created for the data model provided by QArvCamera, use this
 * delegate to provide editing widgets for the view.
 */
class QArvCameraDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit QArvCameraDelegate(QObject* parent = 0);
    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor,
                      QAbstractItemModel* model,
                      const QModelIndex & index) const override;
    void updateEditorGeometry(QWidget* editor,
                              const QStyleOptionViewItem& option,
                              const QModelIndex & index) const override;
    QSize sizeHint(const QStyleOptionViewItem & option,
                   const QModelIndex & index) const override;

private slots:
    void finishEditing();
};

#pragma GCC visibility pop

#endif
