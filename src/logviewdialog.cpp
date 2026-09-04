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

#include "logviewdialog.h"
#include "ui_logviewdialog.h"

#include <QClipboard>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <sys/stat.h>

#include "logging.h"
#include "utils/style.h"

using namespace Syntalos;

// Matches the prefix of a log line written by our file sink, e.g.
// "26-09-03 12:34:56.123456 W main:engine: message", capturing the level short code.
static const QRegularExpression s_logLineRx(
    QStringLiteral(R"(^\d{2}-\d\d-\d\d \d\d:\d\d:\d\d\.\d+ (T3|T2|T1|D|I|N|W|E|C|BT) )"));

LogViewDialog::LogViewDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LogViewDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon::fromTheme(QStringLiteral("text-x-log")));

    ui->logView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->logView->document()->setMaximumBlockCount(MAX_BLOCKS);
    ui->logView->setCenterOnScroll(false);

    const auto logFile = currentLogFilePath();
    if (logFile.isEmpty()) {
        ui->lblFilePath->setText(QStringLiteral("Logging to a file is disabled."));
        ui->btnOpenFolder->setEnabled(false);
        ui->cbFollow->setEnabled(false);
    } else {
        ui->lblFilePath->setText(logFile);
    }

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(POLL_INTERVAL_MSEC);
    connect(m_pollTimer, &QTimer::timeout, this, &LogViewDialog::pollNewData);

    connect(ui->btnClose, &QPushButton::clicked, this, &QDialog::close);

    updateFormats();
}

LogViewDialog::~LogViewDialog()
{
    closeFile();
    delete ui;
}

void LogViewDialog::setModuleLoaderLogHtml(const QString &html)
{
    if (html.isEmpty())
        ui->modLoaderView->setHtml(QStringLiteral("<i>No issues reported.</i>"));
    else
        ui->modLoaderView->setHtml(html);
}

void LogViewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    reloadTail();
    m_pollTimer->start();
}

void LogViewDialog::hideEvent(QHideEvent *event)
{
    // stop following and release all log data while we are not visible
    m_pollTimer->stop();
    closeFile();
    ui->logView->clear();
    m_partial.clear();
    QDialog::hideEvent(event);
}

void LogViewDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        updateFormats();
        if (isVisible())
            reloadTail();
    }
}

void LogViewDialog::on_btnOpenFolder_clicked()
{
    const auto dir = currentLogDir();
    const auto logFile = currentLogFilePath();
    if (dir.isEmpty())
        return;

    // Ask the file manager to show the directory with the current log file selected.
    // This is a freedesktop interface implemented by Dolphin, Nautilus, Thunar and others.
    QDBusInterface fileManager(
        QStringLiteral("org.freedesktop.FileManager1"),
        QStringLiteral("/org/freedesktop/FileManager1"),
        QStringLiteral("org.freedesktop.FileManager1"));
    if (fileManager.isValid()) {
        // do not stall the UI for long if the file manager is slow to activate
        fileManager.setTimeout(5000);
        QDBusReply<void> reply = fileManager.call(
            QStringLiteral("ShowItems"),
            QStringList{QUrl::fromLocalFile(logFile).toString()},
            QString());
        if (reply.isValid())
            return;
    }

    // fall back to just opening the directory
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void LogViewDialog::on_btnCopy_clicked()
{
    QGuiApplication::clipboard()->setText(ui->logView->toPlainText());
}

void LogViewDialog::updateFormats()
{
    const auto pal = ui->logView->palette();

    m_fmtDefault = QTextCharFormat();
    m_fmtDefault.setForeground(pal.color(QPalette::Text));

    m_fmtDim = m_fmtDefault;
    m_fmtDim.setForeground(pal.color(QPalette::PlaceholderText));

    m_fmtWarning = m_fmtDefault;
    m_fmtWarning.setForeground(SyColorWarning);

    m_fmtError = m_fmtDefault;
    m_fmtError.setForeground(SyColorDanger);

    m_fmtCritical = m_fmtDefault;
    m_fmtCritical.setForeground(SyColorDangerHigh);
    m_fmtCritical.setFontWeight(QFont::Bold);

    m_lastFmt = m_fmtDefault;
}

void LogViewDialog::closeFile()
{
    if (m_file.isOpen())
        m_file.close();
    m_pos = 0;
    m_inode = 0;
}

void LogViewDialog::scrollToEnd()
{
    auto sb = ui->logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void LogViewDialog::reloadTail()
{
    closeFile();
    ui->logView->clear();
    m_partial.clear();
    m_lastFmt = m_fmtDefault;

    const auto path = currentLogFilePath();
    if (path.isEmpty())
        return;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        // the file may not exist yet, pollNewData() will retry
        return;
    }

    // remember which file we have open, so we notice when the log is rotated
    struct stat st = {};
    if (fstat(m_file.handle(), &st) == 0)
        m_inode = st.st_ino;

    const qint64 size = m_file.size();
    m_pos = std::max<qint64>(0, size - TAIL_BYTES);
    m_file.seek(m_pos);
    QByteArray data = m_file.read(size - m_pos);

    const bool startedMidFile = m_pos > 0;
    m_pos += data.size();
    if (startedMidFile) {
        // we most likely started in the middle of a line, drop the partial one
        const auto nl = data.indexOf('\n');
        data = (nl < 0) ? QByteArray() : data.mid(nl + 1);
    }

    appendData(data);
    scrollToEnd();
}

void LogViewDialog::pollNewData()
{
    if (!m_file.isOpen()) {
        // the log file may have appeared in the meantime
        reloadTail();
        return;
    }

    // The log sink rotates by renaming the active file and creating a new one, so
    // check whether the path still refers to the file we have open.
    struct stat st = {};
    if (stat(qPrintable(m_file.fileName()), &st) != 0 || st.st_ino != m_inode) {
        reloadTail();
        return;
    }

    // QFile::size() on an open, unbuffered file returns the current on-disk size
    const qint64 size = m_file.size();
    if (size < m_pos) {
        // the file was truncated underneath us
        reloadTail();
        return;
    }
    if (size == m_pos)
        return;

    const bool follow = ui->cbFollow->isChecked();

    m_file.seek(m_pos);
    while (true) {
        const auto chunk = m_file.read(64 * 1024);
        if (chunk.isEmpty())
            break;
        m_pos += chunk.size();
        appendData(chunk);
    }

    if (follow)
        scrollToEnd();
}

void LogViewDialog::appendData(const QByteArray &data)
{
    m_partial.append(data);

    QTextCursor cursor(ui->logView->document());
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();

    qsizetype start = 0;
    while (true) {
        const auto nl = m_partial.indexOf('\n', start);
        if (nl < 0)
            break;
        appendLine(QByteArrayView(m_partial).sliced(start, nl - start), cursor);
        start = nl + 1;
    }

    cursor.endEditBlock();

    // keep the incomplete remainder (if any) until the rest of the line arrives
    m_partial.remove(0, start);
}

void LogViewDialog::appendLine(QByteArrayView line, QTextCursor &cursor)
{
    const auto text = QString::fromUtf8(line);

    const auto match = s_logLineRx.matchView(text);
    if (match.hasMatch()) {
        const auto level = match.capturedView(1);
        if (level == u"W")
            m_lastFmt = m_fmtWarning;
        else if (level == u"E")
            m_lastFmt = m_fmtError;
        else if (level == u"C")
            m_lastFmt = m_fmtCritical;
        else if (level == u"I" || level == u"N")
            m_lastFmt = m_fmtDefault;
        else
            m_lastFmt = m_fmtDim;
    }
    // lines without a prefix are continuations of a multi-line message
    // and inherit the format of the line they belong to

    if (!ui->logView->document()->isEmpty())
        cursor.insertBlock();
    cursor.insertText(text, m_lastFmt);
}
