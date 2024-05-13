/*
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "upyconsole.h"

#include <QScrollBar>

#include "utils/style.h"

UPyConsole::UPyConsole(QWidget *parent)
    : QPlainTextEdit(parent)
{
    document()->setMaximumBlockCount(100);
    QPalette p = palette();
    p.setColor(QPalette::Base, SyColorDark);
    p.setColor(QPalette::Text, SyColorWhite);
    setPalette(p);
}

void UPyConsole::putData(const QByteArray &data)
{
    QTextCursor cursor = textCursor();
    int i = 0;
    while (i < data.size()) {
        if (data[i] == '\x1B') { // Start of ANSI escape sequence
            i++;
            if (i < data.size() && data[i] == '[') {
                i++;
                int number = 0;
                bool hasNumber = false;
                while (i < data.size() && data[i] >= '0' && data[i] <= '9') {
                    number = number * 10 + (data[i] - '0');
                    hasNumber = true;
                    i++;
                }
                if (i < data.size()) {
                    switch (data[i]) {
                    case 'D': // Cursor left
                        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, hasNumber ? number : 1);
                        cursor
                            .removeSelectedText(); // This effectively deletes the text that the cursor has passed over
                        setTextCursor(cursor);
                        break;
                    case 'K': // Clear from cursor to end of line
                        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
                        cursor.removeSelectedText();
                        setTextCursor(cursor);
                        break;
                    default:
                        break;
                    }
                }
            }
        } else if (data[i] == '\x08') {   // Handle backspace
            if (!cursor.atBlockStart()) { // Check to prevent removing the block if at start
                cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
                cursor.deleteChar();
                setTextCursor(cursor);
            }
        } else if (data[i] == '\n') { // Handle new lines
            cursor.insertText(QString('\n'));
            setTextCursor(cursor);
        } else if (data[i] > 31 && data[i] < 127) { // Handle printable ASCII characters
            cursor.insertText(QString(data[i]));
            setTextCursor(cursor);
        }
        i++;
    }

    ensureCursorVisible(); // Ensure the cursor is visible in the viewport
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void UPyConsole::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
        // we do not handle left/right movement for now
        // TODO: Implement correct cursor movement for left/right without breaking up/down behavior
        // (either MicroPython's REPL behaves a bit weirdly here, or we are not handling the cursor correctly)
        break;
    case Qt::Key_Up:
        emit newInput("\x1b[A");
        break;
    case Qt::Key_Down:
        emit newInput("\x1b[B");
        break;
    default:
        emit newInput(e->text().toLocal8Bit());
    }
}

void UPyConsole::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_UNUSED(e);
}
