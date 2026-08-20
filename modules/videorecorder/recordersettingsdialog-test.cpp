/*
 * Copyright (C) 2026 Syntalos contributors
 *
 * Licensed under the GNU Lesser General Public License Version 3
 */

#include "recordersettingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QtTest>

namespace
{
int findCodecIndex(const QComboBox *comboBox, VideoCodec codec)
{
    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i).value<VideoCodec>() == codec)
            return i;
    }
    return -1;
}
} // namespace

class RecorderSettingsDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        initializeSyLogSystem();
    }

    void losslessModeIsEnforcedByCodecProperties()
    {
        CodecProperties ffv1(VideoCodec::FFV1);
        ffv1.setLossless(false);
        QVERIFY(ffv1.isLossless());

        CodecProperties raw(VideoCodec::Raw);
        raw.setLossless(false);
        QVERIFY(raw.isLossless());

        CodecProperties mpeg4(VideoCodec::MPEG4);
        mpeg4.setLossless(true);
        QVERIFY(!mpeg4.isLossless());

        CodecProperties av1(VideoCodec::AV1);
        av1.setLossless(true);
        QVERIFY(av1.isLossless());
        av1.setLossless(false);
        QVERIFY(!av1.isLossless());
    }

    void freshDialogHasSynchronizedFfv1State()
    {
        RecorderSettingsDialog dialog;
        const auto codecComboBox = dialog.findChild<QComboBox *>(QStringLiteral("codecComboBox"));
        const auto containerComboBox = dialog.findChild<QComboBox *>(QStringLiteral("containerComboBox"));
        const auto losslessCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("losslessCheckBox"));
        const auto losslessLabel = dialog.findChild<QLabel *>(QStringLiteral("losslessLabel"));

        QVERIFY(codecComboBox);
        QVERIFY(containerComboBox);
        QVERIFY(losslessCheckBox);
        QVERIFY(losslessLabel);
        QVERIFY(codecComboBox->currentData().value<VideoCodec>() == VideoCodec::FFV1);
        QVERIFY(dialog.codecProps().codec() == VideoCodec::FFV1);
        QVERIFY(dialog.codecProps().isLossless());
        QVERIFY(losslessCheckBox->isChecked());
        QVERIFY(!losslessCheckBox->isEnabled());
        QVERIFY(!losslessLabel->isEnabled());
        QVERIFY(!containerComboBox->isEnabled());
    }

    void codecSwitchKeepsUiAndModelSynchronized()
    {
        RecorderSettingsDialog dialog;
        const auto codecComboBox = dialog.findChild<QComboBox *>(QStringLiteral("codecComboBox"));
        const auto losslessCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("losslessCheckBox"));

        QVERIFY(codecComboBox);
        QVERIFY(losslessCheckBox);

        const int av1Index = findCodecIndex(codecComboBox, VideoCodec::AV1);
        const int ffv1Index = findCodecIndex(codecComboBox, VideoCodec::FFV1);
        QVERIFY(av1Index >= 0);
        QVERIFY(ffv1Index >= 0);

        codecComboBox->setCurrentIndex(av1Index);
        QVERIFY(dialog.codecProps().codec() == VideoCodec::AV1);
        QVERIFY(!dialog.codecProps().isLossless());
        QVERIFY(losslessCheckBox->isEnabled());
        QVERIFY(!losslessCheckBox->isChecked());

        losslessCheckBox->setChecked(true);
        QVERIFY(dialog.codecProps().isLossless());

        codecComboBox->setCurrentIndex(ffv1Index);
        QVERIFY(dialog.codecProps().codec() == VideoCodec::FFV1);
        QVERIFY(dialog.codecProps().isLossless());
        QVERIFY(losslessCheckBox->isChecked());
        QVERIFY(!losslessCheckBox->isEnabled());
    }

    void inconsistentSavedFfv1StateIsNormalized()
    {
        CodecProperties ffv1(VideoCodec::FFV1);
        auto savedState = ffv1.toVariant();
        savedState[QStringLiteral("lossless")] = false;

        CodecProperties restored(savedState);
        QVERIFY(restored.isLossless());

        RecorderSettingsDialog dialog;
        dialog.setCodecProps(restored);
        const auto losslessCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("losslessCheckBox"));
        QVERIFY(losslessCheckBox);
        QVERIFY(losslessCheckBox->isChecked());
        QVERIFY(!losslessCheckBox->isEnabled());

        // Even a programmatic invalid toggle must not corrupt the codec model.
        losslessCheckBox->setChecked(false);
        QVERIFY(dialog.codecProps().isLossless());
        QVERIFY(losslessCheckBox->isChecked());
    }

    void unavailableVaapiConfigurationIsDisabledInModel()
    {
        RecorderSettingsDialog dialog;
        const auto vaapiCheckBox = dialog.findChild<QCheckBox *>(QStringLiteral("vaapiCheckBox"));
        QVERIFY(vaapiCheckBox);

        CodecProperties av1(VideoCodec::AV1);
        av1.setUseVaapi(true);
        dialog.setCodecProps(av1);

        if (vaapiCheckBox->isEnabled()) {
            QVERIFY(dialog.codecProps().useVaapi());
            QVERIFY(vaapiCheckBox->isChecked());
        } else {
            QVERIFY(!dialog.codecProps().useVaapi());
            QVERIFY(!vaapiCheckBox->isChecked());
        }
    }
};

QTEST_MAIN(RecorderSettingsDialogTest)
#include "recordersettingsdialog-test.moc"
