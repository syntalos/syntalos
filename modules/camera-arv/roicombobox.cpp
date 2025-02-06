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

#include "qarv/qarv-globals.h"
#include <QLineEdit>

#include "roicombobox.h"

using namespace QArv;

ROIcomboBox::ROIcomboBox(QWidget *parent)
    : QComboBox(parent)
{
    this->addItem(tr("No size constraint"), QVariant(QSize(0, 0)));
    this->addItem("1024x768", QVariant(QSize(1024, 768)));
    this->addItem("800x600", QVariant(QSize(800, 600)));
    this->addItem("640x480", QVariant(QSize(640, 480)));
    this->addItem("480x360", QVariant(QSize(480, 360)));
    this->addItem("320x240", QVariant(QSize(320, 240)));
    this->addItem(tr("Custom"), QVariant(QSize(-1, -1)));

    QRegularExpression ROIregExp("[1-9][0-9]*x[1-9][0-9]*");
    ROIsizeValidator = new QRegularExpressionValidator(ROIregExp, this);

    this->connect(this, SIGNAL(currentIndexChanged(int)), SLOT(itemSelected(int)));
}

ROIcomboBox::~ROIcomboBox() {}

void ROIcomboBox::itemSelected(int index)
{
    if (index < 0)
        index = 0;

    QSize s = this->itemData(index).toSize();

    if (s.width() == -1) {
        this->setEditable(true);
        this->clearEditText();

        // Line editor is reset when we call setEditable(false) so the following
        // must be repeated everytime we enable editing.
        this->setValidator(ROIsizeValidator);
        this->connect(this->lineEdit(), SIGNAL(editingFinished()), SLOT(customSizeEntered()));
    } else {
        this->setEditable(false);
        emit newSizeSelected(s);
    }
}

void ROIcomboBox::customSizeEntered()
{
    static QRegularExpression ROIparse("^([0-9]+)x([0-9]+)$");
    const auto match = ROIparse.match(this->lineEdit()->text());
    int width = match.captured(1).toInt();
    int height = match.captured(2).toInt();

    clearFocus();
    emit newSizeSelected(QSize(width, height));
}
