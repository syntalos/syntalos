/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "mscontrolwidget.h"

#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

MSControlWidget::MSControlWidget(const MScope::ControlDefinition &ctlDef, QWidget *parent)
    : QWidget(parent)
{
    m_controlId = ctlDef.id;

    const auto layout = new QVBoxLayout(this);
    layout->setMargin(2);
    layout->setSpacing(2);

    auto lblTitle = new QLabel(this);
    lblTitle->setText(ctlDef.name);
    layout->addWidget(lblTitle);

    if (ctlDef.kind == MScope::ControlKind::Selector) {
        const auto sc = new QWidget(this);
        const auto selLayout = new QGridLayout(sc);
        const auto valuesCount = ctlDef.labels.length();
        m_slider = new QSlider(Qt::Horizontal, sc);
        selLayout->setMargin(0);
        selLayout->setSpacing(2);

        m_slider->setRange(ctlDef.valueMin, ctlDef.valueMax);
        m_slider->setSingleStep(1);
        m_slider->setValue(ctlDef.valueStart);
        selLayout->addWidget(m_slider, 0, 0, 1, valuesCount);

        for (int i = 0; i < valuesCount; ++i) {
            const auto lbl = new QLabel(QStringLiteral("<html><i>%1</i>").arg(ctlDef.labels[i]), sc);
            if (i == 0)
                lbl->setAlignment(Qt::AlignLeft);
            else if (i == valuesCount - 1)
                lbl->setAlignment(Qt::AlignRight);
            else
                lbl->setAlignment(Qt::AlignCenter);
            selLayout->addWidget(lbl, 1, i, 1, 1);
        }

        sc->setLayout(selLayout);
        layout->addWidget(sc);
    } else {
        const auto slw = new QWidget(this);
        auto slLayout = new QHBoxLayout(slw);
        slLayout->setMargin(0);
        slLayout->setSpacing(2);

        m_slider = new QSlider(Qt::Horizontal, slw);
        // just assume slider here, for now
        m_slider->setRange(ctlDef.valueMin, ctlDef.valueMax);
        m_slider->setValue(ctlDef.valueStart);
        m_slider->setSingleStep(ctlDef.stepSize);
        slLayout->addWidget(m_slider);

        auto sb = new QSpinBox(slw);
        sb->setRange(ctlDef.valueMin, ctlDef.valueMax);
        sb->setValue(ctlDef.valueStart);
        sb->setSingleStep(ctlDef.stepSize);
        sb->setMinimumWidth(64);
        slLayout->addWidget(sb);

        connect(sb, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), m_slider, &QSlider::setValue);
        connect(m_slider, &QSlider::valueChanged, sb, &QSpinBox::setValue);

        slLayout->setStretchFactor(sb, 1);
        slLayout->setStretchFactor(m_slider, 4);
        slw->setLayout(slLayout);
        layout->addWidget(slw);
    }

    connect(m_slider, &QSlider::valueChanged, this, &MSControlWidget::recvSliderValueChange);
    setLayout(layout);
}

QString MSControlWidget::controlId() const
{
    return m_controlId;
}

double MSControlWidget::value() const
{
    return m_slider->value();
}

void MSControlWidget::setValue(double value)
{
    m_slider->setValue(value);
}

void MSControlWidget::recvSliderValueChange(int value)
{
    Q_EMIT valueChanged(m_controlId, value);
}
