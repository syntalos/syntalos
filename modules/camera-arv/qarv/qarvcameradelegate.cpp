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

#include "qarv/qarvcameradelegate.h"
#include "qarv/qarvtype.h"
#include "qarv-globals.h"
#include <QLayout>

QArvCameraDelegate::QArvCameraDelegate(const QString &modId, QObject* parent) :
    QStyledItemDelegate(parent), m_modId(modId) {}

QWidget* QArvCameraDelegate::createEditor(QWidget* parent,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
    auto var = index.model()->data(index, Qt::EditRole);
    if (!var.isValid())
        return NULL;
    auto val = static_cast<QArvType*>(var.data());
    auto editor = val->createEditor(parent);
    this->connect(editor, SIGNAL(editingFinished()), SLOT(finishEditing()));
    return editor;
}

void QArvCameraDelegate::setEditorData(QWidget* editor,
                                       const QModelIndex& index) const {
    auto var = index.model()->data(index, Qt::EditRole);
    if (!var.isValid()) {
        logMessage() << "Error setting editor data: QArvCameraDelegate";
        return;
    }
    auto val = static_cast<QArvType*>(var.data());
    val->populateEditor(editor);
}

void QArvCameraDelegate::setModelData(QWidget* editor,
                                      QAbstractItemModel* model,
                                      const QModelIndex& index) const {
    auto var = model->data(index, Qt::EditRole);
    if (!var.isValid()) {
        logMessage() << "Error setting model data: QArvCameraDelegate";
        return;
    }
    auto val = static_cast<QArvType*>(var.data());
    val->readFromEditor(editor);
    model->setData(index, var);
}

void QArvCameraDelegate::updateEditorGeometry(QWidget* editor,
                                              const QStyleOptionViewItem& opt,
                                              const QModelIndex& index) const {
    editor->setGeometry(opt.rect);
}

void QArvCameraDelegate::finishEditing() {
    auto editor = qobject_cast<QWidget*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

QSize QArvCameraDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
    if (!index.isValid())
        return QStyledItemDelegate::sizeHint(option, index);
    QScopedPointer<QWidget> ptr(createEditor(NULL, option, index));
    if (ptr) {
        return ptr->layout()->sizeHint();
    } else {
        return QStyledItemDelegate::sizeHint(option, index);
    }
}
