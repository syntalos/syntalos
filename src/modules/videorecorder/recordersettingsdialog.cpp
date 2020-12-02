/**
 * Copyright (C) 2019 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "recordersettingsdialog.h"
#include "ui_recordersettingsdialog.h"

#include <QVariant>
#include <QMessageBox>

#include "utils/misc.h"

RecorderSettingsDialog::RecorderSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RecorderSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    ui->containerComboBox->addItem("MKV", QVariant::fromValue(VideoContainer::Matroska));
    ui->containerComboBox->addItem("AVI", QVariant::fromValue(VideoContainer::AVI));
    ui->containerComboBox->setCurrentIndex(0);

    // We currently only permit a limited set of codecs - less choices are better here.
    // AV1 isn't ready yet for live encoding, and HEVC is slow on most computers as well
    // Currently FFV1 is the best option for lossless encoding, and VP9 is the best choice
    // for lossy encoding

    ui->codecComboBox->addItem("FFV1", QVariant::fromValue(VideoCodec::FFV1));
    //ui->codecComboBox->addItem("AV1", QVariant::fromValue(VideoCodec::AV1));
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

    // no slicing warning by default
    ui->sliceWarnButton->setVisible(false);
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
    m_videoName = simplifyStrForFileBasename(value);
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
            ui->codecComboBox->setCurrentIndex(i);
            break;
        }
    }

    // other properties
    ui->losslessCheckBox->setChecked(m_codecProps.useVaapi());
    ui->vaapiCheckBox->setChecked(m_codecProps.isLossless());
}

void RecorderSettingsDialog::setVideoContainer(const VideoContainer& container)
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

void RecorderSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_videoName = simplifyStrForFileBasename(arg1);
}

void RecorderSettingsDialog::on_codecComboBox_currentIndexChanged(int)
{
    // reset state of lossless infobox
    ui->losslessCheckBox->setEnabled(true);
    ui->losslessCheckBox->setChecked(true);
    ui->containerComboBox->setEnabled(true);

    const auto codec = ui->codecComboBox->currentData().value<VideoCodec>();
    CodecProperties tmpCP(codec);
    m_codecProps = tmpCP;

    // always prefer the Matroska container
    ui->containerComboBox->setCurrentIndex(0);
    ui->containerComboBox->setEnabled(m_codecProps.allowsAviContainer());

    // set lossles UI preferences
    if (m_codecProps.losslessMode() == CodecProperties::Always) {
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessCheckBox->setChecked(true);
    } else if (m_codecProps.losslessMode() == CodecProperties::Never) {
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessCheckBox->setChecked(false);
    } else {
        ui->losslessCheckBox->setEnabled(true);
        ui->losslessCheckBox->setChecked(false);
    }
    // change VAAPI possibility
    ui->vaapiCheckBox->setEnabled(m_codecProps.canUseVaapi());
    ui->vaapiLabel->setEnabled(m_codecProps.canUseVaapi());
    if (!m_codecProps.canUseVaapi())
        ui->vaapiCheckBox->setChecked(false);

    // update slicing issue hint
    ui->sliceWarnButton->setVisible(false);
    if (ui->slicingCheckBox->isChecked()) {
        if (!m_codecProps.allowsSlicing())
            ui->sliceWarnButton->setVisible(true);
    }
}

void RecorderSettingsDialog::on_nameFromSrcCheckBox_toggled(bool checked)
{
    ui->nameLineEdit->setEnabled(!checked);
}

void RecorderSettingsDialog::on_losslessCheckBox_toggled(bool checked)
{
    m_codecProps.setLossless(checked);
    ui->brqWidget->setDisabled(checked);
}

void RecorderSettingsDialog::on_vaapiCheckBox_toggled(bool checked)
{
    if (checked)
        ui->vaapiCheckBox->setText(QStringLiteral("(experimental)"));
    else
        ui->vaapiCheckBox->setText(QStringLiteral(" "));

    if (m_codecProps.canUseVaapi())
        m_codecProps.setUseVaapi(checked);
}

void RecorderSettingsDialog::on_sliceWarnButton_clicked()
{
    QMessageBox::information(this,
                             QStringLiteral("Codec slicing warning"),
                             QStringLiteral("Some codecs (such as the currently selected one) require a bunch of input frames to initialize before they can produce an output frame. "
                                            "Since by slicing the data we need to re-initialize the video encoding for each new file, some frames may be lost when a new slice is started.\n"
                                            "This is usually only a very small quantity, but depending on the video's purpose and framerate, it may be noticeable and could be an issue.\n"
                                            "Please verify if this is an issue for you, and if it is, consider creating bigger slices, not using slicing or choosing a different codec."));
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
