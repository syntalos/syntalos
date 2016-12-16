/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef VIDEOTRACKER_H
#define VIDEOTRACKER_H

#include <QObject>
#include <QList>
#include <QSize>
#include <QPoint>
#include <opencv2/core/core.hpp>

class UEyeCamera;


class VideoTracker : public QObject
{
    Q_OBJECT
public:
    struct LEDTriangle
    {
        cv::Point red; // a
        cv::Point green; // b
        cv::Point blue; // c

        cv::Point2f center;

        double gamma; // angle at c
        double turnAngle; // triangle turn angle
    };

    explicit VideoTracker(QObject *parent = 0);

    QString lastError() const;

    QList<QPair<QString, int>> getCameraList() const;

    void setResolution(const QSize& size);
    void setCameraId(int cameraId);
    int cameraId() const;

    void setFramerate(int fps);
    int framerate() const;

    void setDataLocation(const QString& dir);
    void setMouseId(const QString& mid);

    void setAutoGain(bool enabled);

    QList<QSize> resolutionList(int cameraId);

    void setExportResolution(const QSize& size);
    QSize exportResolution() const;

    void setExposureTime(double value);

    bool openCamera();
    bool closeCamera();

    void setUEyeConfigFile(const QString& fileName);
    QString uEyeConfigFile() const;

    bool makeFrameTarball();
public slots:
    void run();
    void stop();

    void triggerRecording();

signals:
    void error(const QString& message);
    void finished();
    void newFrame(time_t time, const cv::Mat& image);
    void newTrackingFrame(time_t time, const cv::Mat& image);

    void newInfoGraphic(const cv::Mat& image);

    void progress(int max, int value);

    void recordingReady();

private:
    void emitErrorFinished(const QString &message);
    LEDTriangle trackPoints(time_t time, const cv::Mat& image);

    QString m_lastError;

    time_t m_startTime;
    QSize m_resolution;
    int m_framerate;
    QSize m_exportResolution;
    double m_exposureTime;

    int m_cameraId;
    bool m_autoGain;

    bool m_running;
    bool m_triggered;

    QString m_mouseId;
    QString m_exportDir;

    UEyeCamera *m_camera;

    QString m_uEyeConfigFile;

    std::vector<cv::Point2f> m_mazeRect;
    uint m_mazeFindTrialCount;

    cv::Mat m_mouseGraphicMat;
};

#endif // VIDEOTRACKER_H
