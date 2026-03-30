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

#include "chiporderwidget.h"

#include <QApplication>
#include <QByteArray>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QStyle>

namespace Syntalos
{
static constexpr auto DRAG_MIME_FORMAT = "application/x-syntalos-chip-index";

class ChipOrderWidget::Chip : public QFrame
{
public:
    QString id;
    QString title;

    Chip(const QString &chipId, const QString &chipTitle, ChipOrderWidget *owner)
        : QFrame(owner),
          id(chipId),
          title(chipTitle),
          m_owner(owner)
    {
        setFrameStyle(QFrame::NoFrame);
        setCursor(Qt::OpenHandCursor);
        setProperty("chipSelected", false);
        setObjectName("chipFrame");
        setStyleSheet(
            "QFrame#chipFrame {"
            "  border: 1px solid palette(mid);"
            "  border-radius: 6px;"
            "  background-color: palette(base);"
            "}"
            "QFrame#chipFrame[chipSelected=\"true\"] {"
            "  border-color: palette(highlight);"
            "  background-color: palette(highlight);"
            "}"
            "QFrame#chipFrame QLabel {"
            "  border: 0px;"
            "  background-color: transparent;"
            "  color: palette(text);"
            "}"
            "QFrame#chipFrame[chipSelected=\"true\"] QLabel {"
            "  color: palette(highlighted-text);"
            "}");

        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(8, 3, 8, 3);
        auto label = new QLabel(title, this);
        label->setFrameStyle(QFrame::NoFrame);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        setSelected(false);
    }

    void setSelected(bool selected)
    {
        if (property("chipSelected").toBool() == selected)
            return;

        setProperty("chipSelected", selected);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            m_pressPos = event->position().toPoint();
            m_owner->setSelectedChip(this);
            setCursor(Qt::ClosedHandCursor);
        }
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_pressed && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_pressed = false;
            m_owner->beginDragForChip(this);
            setCursor(Qt::OpenHandCursor);
            return;
        }
        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_pressed = false;
        setCursor(Qt::OpenHandCursor);
        QFrame::mouseReleaseEvent(event);
    }

private:
    ChipOrderWidget *m_owner;
    bool m_pressed = false;
    QPoint m_pressPos;
};

ChipOrderWidget::ChipOrderWidget(QWidget *parent)
    : QWidget(parent),
      m_layout(new QHBoxLayout(this)),
      m_selectedChip(nullptr),
      m_dropIndicatorIndex(-1)
{
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    // Keep a tiny gutter so insertion markers at both ends stay visible.
    m_layout->setContentsMargins(6, 0, 6, 0);
    m_layout->setSpacing(2);
}

void ChipOrderWidget::setChips(const QList<QPair<QString, QString>> &chips)
{
    qDeleteAll(m_chips);
    m_chips.clear();

    for (const auto &chip : chips)
        m_chips.append(new Chip(chip.first, chip.second, this));

    m_selectedChip = nullptr;
    rebuildLayout();
}

QStringList ChipOrderWidget::chipIds() const
{
    QStringList ids;
    for (const auto chip : m_chips)
        ids.append(chip->id);

    return ids;
}

void ChipOrderWidget::clearChipSelection()
{
    setSelectedChip(nullptr);
}

void ChipOrderWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        clearChipSelection();

    QWidget::mousePressEvent(event);
}

void ChipOrderWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(DRAG_MIME_FORMAT)) {
        setDropIndicatorIndex(insertionIndexAtX(event->position().toPoint().x()));
        event->acceptProposedAction();
    }
}

void ChipOrderWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat(DRAG_MIME_FORMAT)) {
        setDropIndicatorIndex(insertionIndexAtX(event->position().toPoint().x()));
        event->acceptProposedAction();
    }
}

void ChipOrderWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropIndicatorIndex(-1);
    QWidget::dragLeaveEvent(event);
}

void ChipOrderWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat(DRAG_MIME_FORMAT))
        return;

    QByteArray raw = event->mimeData()->data(DRAG_MIME_FORMAT);
    QDataStream stream(&raw, QIODevice::ReadOnly);
    int sourceIndex = -1;
    stream >> sourceIndex;
    if (sourceIndex < 0 || sourceIndex >= m_chips.size())
        return;

    const int targetIndex = insertionIndexAtX(event->position().toPoint().x());
    setDropIndicatorIndex(-1);
    reorderChip(sourceIndex, targetIndex);
    event->acceptProposedAction();
}

void ChipOrderWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    if (m_dropIndicatorIndex < 0)
        return;

    const int indicatorX = dropIndicatorX();
    if (indicatorX < 0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Highlight));

    constexpr int indicatorWidth = 4;
    const int indicatorHeight = qMax(10, height() - 4);
    const QRect indicatorRect(
        indicatorX - (indicatorWidth / 2), (height() - indicatorHeight) / 2, indicatorWidth, indicatorHeight);
    painter.drawRoundedRect(indicatorRect, 2, 2);
}

void ChipOrderWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    clearChipSelection();
}

bool ChipOrderWidget::event(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateCompactSize();
    return QWidget::event(event);
}

void ChipOrderWidget::setSelectedChip(Chip *chip)
{
    if (m_selectedChip == chip)
        return;

    if (m_selectedChip != nullptr)
        m_selectedChip->setSelected(false);
    m_selectedChip = chip;
    if (m_selectedChip != nullptr)
        m_selectedChip->setSelected(true);
}

void ChipOrderWidget::setDropIndicatorIndex(int index)
{
    const int boundedIndex = qBound(-1, index, m_chips.size());
    if (m_dropIndicatorIndex == boundedIndex)
        return;
    m_dropIndicatorIndex = boundedIndex;
    update();
}

int ChipOrderWidget::dropIndicatorX() const
{
    if (m_chips.isEmpty() || m_dropIndicatorIndex < 0)
        return -1;

    const int leftGutter = m_layout->contentsMargins().left();
    const int rightGutter = m_layout->contentsMargins().right();

    if (m_dropIndicatorIndex <= 0)
        return qMax(2, leftGutter / 2);
    if (m_dropIndicatorIndex >= m_chips.size())
        return width() - qMax(2, rightGutter / 2) - 1;

    const auto leftGeom = m_chips.at(m_dropIndicatorIndex - 1)->geometry();
    const auto rightGeom = m_chips.at(m_dropIndicatorIndex)->geometry();
    return (leftGeom.right() + rightGeom.left()) / 2;
}

int ChipOrderWidget::insertionIndexAtX(int x) const
{
    for (int i = 0; i < m_chips.size(); i++) {
        const auto chipCenterX = m_chips.at(i)->geometry().center().x();
        if (x < chipCenterX)
            return i;
    }
    return m_chips.size();
}

void ChipOrderWidget::reorderChip(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_chips.size())
        return;

    int insertIndex = qBound(0, toIndex, m_chips.size());
    if (insertIndex > fromIndex)
        insertIndex--;
    if (insertIndex == fromIndex)
        return;

    m_chips.move(fromIndex, insertIndex);
    rebuildLayout();
    emit orderChanged();
}

void ChipOrderWidget::beginDragForChip(Chip *chip)
{
    const int chipIndex = m_chips.indexOf(chip);
    if (chipIndex < 0)
        return;

    auto mime = new QMimeData;
    QByteArray raw;
    QDataStream stream(&raw, QIODevice::WriteOnly);
    stream << chipIndex;
    mime->setData(DRAG_MIME_FORMAT, raw);

    QDrag drag(this);
    drag.setMimeData(mime);
    drag.exec(Qt::MoveAction);
    setDropIndicatorIndex(-1);
}

void ChipOrderWidget::rebuildLayout()
{
    while (m_layout->count() > 0)
        m_layout->takeAt(0);
    for (auto chip : m_chips)
        m_layout->addWidget(chip);

    updateCompactSize();
}

void ChipOrderWidget::updateCompactSize()
{
    int totalWidth = 0;
    int maxHeight = 0;
    const QFontMetrics fm(font());
    for (auto chip : m_chips) {
        const int chipWidth = fm.horizontalAdvance(chip->title) + 16;
        const int chipHeight = fm.height() + 8;
        chip->setFixedSize(chipWidth, chipHeight);

        totalWidth += chipWidth;
        if (chipHeight > maxHeight)
            maxHeight = chipHeight;
    }

    if (m_chips.size() > 1)
        totalWidth += m_layout->spacing() * (m_chips.size() - 1);

    const int contentMargins = contentsMargins().left() + contentsMargins().right() + m_layout->contentsMargins().left()
                               + m_layout->contentsMargins().right();
    setFixedWidth(totalWidth + contentMargins + 2);
    setFixedHeight(maxHeight + 2);
}

} // namespace Syntalos
