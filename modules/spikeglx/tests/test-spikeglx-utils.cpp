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

#include <QtTest>

#include "sglxutils.h"

using namespace SglxUtils;

class TestSglxUtils : public QObject
{
    Q_OBJECT
private slots:
    void streamNames()
    {
        QCOMPARE(streamName(Sglx::StreamId{Sglx::JS_IM, 0}), QStringLiteral("imec0"));
        QCOMPARE(streamName(Sglx::StreamId{Sglx::JS_OB, 3}), QStringLiteral("obx3"));
        QCOMPARE(streamName(Sglx::StreamId{Sglx::JS_NI, 0}), QStringLiteral("nidq"));

        auto sid = parseStreamName(QStringLiteral(" IMEC2 "));
        QVERIFY(sid.has_value());
        QCOMPARE(sid->js, Sglx::JS_IM);
        QCOMPARE(sid->ip, 2);

        sid = parseStreamName(QStringLiteral("obx0"));
        QVERIFY(sid.has_value());
        QCOMPARE(sid->js, Sglx::JS_OB);

        sid = parseStreamName(QStringLiteral("nidq"));
        QVERIFY(sid.has_value());
        QCOMPARE(sid->js, Sglx::JS_NI);

        QVERIFY(!parseStreamName(QStringLiteral("imec")).has_value());
        QVERIFY(!parseStreamName(QStringLiteral("probe0")).has_value());
        QVERIFY(!parseStreamName(QString()).has_value());
    }

    void channelGroups()
    {
        QCOMPARE(parseChanGroup(QStringLiteral("ap")).value(), ChanGroup::AP);
        QCOMPARE(parseChanGroup(QString()).value(), ChanGroup::ALL);
        QVERIFY(!parseChanGroup(QStringLiteral("XY")).has_value());

        // IMEC: {AP, LF, SY}
        const std::vector<int> imCounts = {384, 384, 1};
        auto r = chanGroupRange(Sglx::JS_IM, imCounts, ChanGroup::LF);
        QVERIFY(r.has_value());
        QCOMPARE(r->first, 384);
        QCOMPARE(r->second, 384);
        r = chanGroupRange(Sglx::JS_IM, imCounts, ChanGroup::SY);
        QVERIFY(r.has_value());
        QCOMPARE(r->first, 768);
        QCOMPARE(r->second, 1);
        r = chanGroupRange(Sglx::JS_IM, imCounts, ChanGroup::ALL);
        QVERIFY(r.has_value());
        QCOMPARE(r->first, 0);
        QCOMPARE(r->second, 769);
        QVERIFY(!chanGroupRange(Sglx::JS_IM, imCounts, ChanGroup::XA).has_value());
        QVERIFY(!chanGroupRange(Sglx::JS_IM, {384, 384}, ChanGroup::AP).has_value());

        // OneBox: {XA, DW, SY}
        const std::vector<int> obCounts = {12, 1, 1};
        r = chanGroupRange(Sglx::JS_OB, obCounts, ChanGroup::DW);
        QVERIFY(r.has_value());
        QCOMPARE(r->first, 12);
        QCOMPARE(r->second, 1);

        QVERIFY(isDigitalGroup(ChanGroup::SY));
        QVERIFY(isDigitalGroup(ChanGroup::DW));
        QVERIFY(!isDigitalGroup(ChanGroup::AP));
    }

    void channelSpecs()
    {
        auto r = parseChannelSpec(QString(), 4);
        QVERIFY(r.has_value());
        QCOMPARE(*r, (std::vector<int>{0, 1, 2, 3}));

        r = parseChannelSpec(QStringLiteral("all"), 2);
        QVERIFY(r.has_value());
        QCOMPARE(*r, (std::vector<int>{0, 1}));

        r = parseChannelSpec(QStringLiteral("0:3, 8, 2, 10:9"), 16);
        QVERIFY(r.has_value());
        QCOMPARE(*r, (std::vector<int>{0, 1, 2, 3, 8, 9, 10}));

        QVERIFY(!parseChannelSpec(QStringLiteral("0:16"), 16).has_value());
        QVERIFY(!parseChannelSpec(QStringLiteral("-1"), 16).has_value());
        QVERIFY(!parseChannelSpec(QStringLiteral("a:b"), 16).has_value());
        QVERIFY(!parseChannelSpec(QStringLiteral("1:2:3"), 16).has_value());

        QCOMPARE(channelListToSpec({0, 1, 2, 3, 8, 9, 10}), QStringLiteral("0:3,8:10"));
        QCOMPARE(channelListToSpec({5}), QStringLiteral("5"));
        QCOMPARE(channelListToSpec({}), QString());
    }

    void digitalLines()
    {
        // digital lines are packed into 16-bit words, so a line is word * 16 + bit
        QCOMPARE(digitalLineCount(1), 16);
        QCOMPARE(digitalLineCount(4), 64);

        // a single SY word: the whole line spec fits into word 0
        auto r = parseChannelSpec(QString(), digitalLineCount(1), QStringLiteral("Line"));
        QVERIFY(r.has_value());
        QCOMPARE(r->size(), 16ul);
        QCOMPARE(digitalWordsForLines(*r), (std::vector<int>{0}));

        // the imec sync waveform
        r = parseChannelSpec(QStringLiteral("6"), digitalLineCount(1), QStringLiteral("Line"));
        QVERIFY(r.has_value());
        QCOMPARE(*r, (std::vector<int>{6}));
        QCOMPARE(digitalWordsForLines(*r), (std::vector<int>{0}));

        // a quad-base probe has four SY words, one per shank
        r = parseChannelSpec(QStringLiteral("0:63"), digitalLineCount(4), QStringLiteral("Line"));
        QVERIFY(r.has_value());
        QCOMPARE(digitalWordsForLines(*r), (std::vector<int>{0, 1, 2, 3}));

        // a sparse selection only needs the words the lines live in
        r = parseChannelSpec(QStringLiteral("6,33"), digitalLineCount(4), QStringLiteral("Line"));
        QVERIFY(r.has_value());
        QCOMPARE(*r, (std::vector<int>{6, 33}));
        QCOMPARE(digitalWordsForLines(*r), (std::vector<int>{0, 2}));

        QCOMPARE(digitalWordsForLines({}), (std::vector<int>{}));

        // lines beyond the group are rejected, and the message names them as lines
        auto bad = parseChannelSpec(QStringLiteral("16"), digitalLineCount(1), QStringLiteral("Line"));
        QVERIFY(!bad.has_value());
        QVERIFY(bad.error().startsWith(QStringLiteral("Line entry '16'")));
    }

    void runNameSanitizing()
    {
        QCOMPARE(sanitizeRunName(QStringLiteral("mouse 12/exp:1")), QStringLiteral("mouse_12_exp_1"));
        QCOMPARE(sanitizeRunName(QStringLiteral("__a__b__")), QStringLiteral("a_b"));
        QCOMPARE(sanitizeRunName(QStringLiteral("fine-name.v2")), QStringLiteral("fine-name.v2"));
        QCOMPARE(sanitizeRunName(QString()), QString());

        QVERIFY(hasReservedGTSuffix(QStringLiteral("run_g0_t3")));
        QVERIFY(hasReservedGTSuffix(QStringLiteral("run_G12_T0")));
        QVERIFY(!hasReservedGTSuffix(QStringLiteral("run_g0")));
        QVERIFY(!hasReservedGTSuffix(QStringLiteral("run_g0_t3_more")));
        QCOMPARE(sanitizeRunName(QStringLiteral("run_g0_t3")), QStringLiteral("run_g0_t3_r"));
        QVERIFY(!hasReservedGTSuffix(sanitizeRunName(QStringLiteral("run_g0_t3"))));
    }
};

QTEST_MAIN(TestSglxUtils)
#include "test-spikeglx-utils.moc"
