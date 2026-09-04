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

#include <QByteArray>
#include <QDialog>
#include <QFile>
#include <QTextCharFormat>

class QTimer;

namespace Ui
{
class LogViewDialog;
}

namespace Syntalos
{

/**
 * @brief Tail-style viewer for the current application log data.
 */
class LogViewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogViewDialog(QWidget *parent = nullptr);
    ~LogViewDialog() override;

    /**
     * @brief Set the module loader issue log to display.
     * @param html The log as HTML fragment, may be empty if there were no issues.
     */
    void setModuleLoaderLogHtml(const QString &html);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void on_btnOpenFolder_clicked();
    void on_btnCopy_clicked();

private:
    void updateFormats();
    void reloadTail();
    void pollNewData();
    void appendData(const QByteArray &data);
    void appendLine(QByteArrayView line, QTextCursor &cursor);
    void closeFile();
    void scrollToEnd();

    Ui::LogViewDialog *ui;
    QTimer *m_pollTimer;
    QFile m_file;
    qint64 m_pos{0};
    quint64 m_inode{0};
    QByteArray m_partial;

    QTextCharFormat m_fmtDefault;
    QTextCharFormat m_fmtDim;
    QTextCharFormat m_fmtWarning;
    QTextCharFormat m_fmtError;
    QTextCharFormat m_fmtCritical;
    QTextCharFormat m_lastFmt;

    static constexpr qint64 TAIL_BYTES = 256 * 1024;
    static constexpr int MAX_BLOCKS = 5000;
    static constexpr int POLL_INTERVAL_MSEC = 300;
};

} // namespace Syntalos
