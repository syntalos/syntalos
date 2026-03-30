/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QPair>

class QHBoxLayout;
class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QFocusEvent;
class QMouseEvent;
class QPaintEvent;

namespace Syntalos
{

class ChipOrderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChipOrderWidget(QWidget *parent = nullptr);

    void setChips(const QList<QPair<QString, QString>> &chips);
    QStringList chipIds() const;
    void clearChipSelection();

signals:
    void orderChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool event(QEvent *event) override;

private:
    class Chip;

    QList<Chip *> m_chips;
    QHBoxLayout *m_layout;
    Chip *m_selectedChip;
    int m_dropIndicatorIndex;

    void setSelectedChip(Chip *chip);
    void setDropIndicatorIndex(int index);
    int dropIndicatorX() const;
    int insertionIndexAtX(int x) const;
    void reorderChip(int fromIndex, int toIndex);
    void beginDragForChip(Chip *chip);
    void rebuildLayout();
    void updateCompactSize();
};

} // namespace Syntalos
