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

#include "qarvtype.h"
#include "qarv-globals.h"
#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

QArvEditor::QArvEditor(QWidget* parent) : QWidget(parent) {
    setAutoFillBackground(true);
}

QArvEnumeration::QArvEnumeration() : values(), isAvailable() {}

QArvEnumeration::operator QString() const {
    return currentValue >= 0 ? names[currentValue] : QString();
}

static void squeezeLeft(QLayout* layout) {
    layout->addItem(new QSpacerItem(1, 1,
                                    QSizePolicy::Expanding,
                                    QSizePolicy::Expanding));
}

QArvEditor* QArvEnumeration::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto select = new QComboBox(editor);
    select->setObjectName("selectEnum");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(select);
    squeezeLeft(layout);
    editor->connect(select, SIGNAL(activated(int)), SLOT(editingComplete()));
    return editor;
}

void QArvEnumeration::populateEditor(QWidget* editor) const {
    auto select = editor->findChild<QComboBox*>("selectEnum");
    if (!select) {
        qDebug().noquote() << "Error populating editor: QArvEnumeration.";
        return;
    }
    select->clear();
    int choose = 0;
    for (int i = 0; i < names.size(); i++) {
        if (isAvailable.at(i)) {
            select->addItem(names.at(i), QVariant::fromValue(values.at(i)));
            if (i < currentValue) choose++;
        }
    }
    select->setCurrentIndex(choose);
}

void QArvEnumeration::readFromEditor(QWidget* editor) {
    auto select = editor->findChild<QComboBox*>("selectEnum");
    if (!select) {
        qDebug().noquote() << "Error reading from editor: QArvEnumeration.";
        return;
    }
    auto val = select->itemData(select->currentIndex());
    currentValue = values.indexOf(val.toString());
}

QArvString::QArvString() : value() {}

QArvString::operator QString() const {
    return value;
}

QArvEditor* QArvString::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto edline = new QLineEdit(editor);
    edline->setObjectName("editString");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(edline);
    squeezeLeft(layout);
    editor->connect(edline, SIGNAL(editingFinished()), SLOT(editingComplete()));
    return editor;
}

void QArvString::populateEditor(QWidget* editor) const {
    auto edline = editor->findChild<QLineEdit*>("editString");
    if (!edline) {
        qDebug().noquote() << "Error populating editor: QArvString.";
        return;
    }
    edline->setMaxLength(maxlength);
    edline->setText(value);
}

void QArvString::readFromEditor(QWidget* editor) {
    auto edline = editor->findChild<QLineEdit*>("editString");
    if (!edline) {
        qDebug().noquote() << "Error reading from editor: QArvString.";
        return;
    }
    value = edline->text();
}

QArvFloat::QArvFloat() : unit() {}

QArvFloat::operator QString() const {
    return QString::number(value) + " " + unit;
}

QArvEditor* QArvFloat::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto edbox = new QDoubleSpinBox(editor);
    edbox->setObjectName("editFloat");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(edbox);
    squeezeLeft(layout);
    editor->connect(edbox, SIGNAL(editingFinished()), SLOT(editingComplete()));
    return editor;
}

void QArvFloat::populateEditor(QWidget* editor) const {
    auto edbox = editor->findChild<QDoubleSpinBox*>("editFloat");
    if (!edbox) {
        qDebug().noquote() << "Error populating editor: QArvFloat.";
        return;
    }
    edbox->setMaximum(max);
    edbox->setMinimum(min);
    edbox->setValue(value);
    edbox->setSuffix(QString(" ") + unit);
}

void QArvFloat::readFromEditor(QWidget* editor) {
    auto edbox = editor->findChild<QDoubleSpinBox*>("editFloat");
    if (!edbox) {
        qDebug().noquote() << "Error reading from editor: QArvFloat.";
        return;
    }
    value = edbox->value();
}

QArvInteger::operator QString() const {
    return QString::number(value);
}

QArvEditor* QArvInteger::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto edbox = new QSpinBox(editor);
    edbox->setObjectName("editInteger");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(edbox);
    squeezeLeft(layout);
    editor->connect(edbox, SIGNAL(editingFinished()), SLOT(editingComplete()));
    return editor;
}

void QArvInteger::populateEditor(QWidget* editor) const {
    auto edbox = editor->findChild<QSpinBox*>("editInteger");
    if (!edbox) {
        qDebug().noquote() << "Error populating editor: QArvInteger.";
        return;
    }
    edbox->setMaximum(max < INT_MAX ? max : INT_MAX);
    edbox->setMinimum(min > INT_MIN ? min : INT_MIN);
    edbox->setValue(value);
}

void QArvInteger::readFromEditor(QWidget* editor) {
    auto edbox = editor->findChild<QSpinBox*>("editInteger");
    if (!edbox) {
        qDebug().noquote() << "Error reading from editor: QArvInteger.";
        return;
    }
    value = edbox->value();
}

QArvBoolean::operator QString() const {
    return value
           ? QObject::tr("on/true", "QArvCamera")
           : QObject::tr("off/false", "QArvCamera");
}

QArvEditor* QArvBoolean::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto check = new QCheckBox(editor);
    check->setObjectName("editBool");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(check);
    squeezeLeft(layout);
    editor->connect(check, SIGNAL(clicked(bool)), SLOT(editingComplete()));
    return editor;
}

void QArvBoolean::populateEditor(QWidget* editor) const {
    auto check = editor->findChild<QCheckBox*>("editBool");
    if (!check) {
        qDebug().noquote() << "Error populating editor: QArvBoolean.";
        return;
    }
    check->setChecked(value);
}

void QArvBoolean::readFromEditor(QWidget* editor) {
    auto check = editor->findChild<QCheckBox*>("editBool");
    if (!check) {
        qDebug().noquote() << "Error reading from editor: QArvBoolean.";
        return;
    }
    value = check->isChecked();
}

QArvCommand::operator QString() const {
    return QObject::tr("<command>", "QArvCamera");
}

QArvEditor* QArvCommand::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto button = new QPushButton(editor);
    button->setObjectName("execCommand");
    button->setText(QObject::tr("Execute", "QArvCamera"));
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(button);
    squeezeLeft(layout);
    editor->connect(button, SIGNAL(clicked(bool)), SLOT(editingComplete()));
    return editor;
}

void QArvCommand::populateEditor(QWidget* editor) const {}

void QArvCommand::readFromEditor(QWidget* editor) {}

QArvRegister::QArvRegister() : value() {}

QArvRegister::operator QString() const {
    return QString("0x") + value.toHex();
}

QArvEditor* QArvRegister::createEditor(QWidget* parent) const {
    auto editor = new QArvEditor(parent);
    auto edline = new QLineEdit(editor);
    edline->setObjectName("editRegister");
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    editor->setLayout(layout);
    layout->addWidget(edline);
    squeezeLeft(layout);
    editor->connect(edline, SIGNAL(editingFinished()), SLOT(editingComplete()));
    return editor;
}

void QArvRegister::populateEditor(QWidget* editor) const {
    auto edline = editor->findChild<QLineEdit*>("editRegister");
    if (!edline) {
        qDebug().noquote() << "Error populating editor: QArvRegister.";
        return;
    }
    auto hexval = value.toHex();
    QString imask("");
    for (int i = 0; i < hexval.length(); i++) imask += "H";
    edline->setInputMask(imask);
    edline->setText(hexval);
}

void QArvRegister::readFromEditor(QWidget* editor) {
    auto edline = editor->findChild<QLineEdit*>("editRegister");
    if (!edline) {
        qDebug().noquote() << "Error reading from editor: QArvRegister.";
        return;
    }
    value = QByteArray::fromHex(edline->text().toLatin1());
}
