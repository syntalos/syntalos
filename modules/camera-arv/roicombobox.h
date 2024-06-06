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

#ifndef ROICOMBOBOX_H
#define ROICOMBOBOX_H

#include <QComboBox>

namespace QArv
{

class ROIcomboBox : public QComboBox
{
    Q_OBJECT

public:
    ROIcomboBox(QWidget *parent = 0);
    ~ROIcomboBox();

signals:
    void newSizeSelected(QSize size);

private slots:
    void itemSelected(int index);
    void customSizeEntered();

private:
    QValidator *ROIsizeValidator;
};

} // namespace QArv

#endif
