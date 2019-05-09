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

#ifndef RECORDERSETTINGSDIALOG_H
#define RECORDERSETTINGSDIALOG_H

#include <QDialog>
#include <QList>

#include "imagesourcemodule.h"
#include "videowriter.h"

namespace Ui {
class RecorderSettingsDialog;
}

class RecorderSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecorderSettingsDialog(QWidget *parent = nullptr);
    ~RecorderSettingsDialog();

    void setImageSourceModules(const QList<ImageSourceModule*>& mods);

    ImageSourceModule *selectedImageSourceMod();
    void setSelectedImageSourceMod(ImageSourceModule *mod);

    void setVideoName(const QString& value);
    QString videoName() const;

    void setSaveTimestamps(bool save);
    bool saveTimestamps() const;

    void setVideoCodec(const VideoCodec& codec);
    VideoCodec videoCodec() const;

    void setVideoContainer(const VideoContainer &container);
    VideoContainer videoContainer() const;

    void setLossless(bool lossless);
    bool isLossless() const;

    void setSliceInterval(uint interval);
    uint sliceInterval() const;

private slots:
    void on_codecComboBox_currentIndexChanged(int index);

private slots:
    void on_nameLineEdit_textChanged(const QString &arg1);
    void on_frameSourceComboBox_currentIndexChanged(int index);

private:
    Ui::RecorderSettingsDialog *ui;

    ImageSourceModule *m_selectedImgSrcMod;
    QString m_videoName;
};

#endif // RECORDERSETTINGSDIALOG_H
