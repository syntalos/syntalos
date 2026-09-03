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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sglxutils.h"

#include <QRegularExpression>
#include <algorithm>
#include <set>

namespace SglxUtils
{

QString streamName(Sglx::StreamId sid)
{
    switch (sid.js) {
    case Sglx::JS_NI:
        return QStringLiteral("nidq");
    case Sglx::JS_OB:
        return QStringLiteral("obx%1").arg(sid.ip);
    case Sglx::JS_IM:
        return QStringLiteral("imec%1").arg(sid.ip);
    default:
        return QStringLiteral("stream%1.%2").arg(sid.js).arg(sid.ip);
    }
}

std::optional<Sglx::StreamId> parseStreamName(const QString &name)
{
    const auto n = name.trimmed().toLower();
    if (n == QLatin1String("nidq") || n == QLatin1String("ni"))
        return Sglx::StreamId{Sglx::JS_NI, 0};

    static const QRegularExpression re(QStringLiteral("^(imec|obx)(\\d+)$"));
    const auto m = re.match(n);
    if (!m.hasMatch())
        return std::nullopt;

    Sglx::StreamId sid;
    sid.js = m.captured(1) == QLatin1String("imec") ? Sglx::JS_IM : Sglx::JS_OB;
    sid.ip = m.captured(2).toInt();
    return sid;
}

QString chanGroupName(ChanGroup group)
{
    switch (group) {
    case ChanGroup::ALL:
        return QStringLiteral("ALL");
    case ChanGroup::AP:
        return QStringLiteral("AP");
    case ChanGroup::LF:
        return QStringLiteral("LF");
    case ChanGroup::SY:
        return QStringLiteral("SY");
    case ChanGroup::XA:
        return QStringLiteral("XA");
    case ChanGroup::DW:
        return QStringLiteral("DW");
    case ChanGroup::MN:
        return QStringLiteral("MN");
    case ChanGroup::MA:
        return QStringLiteral("MA");
    }
    return {};
}

std::optional<ChanGroup> parseChanGroup(const QString &name)
{
    const auto n = name.trimmed().toUpper();
    if (n.isEmpty() || n == QLatin1String("ALL"))
        return ChanGroup::ALL;
    for (const auto g :
         {ChanGroup::AP, ChanGroup::LF, ChanGroup::SY, ChanGroup::XA, ChanGroup::DW, ChanGroup::MN, ChanGroup::MA}) {
        if (chanGroupName(g) == n)
            return g;
    }
    return std::nullopt;
}

std::vector<ChanGroup> chanGroupsForStream(int js)
{
    switch (js) {
    case Sglx::JS_NI:
        return {ChanGroup::MN, ChanGroup::MA, ChanGroup::XA, ChanGroup::DW};
    case Sglx::JS_OB:
        return {ChanGroup::XA, ChanGroup::DW, ChanGroup::SY};
    case Sglx::JS_IM:
        return {ChanGroup::AP, ChanGroup::LF, ChanGroup::SY};
    default:
        return {};
    }
}

bool isDigitalGroup(ChanGroup group)
{
    return group == ChanGroup::SY || group == ChanGroup::DW;
}

std::expected<std::pair<int, int>, QString> chanGroupRange(int js, const std::vector<int> &acqCounts, ChanGroup group)
{
    const auto groups = chanGroupsForStream(js);
    if (groups.empty())
        return std::unexpected(QStringLiteral("Unknown stream type %1").arg(js));
    if (acqCounts.size() != groups.size())
        return std::unexpected(QStringLiteral("Unexpected channel layout: got %1 channel-type counts, expected %2")
                                   .arg(acqCounts.size())
                                   .arg(groups.size()));

    int total = 0;
    for (const auto c : acqCounts)
        total += c;
    if (group == ChanGroup::ALL)
        return std::make_pair(0, total);

    int offset = 0;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (groups[i] == group)
            return std::make_pair(offset, acqCounts[i]);
        offset += acqCounts[i];
    }

    return std::unexpected(
        QStringLiteral("Channel group %1 does not exist in this stream type").arg(chanGroupName(group)));
}

std::expected<std::vector<int>, QString> parseChannelSpec(const QString &spec, int groupChanCount)
{
    std::vector<int> result;
    if (groupChanCount <= 0)
        return result;

    const auto s = spec.simplified().remove(' ').toLower();
    if (s.isEmpty() || s == QLatin1String("all")) {
        result.resize(groupChanCount);
        for (int i = 0; i < groupChanCount; ++i)
            result[i] = i;
        return result;
    }

    std::set<int> chans;
    const auto items = s.split(',', Qt::SkipEmptyParts);
    for (const auto &item : items) {
        int lo;
        int hi;
        bool okLo = false;
        bool okHi = false;
        const auto range = item.split(':');
        if (range.size() == 1) {
            lo = hi = range[0].toInt(&okLo);
            okHi = okLo;
        } else if (range.size() == 2) {
            lo = range[0].toInt(&okLo);
            hi = range[1].toInt(&okHi);
        } else {
            return std::unexpected(QStringLiteral("Invalid channel range '%1'").arg(item));
        }
        if (!okLo || !okHi)
            return std::unexpected(QStringLiteral("Invalid channel entry '%1'").arg(item));
        if (lo > hi)
            std::swap(lo, hi);
        if (lo < 0 || hi >= groupChanCount)
            return std::unexpected(QStringLiteral("Channel entry '%1' is outside of the valid range 0:%2")
                                       .arg(item)
                                       .arg(groupChanCount - 1));
        for (int c = lo; c <= hi; ++c)
            chans.insert(c);
    }

    result.assign(chans.begin(), chans.end());
    return result;
}

QString channelListToSpec(const std::vector<int> &chans)
{
    QStringList parts;
    size_t i = 0;
    while (i < chans.size()) {
        size_t j = i;
        while (j + 1 < chans.size() && chans[j + 1] == chans[j] + 1)
            ++j;
        if (j == i)
            parts << QString::number(chans[i]);
        else
            parts << QStringLiteral("%1:%2").arg(chans[i]).arg(chans[j]);
        i = j + 1;
    }
    return parts.join(',');
}

bool hasReservedGTSuffix(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("_[gG]\\d+_[tT]\\d+$"));
    return name.contains(re);
}

QString sanitizeRunName(const QString &name)
{
    static const QRegularExpression illegal(QStringLiteral("[/\\\\\\[\\]<>*\":;,?|=\\s]"));
    auto result = name.trimmed();
    result.replace(illegal, QStringLiteral("_"));

    // collapse runs of underscores and trim them from the ends
    static const QRegularExpression multiUnderscore(QStringLiteral("_{2,}"));
    result.replace(multiUnderscore, QStringLiteral("_"));
    while (result.startsWith('_'))
        result.remove(0, 1);
    while (result.endsWith('_'))
        result.chop(1);

    if (hasReservedGTSuffix(result))
        result += QStringLiteral("_r");

    return result;
}

} // namespace SglxUtils
