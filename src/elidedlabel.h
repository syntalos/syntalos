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

#ifndef ELIDEDLABEL_H
#define ELIDEDLABEL_H

#include <QWidget>
#include <QLabel>

class ElidedLabel : public QLabel
{
    Q_OBJECT
#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode)
#endif

public:
    explicit ElidedLabel(QWidget *parent = nullptr);
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);

    void setText(const QString &text);
    const QString & text() const { return m_rawText; }

    Qt::TextElideMode elideMode() { return m_elideMode; }
    void setElideMode(Qt::TextElideMode mode) { m_elideMode = mode; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateElision();
    QString m_rawText;
    int m_realMinWidth;
    Qt::TextElideMode m_elideMode;
};

#endif // ELIDEDLABEL_H
