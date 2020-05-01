/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include <QWidget>
#include "miniscope.h"

class QSlider;

/**
 * @brief A simple widget to control Miniscope properties
 */
class MSControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MSControlWidget(const MScope::ControlDefinition &ctlDef, QWidget *parent = nullptr);

    QString controlId() const;

    double value() const;
    void setValue(double value);

signals:
    void valueChanged(const QString ctlId, double value);

private slots:
    void recvSliderValueChange(int value);

private:
    QString m_controlId;
    QSlider *m_slider;
};
