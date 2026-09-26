/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "uiprompts.h"

#include <QMessageBox>
#include <atomic>

#include "logging.h"

namespace Syntalos
{

static std::atomic_bool g_nonInteractive{false};

static quill::Logger *uiLogger()
{
    static quill::Logger *const log = getLogger("ui");
    return log;
}

void setNonInteractiveMode(bool enabled)
{
    g_nonInteractive = enabled;
}

bool isNonInteractive()
{
    return g_nonInteractive;
}

void reportCriticalError(QWidget *parent, const QString &title, const QString &message)
{
    LOG_ERROR(uiLogger(), "{}: {}", title, message);
    if (isNonInteractive())
        return;
    QMessageBox::critical(parent, title, message);
}

void reportWarning(QWidget *parent, const QString &title, const QString &message)
{
    LOG_WARNING(uiLogger(), "{}: {}", title, message);
    if (isNonInteractive())
        return;
    QMessageBox::warning(parent, title, message);
}

bool askYesNoQuestion(QWidget *parent, const QString &title, const QString &question, bool nonInteractiveAnswer)
{
    if (isNonInteractive()) {
        LOG_WARNING(
            uiLogger(),
            "{}: {} - Answered with '{}' (non-interactive mode)",
            title,
            question,
            nonInteractiveAnswer ? "yes" : "no");
        return nonInteractiveAnswer;
    }
    const auto reply = QMessageBox::question(parent, title, question, QMessageBox::Yes | QMessageBox::No);
    return reply == QMessageBox::Yes;
}

} // namespace Syntalos
