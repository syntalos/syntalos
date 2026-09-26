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

#pragma once

#include <QString>

class QWidget;

namespace Syntalos
{

/**
 * @brief Enable or disable non-interactive mode for the whole application.
 *
 * In non-interactive mode, no modal prompts are shown: errors are only logged, and
 * questions are answered with their conservative default.
 */
void setNonInteractiveMode(bool enabled);

/**
 * @brief Whether the application runs in non-interactive mode.
 */
bool isNonInteractive();

/**
 * @brief Report a critical error to the user.
 *
 * The error is always logged, a modal dialog is only shown in interactive mode.
 */
void reportCriticalError(QWidget *parent, const QString &title, const QString &message);

/**
 * @brief Report a warning to the user.
 *
 * The warning is always logged, a modal dialog is only shown in interactive mode.
 */
void reportWarning(QWidget *parent, const QString &title, const QString &message);

/**
 * @brief Ask the user a yes/no question.
 *
 * In non-interactive mode the question is logged and @p nonInteractiveAnswer is returned.
 * @return true if the answer is "yes".
 */
bool askYesNoQuestion(
    QWidget *parent,
    const QString &title,
    const QString &question,
    bool nonInteractiveAnswer = false);

} // namespace Syntalos
