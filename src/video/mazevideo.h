/*
 * Copyright (C) 2016-2017 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef MAZEVIDEO_H
#define MAZEVIDEO_H

#include <QObject>
#include <QList>
#include <QSize>
#include <QPoint>
#include <QVariant>
#include <opencv2/core/core.hpp>

#include "../utils.h"
#include "../barrier.h"


class MACamera : public QObject
{
    Q_OBJECT
public:
    explicit MACamera(QObject *parent)
        : QObject(parent) {}
    virtual ~MACamera() {}

    virtual QString lastError() const = 0;

    virtual QList<QPair<QString, QVariant>> getCameraList() const = 0;

    virtual bool open(QVariant cameraId, const QSize& size) = 0;
    virtual bool close() = 0;

    virtual QPair<time_t, cv::Mat> getFrame() = 0;
    virtual bool getFrame(time_t *time, cv::Mat& buffer) = 0;

    virtual bool setAutoWhiteBalance(bool enabled) = 0;
    virtual bool setAutoGain(bool enabled) = 0;
    virtual bool setExposureTime(double val) = 0;
    virtual bool setFramerate(double fps) = 0;

    virtual QList<QSize> getResolutionList(QVariant cameraId) = 0;

    virtual void setConfFile(const QString& fileName) { Q_UNUSED(fileName); }
    virtual bool setGPIOFlash(bool enabled) { Q_UNUSED(enabled); return true; }
};

class Tracker;

class MazeVideo : public QObject
{
    Q_OBJECT
public:
    explicit MazeVideo(QObject *parent = 0);

    QString lastError() const;

    QList<QPair<QString, QVariant>> getCameraList() const;

    void setResolution(const QSize& size);
    void setCameraId(QVariant cameraId);
    QVariant cameraId() const;

    void setFramerate(int fps);
    int framerate() const;

    void setDataLocation(const QString& dir);
    void setSubjectId(const QString& mid);

    void setAutoGain(bool enabled);

    QList<QSize> resolutionList(QVariant cameraId);

    void setExportResolution(const QSize& size);
    QSize exportResolution() const;

    void setExposureTime(double value);

    bool openCamera();

    void setUEyeConfigFile(const QString& fileName);
    QString uEyeConfigFile() const;

    void setGPIOFlash(bool enabled);
    bool gpioFlash() const;

    void setTrackingEnabled(bool enabled);

    bool makeFrameTarball();

public slots:
    void run(Barrier barrier);
    void stop();

signals:
    void error(const QString& message);
    void finished();
    void newFrame(time_t time, const cv::Mat& image);
    void newTrackingFrame(time_t time, const cv::Mat& image);

    void newInfoGraphic(const cv::Mat& image);

    void progress(int max, int value);

protected:
    bool closeCamera();

private:
    void emitErrorFinished(const QString &message);

    QString m_lastError;
    bool m_cameraOpened;

    QSize m_resolution;
    int m_framerate;
    QSize m_exportResolution;
    double m_exposureTime;
    bool m_gpioFlash;

    QVariant m_cameraId;
    bool m_autoGain;

    QString m_subjectId;
    QString m_exportDir;

    MACamera *m_camera;
    Tracker *m_tracker;

    QString m_uEyeConfigFile;

    bool m_trackingEnabled;
};

#endif // MAZEVIDEO_H
