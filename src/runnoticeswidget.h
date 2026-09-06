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

#pragma once

#include <QIcon>
#include <QList>
#include <QWidget>

class QVBoxLayout;
class QFrame;
class QLabel;
class QTimer;

/**
 * @brief Stacked list of notices about the current run.
 *
 * Each notice is identified by a key, so updates to the same issue replace
 * the existing entry instead of piling up, while different issues are shown
 * next to each other and never overwrite one another.
 * Resolved issues can be shown as transient positive notice.
 */
class RunNoticesWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Severity {
        Info,
        Positive,
        Warning,
        Critical
    };
    Q_ENUM(Severity)

    explicit RunNoticesWidget(QWidget *parent = nullptr);
    ~RunNoticesWidget() override;

    /**
     * @brief Show or update the notice with the given key.
     */
    void setNotice(const QString &key, Severity severity, const QString &text);

    /**
     * @brief Replace the notice with a transient positive one that clears itself.
     *
     * If @p text is empty, the notice is removed immediately.
     */
    void resolveNotice(const QString &key, const QString &text, int timeoutMsec = 20 * 1000);

    void clearNotice(const QString &key);
    void clearAll();

    bool hasNotice(const QString &key) const;
    int noticeCount() const;

    /**
     * @brief Text and severity of the given notice, if any.
     */
    QString noticeText(const QString &key) const;
    Severity noticeSeverity(const QString &key) const;

    static QIcon iconForSeverity(Severity severity);

protected:
    void changeEvent(QEvent *event) override;

private:
    struct Entry {
        QString key;
        QFrame *frame = nullptr;
        QLabel *iconLabel = nullptr;
        QLabel *textLabel = nullptr;
        QTimer *expiryTimer = nullptr;
        Severity severity = Severity::Info;
    };

    Entry *findEntry(const QString &key);
    const Entry *findEntry(const QString &key) const;
    Entry &ensureEntry(const QString &key);
    void removeEntry(const QString &key);
    void applyStyle(Entry &entry);
    void updateVisibility();

    QVBoxLayout *m_layout;
    QList<Entry> m_entries;
};
