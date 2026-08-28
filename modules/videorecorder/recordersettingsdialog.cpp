/**
 * Copyright (C) 2019-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "recordersettingsdialog.h"
#include "ui_recordersettingsdialog.h"

#include <QMessageBox>
#include <QSignalBlocker>
#include <QThread>
#include <QVariant>

#include "utils/misc.h"

RecorderSettingsDialog::RecorderSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::RecorderSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    ui->containerComboBox->addItem("MKV", QVariant::fromValue(VideoContainer::Matroska));
    ui->containerComboBox->addItem("AVI", QVariant::fromValue(VideoContainer::AVI));
    ui->containerComboBox->setCurrentIndex(0);

    // We currently only permit a limited set of codecs - less choices are better here.
    // Currently FFV1 is the best option for lossless encoding, and VP9 is the best choice
    // for lossy encoding, unless the CPU is capable of encoding AV1 quickly enough

    ui->codecComboBox->addItem("FFV1", QVariant::fromValue(VideoCodec::FFV1));
    ui->codecComboBox->addItem("AV1", QVariant::fromValue(VideoCodec::AV1));
    ui->codecComboBox->addItem("VP9", QVariant::fromValue(VideoCodec::VP9));
    ui->codecComboBox->addItem("HEVC", QVariant::fromValue(VideoCodec::HEVC));
    ui->codecComboBox->addItem("H.264", QVariant::fromValue(VideoCodec::H264));
    ui->codecComboBox->addItem("Raw", QVariant::fromValue(VideoCodec::Raw));
    ui->codecComboBox->setCurrentIndex(0);

    // take name from source module by default
    ui->nameFromSrcCheckBox->setChecked(true);

    // VAAPi is disabled by default
    ui->vaapiCheckBox->setEnabled(false);
    ui->vaapiCheckBox->setChecked(false);
    ui->vaapiLabel->setEnabled(false);
    ui->renderNodeLabel->setEnabled(false);
    ui->renderNodeComboBox->setEnabled(false);
    m_renderNodes = findVideoRenderNodes();
    for (const auto &node : m_renderNodes.keys())
        ui->renderNodeComboBox->addItem(m_renderNodes.value(node), node);

    // no slicing warning by default
    ui->sliceWarnButton->setVisible(false);
    on_slicingCheckBox_toggled(ui->slicingCheckBox->isChecked());

    // no deferred encoding by default
    ui->encodeAfterRunCheckBox->setChecked(false);
    on_encodeAfterRunCheckBox_toggled(ui->encodeAfterRunCheckBox->isChecked());
    ui->deferredParallelCountSpinBox->setMaximum(QThread::idealThreadCount() + 1);
    ui->deferredParallelCountSpinBox->setMinimum(1);
    const auto defaultDeferredTasks = QThread::idealThreadCount() - 2;
    ui->deferredParallelCountSpinBox->setValue((defaultDeferredTasks >= 2) ? defaultDeferredTasks : 2);

    // Render the initial codec properties, now that every control exists. Without this the
    // widgets keep whatever the .ui file set until the user picks a different codec, which
    // leaves the lossless checkbox editable for codecs that are always lossless.
    setCodecProps(m_codecProps);
}

RecorderSettingsDialog::~RecorderSettingsDialog()
{
    delete ui;
}

bool RecorderSettingsDialog::videoNameFromSource() const
{
    return ui->nameFromSrcCheckBox->isChecked();
}

void RecorderSettingsDialog::setVideoNameFromSource(bool fromSource)
{
    ui->nameFromSrcCheckBox->setChecked(fromSource);
}

void RecorderSettingsDialog::setVideoName(const QString &value)
{
    m_videoName = simplifyStrForFileBasename(value, false);
    ui->nameLineEdit->setText(m_videoName);
}

QString RecorderSettingsDialog::videoName() const
{
    return m_videoName;
}

void RecorderSettingsDialog::setSaveTimestamps(bool save)
{
    ui->timestampFileCheckBox->setChecked(save);
}

bool RecorderSettingsDialog::saveTimestamps() const
{
    return ui->timestampFileCheckBox->isChecked();
}

CodecProperties RecorderSettingsDialog::codecProps() const
{
    return m_codecProps;
}

void RecorderSettingsDialog::setCodecProps(CodecProperties props)
{
    m_codecProps = props;

    // select codec in UI
    for (int i = 0; i < ui->codecComboBox->count(); i++) {
        if (ui->codecComboBox->itemData(i).value<VideoCodec>() == props.codec()) {
            if (ui->codecComboBox->currentIndex() == i)
                break;
            ui->codecComboBox->setCurrentIndex(i);
            break;
        }
    }

    // set render node
    for (int i = 0; i < ui->renderNodeComboBox->count(); i++) {
        if (ui->renderNodeComboBox->itemData(i).value<QString>() == props.renderNode()) {
            if (ui->renderNodeComboBox->currentIndex() == i)
                break;
            ui->renderNodeComboBox->setCurrentIndex(i);
            break;
        }
    }

    if (!m_codecProps.allowsAviContainer())
        ui->containerComboBox->setCurrentIndex(0);
    ui->containerComboBox->setEnabled(m_codecProps.allowsAviContainer());

    // change VAAPI option
    if (m_renderNodes.isEmpty()) {
        ui->vaapiCheckBox->setEnabled(false);
        ui->vaapiLabel->setEnabled(false);
        ui->renderNodeLabel->setEnabled(false);
        ui->renderNodeComboBox->setEnabled(false);
    } else {
        ui->vaapiCheckBox->setEnabled(m_codecProps.canUseVaapi());
        ui->vaapiLabel->setEnabled(m_codecProps.canUseVaapi());
        ui->vaapiCheckBox->setChecked(m_codecProps.canUseVaapi() ? m_codecProps.useVaapi() : false);
    }

    // update slicing issue hint
    ui->sliceWarnButton->setVisible(false);
    if (ui->slicingCheckBox->isChecked()) {
        if (!m_codecProps.allowsSlicing())
            ui->sliceWarnButton->setVisible(true);
    }

    // set min/max quality, default bitrate
    if (m_codecProps.qualityMax() < m_codecProps.qualityMin()) {
        ui->qualitySlider->setMaximum(m_codecProps.qualityMax());
        ui->qualitySlider->setMinimum(m_codecProps.qualityMin() * -1);
        ui->qualitySlider->setValue(m_codecProps.quality() * -1);
    } else {
        ui->qualitySlider->setMaximum(m_codecProps.qualityMax());
        ui->qualitySlider->setMinimum(m_codecProps.qualityMin());
        ui->qualitySlider->setValue(m_codecProps.quality());
    }
    ui->bitrateSpinBox->setValue(m_codecProps.bitrateKbps());

    // other properties
    ui->losslessCheckBox->setChecked(m_codecProps.isLossless());
    updateLosslessUiState();
    ui->exactColorsCheckBox->setChecked(m_codecProps.exactColors());
    updateExactColorsUiState();

    ui->brqWidget->setDisabled(ui->losslessCheckBox->isChecked());

    ui->radioButtonBitrate->setChecked(m_codecProps.mode() == CodecProperties::ConstantBitrate);
    on_radioButtonBitrate_toggled(m_codecProps.mode() == CodecProperties::ConstantBitrate);

    // change whether deferred encoding is possible
    ui->encodeAfterRunCheckBox->setEnabled(true);
    if (m_codecProps.codec() == VideoCodec::Raw) {
        // deferred encoding makes no sense for "Raw" video, as there would be no
        // encoding step that could be deferred
        ui->encodeAfterRunCheckBox->setChecked(false);
        ui->encodeAfterRunCheckBox->setEnabled(false);
    }
    ui->encodeAfterRunLabel->setEnabled(ui->encodeAfterRunCheckBox->isEnabled());
}

void RecorderSettingsDialog::setVideoContainer(const VideoContainer &container)
{
    for (int i = 0; i < ui->containerComboBox->count(); i++) {
        if (ui->containerComboBox->itemData(i).value<VideoContainer>() == container) {
            ui->containerComboBox->setCurrentIndex(i);
            break;
        }
    }
}

VideoContainer RecorderSettingsDialog::videoContainer() const
{
    return ui->containerComboBox->currentData().value<VideoContainer>();
}

bool RecorderSettingsDialog::slicingEnabled() const
{
    return ui->slicingCheckBox->isChecked();
}

void RecorderSettingsDialog::setSlicingEnabled(bool enabled)
{
    ui->slicingCheckBox->setChecked(enabled);
}

void RecorderSettingsDialog::setSliceInterval(uint interval)
{
    ui->sliceIntervalSpinBox->setValue(static_cast<int>(interval));
}

uint RecorderSettingsDialog::sliceInterval() const
{
    return static_cast<uint>(ui->sliceIntervalSpinBox->value());
}

bool RecorderSettingsDialog::startStopped() const
{
    return ui->startStoppedCheckBox->isChecked();
}

void RecorderSettingsDialog::setStartStopped(bool startStopped)
{
    ui->startStoppedCheckBox->setChecked(startStopped);
}

bool RecorderSettingsDialog::deferredEncoding()
{
    return ui->encodeAfterRunCheckBox->isChecked();
}

void RecorderSettingsDialog::setDeferredEncoding(bool enabled)
{
    ui->encodeAfterRunCheckBox->setChecked(enabled);
}

bool RecorderSettingsDialog::deferredEncodingInstantStart()
{
    return ui->deferredInstantEncodeCheckBox->isChecked();
}

void RecorderSettingsDialog::setDeferredEncodingInstantStart(bool enabled)
{
    ui->deferredInstantEncodeCheckBox->setChecked(enabled);
}

int RecorderSettingsDialog::deferredEncodingParallelCount()
{
    return ui->deferredParallelCountSpinBox->value();
}

void RecorderSettingsDialog::setDeferredEncodingParallelCount(int count)
{
    ui->deferredParallelCountSpinBox->setValue(count);
}

void RecorderSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_videoName = simplifyStrForFileBasename(arg1, false);
}

void RecorderSettingsDialog::on_codecComboBox_currentIndexChanged(int)
{
    const auto codec = ui->codecComboBox->currentData().value<VideoCodec>();
    if (codec == m_codecProps.codec())
        return;

    // always prefer the Matroska container
    ui->containerComboBox->setCurrentIndex(0);

    CodecProperties tmpCP(codec);
    setCodecProps(tmpCP);
}

void RecorderSettingsDialog::on_nameFromSrcCheckBox_toggled(bool checked)
{
    ui->nameLineEdit->setEnabled(!checked);
}

void RecorderSettingsDialog::updateLosslessUiState()
{
    // VA-API encoders have no lossless mode, so that option must not be selectable
    // while hardware acceleration is active
    const bool vaapiActive = ui->vaapiCheckBox->isEnabled() && ui->vaapiCheckBox->isChecked();

    if (m_codecProps.losslessMode() == CodecProperties::Always) {
        ui->losslessCheckBox->setChecked(true);
        ui->losslessCheckBox->setEnabled(false);
    } else if (m_codecProps.losslessMode() == CodecProperties::Never || vaapiActive) {
        ui->losslessCheckBox->setChecked(false);
        ui->losslessCheckBox->setEnabled(false);
    } else {
        ui->losslessCheckBox->setEnabled(true);
    }
    ui->losslessLabel->setEnabled(ui->losslessCheckBox->isEnabled());
}

void RecorderSettingsDialog::on_losslessCheckBox_toggled(bool checked)
{
    m_codecProps.setLossless(checked);
    ui->brqWidget->setDisabled(checked);

    // Exact colors are only worth their size and speed penalty when the rest of the
    // recording is lossless too, so follow that setting by default. The user can still
    // override the result afterwards.
    ui->exactColorsCheckBox->setChecked(m_codecProps.isLossless());
    updateExactColorsUiState();
}

void RecorderSettingsDialog::on_exactColorsCheckBox_toggled(bool checked)
{
    m_codecProps.setExactColors(checked);
    updateExactColorsUiState();
}

void RecorderSettingsDialog::on_containerComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    // whether raw RGB frames can be stored depends on the container
    updateExactColorsUiState();
}

void RecorderSettingsDialog::updateExactColorsUiState()
{
    const bool supported = m_codecProps.isLossless() && !m_codecProps.useVaapi()
                           && videoCodecCanStoreExactColors(m_codecProps.codec(), videoContainer());

    ui->exactColorsCheckBox->setEnabled(supported);
    ui->exactColorsLabel->setEnabled(supported);

    // Show what the recording will actually do: a combination that can not store exact
    // colors must not display a ticked box. The preference itself is left untouched, so
    // it comes back when the user selects a codec or container that supports it again.
    const bool effective = supported && m_codecProps.exactColors();
    if (ui->exactColorsCheckBox->isChecked() != effective) {
        const QSignalBlocker blocker(ui->exactColorsCheckBox);
        ui->exactColorsCheckBox->setChecked(effective);
    }

    ui->exactColorsCheckBox->setToolTip(exactColorsSummary());
}

QString RecorderSettingsDialog::exactColorsSummary() const
{
    // VA-API rules out lossless encoding as well, so name it first - it is the setting
    // the user has to change to get exact colors back
    if (m_codecProps.useVaapi())
        return QStringLiteral(
            "Hardware encoders always convert color frames to YUV 4:2:0, which loses color "
            "detail. Disable VA-API acceleration to store colors exactly.");

    if (!m_codecProps.isLossless())
        return QStringLiteral(
            "Lossy compression discards color information no matter how the frames are stored, "
            "so exact colors are only available for lossless recordings.");

    if (!videoCodecCanStoreExactColors(m_codecProps.codec(), videoContainer())) {
        if (m_codecProps.codec() == VideoCodec::Raw)
            return QStringLiteral(
                "The Matroska container can not store raw RGB frames. Select the AVI container "
                "to keep colors exact, or use the FFV1 codec.");
        return QStringLiteral(
                   "%1 can not store colors exactly, frames are converted to YUV 4:2:0. Use the FFV1 "
                   "codec for bit-exact color recordings.")
            .arg(QString::fromStdString(videoCodecToString(m_codecProps.codec())));
    }

    if (m_codecProps.exactColors())
        return QStringLiteral(
            "Color frames are stored as RGB and are preserved bit-exactly. This roughly doubles "
            "the file size and needs about 40% more CPU time.");
    return QStringLiteral(
        "Color frames are converted to YUV 4:2:0: smaller files and faster encoding, but color "
        "detail is lost permanently.");
}

void RecorderSettingsDialog::on_exactColorsInfoButton_clicked()
{
    QMessageBox::information(
        this,
        QStringLiteral("Information on exact colors"),
        QStringLiteral(
            "<html>"
            "Video codecs usually store colors as brightness plus two color channels, and keep only a quarter of "
            "the color samples (&quot;YUV 4:2:0&quot;). That is hard to see, but it permanently discards color "
            "information, so a recording is never an exact copy of the camera frames &ndash; not even when a "
            "lossless codec is used.<br/><br/>"
            "With this option enabled, color frames are stored as RGB instead and every pixel is preserved "
            "exactly. In exchange, the files are roughly twice as large and encoding needs about 40&#37; more CPU "
            "time, which matters when recording at a high resolution or framerate.<br/><br/>"
            "This setting has no effect on grayscale recordings, as those are never subsampled. Not every codec "
            "can store colors exactly: <b>FFV1</b> is the recommended choice, VP9 and HEVC are able to do it as "
            "well.<br/><br/>"
            "<b>Currently:</b> %1")
            .arg(exactColorsSummary()));
}

void RecorderSettingsDialog::on_vaapiCheckBox_toggled(bool checked)
{
    if (checked)
        ui->vaapiCheckBox->setText(QStringLiteral("(experimental)"));
    else
        ui->vaapiCheckBox->setText(QStringLiteral(" "));
    ui->renderNodeLabel->setEnabled(checked);
    ui->renderNodeComboBox->setEnabled(checked);

    if (m_codecProps.canUseVaapi())
        m_codecProps.setUseVaapi(checked);

    updateLosslessUiState();
    updateExactColorsUiState();
}

void RecorderSettingsDialog::on_renderNodeComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    const auto renderNode = ui->renderNodeComboBox->currentData().value<QString>();
    if (renderNode == m_codecProps.renderNode())
        return;

    m_codecProps.setRenderNode(renderNode);
}

void RecorderSettingsDialog::on_sliceWarnButton_clicked()
{
    QMessageBox::information(
        this,
        QStringLiteral("Codec slicing warning"),
        QStringLiteral(
            "Some codecs (such as the currently selected one) require a bunch of input frames to initialize "
            "before they can produce an output frame. "
            "Since by slicing the data we need to re-initialize the video encoding for each new file, some "
            "frames may be lost when a new slice is started.\n"
            "This is usually only a very small quantity, but depending on the video's purpose and "
            "framerate, it may be noticeable and could be an issue.\n"
            "Please verify if this is an issue for you, and if it is, consider creating bigger slices, not "
            "using slicing or choosing a different codec."));
}

void RecorderSettingsDialog::on_deferredEncodeWarnButton_clicked()
{
    QMessageBox::information(
        this,
        QStringLiteral("Information on deferred encoding"),
        QStringLiteral(
            "<html>"
            "In order to free up CPU and I/O resources while the experiment is running, Syntalos can perform the "
            "expensive video "
            "encoding step after the experiment is done. This is especially useful if GPU-accelerated encoding can not "
            "be used, "
            "or a slower codec is in use.<br/>"
            "Encoding can run in the background, or be run in batch after many experiments have completed.<br/>"
            "However, during the recording the video data will be saved <b>uncompressed</b> and may exist on disk "
            "twice while encoding is ongoing. "
            "This effect is multiplied when more videos are encoded in parallel. Please ensure that you have <b>excess "
            "diskspace</b> available "
            "when using this option!"));
}

void RecorderSettingsDialog::on_encodeAfterRunCheckBox_toggled(bool checked)
{
    ui->deferredInstantEncodeCheckBox->setEnabled(checked);
    ui->deferredParallelCountSpinBox->setEnabled(checked);
    ui->startEncodingImmediatelyLabel->setEnabled(checked);
    ui->parallelTasksLabel->setEnabled(checked);
}

void RecorderSettingsDialog::on_slicingCheckBox_toggled(bool checked)
{
    ui->sliceWarnButton->setVisible(false);
    if (checked) {
        if (!m_codecProps.allowsSlicing())
            ui->sliceWarnButton->setVisible(true);
    }
    ui->sliceIntervalSpinBox->setEnabled(checked);
    ui->sliceWarnButton->setEnabled(checked);
}

void RecorderSettingsDialog::on_qualitySlider_valueChanged(int value)
{
    auto realVal = value;
    if (m_codecProps.qualityMax() < m_codecProps.qualityMin())
        realVal = value * -1;
    m_codecProps.setQuality(realVal);
    ui->qualityValLabel->setText(QString::number(realVal));
}

void RecorderSettingsDialog::on_bitrateSpinBox_valueChanged(int b)
{
    m_codecProps.setBitrateKbps(b);
}

void RecorderSettingsDialog::on_radioButtonBitrate_toggled(bool checked)
{
    ui->qualityValWidget->setEnabled(!checked);
    if (checked)
        m_codecProps.setMode(CodecProperties::ConstantBitrate);
    else
        m_codecProps.setMode(CodecProperties::ConstantQuality);
}
