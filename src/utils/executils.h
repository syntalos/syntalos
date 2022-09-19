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

#pragma once

#include <QObject>

/**
 * @brief Shell-escape a string
 */
QString shellQuote(const QString &str);

QString findHostExecutable(const QString &exe);
int runHostExecutable(const QString &exe,
                      const QStringList &args,
                      bool waitForFinished);

int runInExternalTerminal(const QString &cmd, const QStringList &args = QStringList(),
                          const QString &wdir = QString());
