/**
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef RECORDERSETTINGSDIALOG_H
#define RECORDERSETTINGSDIALOG_H

#include <QDialog>
#include <QList>

#include "videowriter.h"

namespace Ui
{
class RecorderSettingsDialog;
}

class RecorderSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecorderSettingsDialog(QWidget *parent = nullptr);
    ~RecorderSettingsDialog();

    bool videoNameFromSource() const;
    void setVideoNameFromSource(bool fromSource);

    QString videoName() const;
    void setVideoName(const QString &value);

    void setSaveTimestamps(bool save);
    bool saveTimestamps() const;

    CodecProperties codecProps() const;
    void setCodecProps(CodecProperties props);

    void setVideoContainer(const VideoContainer &container);
    VideoContainer videoContainer() const;

    bool slicingEnabled() const;
    void setSlicingEnabled(bool enabled);

    void setSliceInterval(uint interval);
    uint sliceInterval() const;

    bool startStopped() const;
    void setStartStopped(bool startStopped);

    bool deferredEncoding();
    void setDeferredEncoding(bool enabled);

    bool deferredEncodingInstantStart();
    void setDeferredEncodingInstantStart(bool enabled);

    int deferredEncodingParallelCount();
    void setDeferredEncodingParallelCount(int count);

private slots:
    void on_codecComboBox_currentIndexChanged(int index);
    void on_nameLineEdit_textChanged(const QString &arg1);
    void on_nameFromSrcCheckBox_toggled(bool checked);
    void on_losslessCheckBox_toggled(bool checked);
    void on_vaapiCheckBox_toggled(bool checked);
    void on_renderNodeComboBox_currentIndexChanged(int index);

    void on_slicingCheckBox_toggled(bool checked);
    void on_sliceWarnButton_clicked();

    void on_deferredEncodeWarnButton_clicked();
    void on_encodeAfterRunCheckBox_toggled(bool checked);

    void on_qualitySlider_valueChanged(int value);
    void on_bitrateSpinBox_valueChanged(int arg1);
    void on_radioButtonBitrate_toggled(bool checked);

private:
    Ui::RecorderSettingsDialog *ui;

    QString m_videoName;
    CodecProperties m_codecProps;
    QMap<QString, QString> m_renderNodes;
};

#endif // RECORDERSETTINGSDIALOG_H
