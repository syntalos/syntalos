/*
 * Copyright (C) 2022-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "runnoticeswidget.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <KColorScheme>

static constexpr int NOTICE_ICON_SIZE = 16;

RunNoticesWidget::RunNoticesWidget(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);

    // we only ever want to take up the space our notices need, and should never
    // force the surrounding layout to become wider - long texts wrap instead
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setMinimumWidth(0);
    updateVisibility();
}

RunNoticesWidget::~RunNoticesWidget() = default;

QIcon RunNoticesWidget::iconForSeverity(Severity severity)
{
    switch (severity) {
    case Severity::Info:
        return QIcon::fromTheme(
            QStringLiteral("emblem-information"),
            QIcon::fromTheme(QStringLiteral("dialog-information")));
    case Severity::Positive:
        return QIcon::fromTheme(QStringLiteral("emblem-success"), QIcon::fromTheme(QStringLiteral("dialog-ok")));
    case Severity::Warning:
        return QIcon::fromTheme(QStringLiteral("emblem-warning"), QIcon::fromTheme(QStringLiteral("dialog-warning")));
    case Severity::Critical:
        return QIcon::fromTheme(QStringLiteral("emblem-important"), QIcon::fromTheme(QStringLiteral("dialog-error")));
    }
    return QIcon();
}

RunNoticesWidget::Entry *RunNoticesWidget::findEntry(const QString &key)
{
    for (auto &entry : m_entries) {
        if (entry.key == key)
            return &entry;
    }
    return nullptr;
}

const RunNoticesWidget::Entry *RunNoticesWidget::findEntry(const QString &key) const
{
    for (const auto &entry : m_entries) {
        if (entry.key == key)
            return &entry;
    }
    return nullptr;
}

RunNoticesWidget::Entry &RunNoticesWidget::ensureEntry(const QString &key)
{
    if (auto *existing = findEntry(key))
        return *existing;

    Entry entry;
    entry.key = key;
    entry.frame = new QFrame(this);
    entry.frame->setFrameShape(QFrame::NoFrame);
    entry.frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *hl = new QHBoxLayout(entry.frame);
    hl->setContentsMargins(4, 2, 4, 2);
    hl->setSpacing(4);

    entry.iconLabel = new QLabel(entry.frame);
    entry.iconLabel->setFixedSize(NOTICE_ICON_SIZE, NOTICE_ICON_SIZE);
    entry.iconLabel->setScaledContents(false);
    hl->addWidget(entry.iconLabel, 0, Qt::AlignTop);

    entry.textLabel = new QLabel(entry.frame);
    entry.textLabel->setWordWrap(true);
    entry.textLabel->setTextFormat(Qt::PlainText);
    entry.textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    entry.textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    entry.textLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    hl->addWidget(entry.textLabel, 1);

    entry.expiryTimer = new QTimer(entry.frame);
    entry.expiryTimer->setSingleShot(true);
    connect(entry.expiryTimer, &QTimer::timeout, this, [this, key]() {
        clearNotice(key);
    });

    m_layout->addWidget(entry.frame);
    m_entries.append(entry);
    return m_entries.last();
}

void RunNoticesWidget::removeEntry(const QString &key)
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].key != key)
            continue;
        auto *frame = m_entries[i].frame;
        m_layout->removeWidget(frame);
        frame->deleteLater();
        m_entries.removeAt(i);
        return;
    }
}

void RunNoticesWidget::applyStyle(Entry &entry)
{
    entry.iconLabel->setPixmap(iconForSeverity(entry.severity).pixmap(NOTICE_ICON_SIZE, NOTICE_ICON_SIZE));

    // tint the notice background in accordance with the current color scheme, so it also
    // looks right with dark themes
    KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    KColorScheme::BackgroundRole bgRole;
    switch (entry.severity) {
    case Severity::Positive:
        bgRole = KColorScheme::PositiveBackground;
        break;
    case Severity::Warning:
        bgRole = KColorScheme::NeutralBackground;
        break;
    case Severity::Critical:
        bgRole = KColorScheme::NegativeBackground;
        break;
    case Severity::Info:
    default:
        bgRole = KColorScheme::NormalBackground;
        break;
    }

    if (bgRole == KColorScheme::NormalBackground) {
        entry.frame->setStyleSheet(QString());
    } else {
        const auto bg = scheme.background(bgRole).color();
        entry.frame->setStyleSheet(
            QStringLiteral("QFrame { background-color: rgba(%1, %2, %3, %4); border-radius: 3px; }")
                .arg(bg.red())
                .arg(bg.green())
                .arg(bg.blue())
                .arg(bg.alpha()));
    }
}

void RunNoticesWidget::updateVisibility()
{
    setVisible(!m_entries.isEmpty());
}

void RunNoticesWidget::setNotice(const QString &key, Severity severity, const QString &text)
{
    auto &entry = ensureEntry(key);
    entry.expiryTimer->stop();
    entry.severity = severity;
    entry.textLabel->setText(text);
    entry.textLabel->setToolTip(text);
    applyStyle(entry);
    updateVisibility();
}

void RunNoticesWidget::resolveNotice(const QString &key, const QString &text, int timeoutMsec)
{
    if (text.isEmpty() || timeoutMsec <= 0) {
        clearNotice(key);
        return;
    }

    setNotice(key, Severity::Positive, text);
    findEntry(key)->expiryTimer->start(timeoutMsec);
}

void RunNoticesWidget::clearNotice(const QString &key)
{
    removeEntry(key);
    updateVisibility();
}

void RunNoticesWidget::clearAll()
{
    while (!m_entries.isEmpty())
        removeEntry(m_entries.first().key);
    updateVisibility();
}

bool RunNoticesWidget::hasNotice(const QString &key) const
{
    return findEntry(key) != nullptr;
}

int RunNoticesWidget::noticeCount() const
{
    return m_entries.size();
}

QString RunNoticesWidget::noticeText(const QString &key) const
{
    const auto *entry = findEntry(key);
    return entry ? entry->textLabel->text() : QString();
}

RunNoticesWidget::Severity RunNoticesWidget::noticeSeverity(const QString &key) const
{
    const auto *entry = findEntry(key);
    return entry ? entry->severity : Severity::Info;
}

void RunNoticesWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange) {
        for (auto &entry : m_entries)
            applyStyle(entry);
    }
}
