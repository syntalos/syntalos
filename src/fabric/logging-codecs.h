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

/**
 * Quill codec specializations for common Qt types.
 *
 * All types use DeferredFormatCodec - Qt's implicit sharing makes copies thread-safe.
 */

// format.h must be included to pull in format-inl.h (inline implementations of
// vformat_to etc.) in header-only mode, so TUs that don't include the backend get them.
#include <quill/bundled/fmt/format.h>
#include <quill/DeferredFormatCodec.h>

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUuid>

// -- QString ------------------------------------------------------------------

template<>
struct fmtquill::formatter<QString> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QString const &s, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", s.toStdString());
    }
};

template<>
struct quill::Codec<QString> : quill::DeferredFormatCodec<QString> {
};

// -- QByteArray --------------------------------------------------------------─

template<>
struct fmtquill::formatter<QByteArray> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QByteArray const &ba, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", std::string_view(ba.constData(), ba.size()));
    }
};

template<>
struct quill::Codec<QByteArray> : quill::DeferredFormatCodec<QByteArray> {
};

// -- QStringList --------------------------------------------------------------

template<>
struct fmtquill::formatter<QStringList> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QStringList const &list, format_context &ctx) const
    {
        auto out = ctx.out();
        *out++ = '[';
        for (int i = 0; i < list.size(); ++i) {
            if (i > 0)
                out = fmtquill::format_to(out, ", ");
            out = fmtquill::format_to(out, "\"{}\"", list[i].toStdString());
        }
        *out++ = ']';
        return out;
    }
};

template<>
struct quill::Codec<QStringList> : quill::DeferredFormatCodec<QStringList> {
};

// -- QPoint ------------------------------------------------------------------─

template<>
struct fmtquill::formatter<QPoint> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QPoint const &p, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "({}, {})", p.x(), p.y());
    }
};

template<>
struct quill::Codec<QPoint> : quill::DeferredFormatCodec<QPoint> {
};

// -- QPointF ------------------------------------------------------------------

template<>
struct fmtquill::formatter<QPointF> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QPointF const &p, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "({}, {})", p.x(), p.y());
    }
};

template<>
struct quill::Codec<QPointF> : quill::DeferredFormatCodec<QPointF> {
};

// -- QSize --------------------------------------------------------------------

template<>
struct fmtquill::formatter<QSize> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QSize const &s, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}x{}", s.width(), s.height());
    }
};

template<>
struct quill::Codec<QSize> : quill::DeferredFormatCodec<QSize> {
};

// -- QSizeF ------------------------------------------------------------------─

template<>
struct fmtquill::formatter<QSizeF> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QSizeF const &s, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}x{}", s.width(), s.height());
    }
};

template<>
struct quill::Codec<QSizeF> : quill::DeferredFormatCodec<QSizeF> {
};

// -- QRect --------------------------------------------------------------------

template<>
struct fmtquill::formatter<QRect> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QRect const &r, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "({}, {}, {}x{})", r.x(), r.y(), r.width(), r.height());
    }
};

template<>
struct quill::Codec<QRect> : quill::DeferredFormatCodec<QRect> {
};

// -- QRectF ------------------------------------------------------------------─

template<>
struct fmtquill::formatter<QRectF> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QRectF const &r, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "({}, {}, {}x{})", r.x(), r.y(), r.width(), r.height());
    }
};

template<>
struct quill::Codec<QRectF> : quill::DeferredFormatCodec<QRectF> {
};

// -- QColor ------------------------------------------------------------------─

template<>
struct fmtquill::formatter<QColor> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QColor const &c, format_context &ctx) const
    {
        if (c.alpha() == 255)
            return fmtquill::format_to(ctx.out(), "#{:02X}{:02X}{:02X}", c.red(), c.green(), c.blue());
        return fmtquill::format_to(ctx.out(), "rgba({}, {}, {}, {})", c.red(), c.green(), c.blue(), c.alpha());
    }
};

template<>
struct quill::Codec<QColor> : quill::DeferredFormatCodec<QColor> {
};

// -- QDate --------------------------------------------------------------------

template<>
struct fmtquill::formatter<QDate> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QDate const &d, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", d.toString(Qt::ISODate).toStdString());
    }
};

template<>
struct quill::Codec<QDate> : quill::DeferredFormatCodec<QDate> {
};

// -- QTime --------------------------------------------------------------------

template<>
struct fmtquill::formatter<QTime> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QTime const &t, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", t.toString(Qt::ISODateWithMs).toStdString());
    }
};

template<>
struct quill::Codec<QTime> : quill::DeferredFormatCodec<QTime> {
};

// -- QDateTime ----------------------------------------------------------------

template<>
struct fmtquill::formatter<QDateTime> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QDateTime const &dt, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", dt.toString(Qt::ISODateWithMs).toStdString());
    }
};

template<>
struct quill::Codec<QDateTime> : quill::DeferredFormatCodec<QDateTime> {
};

// -- QUrl --------------------------------------------------------------------─

template<>
struct fmtquill::formatter<QUrl> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QUrl const &u, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", u.toString().toStdString());
    }
};

template<>
struct quill::Codec<QUrl> : quill::DeferredFormatCodec<QUrl> {
};

// -- QUuid --------------------------------------------------------------------

template<>
struct fmtquill::formatter<QUuid> {
    constexpr auto parse(format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(QUuid const &u, format_context &ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", u.toString(QUuid::WithoutBraces).toStdString());
    }
};

template<>
struct quill::Codec<QUuid> : quill::DeferredFormatCodec<QUuid> {
};
