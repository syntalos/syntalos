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

#include "utils.h"

RecorderSettingsDialog::RecorderSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RecorderSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    ui->containerComboBox->addItem("MKV", QVariant::fromValue(VideoContainer::Matroska));
    ui->containerComboBox->addItem("AVI", QVariant::fromValue(VideoContainer::AVI));
    ui->containerComboBox->setCurrentIndex(0);

    ui->codecComboBox->addItem("FFV1", QVariant::fromValue(VideoCodec::FFV1));
    ui->codecComboBox->addItem("AV1", QVariant::fromValue(VideoCodec::AV1));
    ui->codecComboBox->addItem("VP9", QVariant::fromValue(VideoCodec::VP9));
    ui->codecComboBox->addItem("H.264", QVariant::fromValue(VideoCodec::H264));
    ui->codecComboBox->addItem("HEVC", QVariant::fromValue(VideoCodec::HEVC));
    ui->codecComboBox->addItem("Raw", QVariant::fromValue(VideoCodec::Raw));
    ui->codecComboBox->setCurrentIndex(0);

    // take name from source module by default
    ui->nameFromSrcCheckBox->setChecked(true);
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
    m_videoName = simplifyStringForFilebasename(value);
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

void RecorderSettingsDialog::setVideoCodec(const VideoCodec& codec)
{
    for (int i = 0; i < ui->codecComboBox->count(); i++) {
        if (ui->codecComboBox->itemData(i).value<VideoCodec>() == codec) {
            ui->codecComboBox->setCurrentIndex(i);
            break;
        }
    }
}

VideoCodec RecorderSettingsDialog::videoCodec() const
{
    return ui->codecComboBox->currentData().value<VideoCodec>();
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

void RecorderSettingsDialog::setLossless(bool lossless)
{
    ui->losslessCheckBox->setChecked(lossless);
}

bool RecorderSettingsDialog::isLossless() const
{
    return ui->losslessCheckBox->isChecked();
}

void RecorderSettingsDialog::setSliceInterval(uint interval)
{
    ui->sliceIntervalSpinBox->setValue(static_cast<int>(interval));
}

uint RecorderSettingsDialog::sliceInterval() const
{
    return static_cast<uint>(ui->sliceIntervalSpinBox->value());
}

void RecorderSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_videoName = simplifyStringForFilebasename(arg1);
}

void RecorderSettingsDialog::on_codecComboBox_currentIndexChanged(int)
{
    // reset state of lossless infobox
    ui->losslessCheckBox->setEnabled(true);
    ui->losslessLabel->setEnabled(true);
    ui->losslessCheckBox->setChecked(true);
    ui->containerComboBox->setEnabled(true);

    const auto codec = ui->codecComboBox->currentData().value<VideoCodec>();

    if (codec == VideoCodec::FFV1) {
        // FFV1 is always lossless
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(true);

    } else if ((codec == VideoCodec::H264) || (codec == VideoCodec::HEVC)) {
        // H.264 and HEVC only work with MKV and MP4 containers, select MKV by default
        ui->containerComboBox->setCurrentIndex(0);
        ui->containerComboBox->setEnabled(false);

    } else if (codec == VideoCodec::VP9) {
        // VP9 only seems to work well in MKV containers, so select those
        ui->containerComboBox->setCurrentIndex(0);
        ui->containerComboBox->setEnabled(false);

    } else if (codec == VideoCodec::MPEG4) {
        // MPEG-4 can't do lossless encoding
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(false);

    } else if (codec == VideoCodec::Raw) {
        // Raw is always lossless
        ui->losslessCheckBox->setEnabled(false);
        ui->losslessLabel->setEnabled(false);
        ui->losslessCheckBox->setChecked(true);

        // Raw RGB only works with AVI containers
        ui->containerComboBox->setCurrentIndex(1);
        ui->containerComboBox->setEnabled(false);
    }
}

void RecorderSettingsDialog::on_nameFromSrcCheckBox_toggled(bool checked)
{
    ui->nameLineEdit->setEnabled(!checked);
}
