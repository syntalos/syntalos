/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "elidedlabel.h"

#include <QTextLayout>

ElidedLabel::ElidedLabel(QWidget *parent)
    : ElidedLabel(QString(), parent)
{
}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent)
    : QLabel(parent),
      m_elideMode(Qt::ElideMiddle)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setText(text);
}

void ElidedLabel::setText(const QString &newText)
{
    m_rawText = newText;
    m_realMinWidth = minimumWidth();
    updateElision();
}

void ElidedLabel::resizeEvent(QResizeEvent *)
{
    updateElision();
}

void ElidedLabel::updateElision()
{
    QFontMetrics metrics(font());
    QString elidedText = metrics.elidedText(m_rawText, m_elideMode, width());
    QLabel::setText(elidedText);
    if (!elidedText.isEmpty())
        setMinimumWidth((m_realMinWidth == 0) ? 1 : m_realMinWidth);
}
