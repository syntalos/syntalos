/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QCoreApplication>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <KTextEditor/Editor>
#include <KSyntaxHighlighting/Repository>
#pragma GCC diagnostic pop
#include <ksyntaxhighlighting_version.h>

namespace Syntalos
{

/**
 * @brief Obtain the global KTextEditor instance.
 *
 * This wraps KTextEditor::Editor::instance() and applies workarounds for
 * known issues in the underlying libraries. Always use this function instead
 * of calling KTextEditor::Editor::instance() directly.
 */
inline KTextEditor::Editor *kTextEditorInstance()
{
    auto editor = KTextEditor::Editor::instance();

#if KSYNTAXHIGHLIGHTING_VERSION >= QT_VERSION_CHECK(6, 28, 0) \
    && KSYNTAXHIGHLIGHTING_VERSION < QT_VERSION_CHECK(6, 29, 0)
    // KSyntaxHighlighting 6.28 installs an application event filter that performs a full
    // Repository::reload() whenever a QEvent::LanguageChange is received. This resets all
    // format IDs, but KTextEditor does not recreate the highlighters of existing documents,
    // so their cached format-ID maps become stale and the next highlighting pass crashes
    // in KateHighlighting::applyFormat().
    // QTermWidget installs a translator in its constructor, which sends exactly such an
    // event, so any editor document created before a terminal widget would crash later.
    // This was fixed upstream in KSyntaxHighlighting 6.29.0:
    // https://invent.kde.org/frameworks/syntax-highlighting/-/commit/62667617
    // We don't switch languages at runtime, so we can simply detach the filter.
    auto repo = const_cast<KSyntaxHighlighting::Repository *>(&editor->repository());
    QCoreApplication::instance()->removeEventFilter(repo);
#endif

    return editor;
}

} // namespace Syntalos
