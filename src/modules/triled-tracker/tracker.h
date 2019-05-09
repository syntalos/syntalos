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

#ifndef TRACKER_H
#define TRACKER_H

#include "mazevideo.h"
#include "../barrier.h"


class Tracker : public QObject
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

    explicit Tracker(Barrier barrier, GenericCamera *cam, int framerate, const QString &exportDir, const QString& frameBasePath,
                     const QString &subjectId, const QSize& exportRes);

    bool running() const;

signals:
    void finished(bool success, const QString& errorMessage);

    void newFrame(time_t time, const cv::Mat image);
    void newTrackingFrame(time_t time, const cv::Mat image);
    void newInfoGraphic(const cv::Mat image);

public slots:
    void runTracking();
    void runRecordingOnly();

    void stop();

private:
    void emitError(const QString& msg);
    void emitFinishedSuccess();
    void waitOnBarrier();

    LEDTriangle trackPoints(time_t time, const cv::Mat& image);

    void storeFrameAndWait(const QPair<time_t, cv::Mat> &frame, const QString& frameBasePath,
                           time_t *lastFrameTime, const time_t& timeSinceStart, const time_t &frameInterval);

    Barrier m_barrier;
    GenericCamera *m_camera;
    time_t m_startTime;
    int m_framerate;

    QString m_subjectId;
    QString m_exportDir;
    QString m_frameBasePath;
    QSize m_exportResolution;

    std::vector<cv::Point2f> m_mazeRect;
    uint m_mazeFindTrialCount;
    cv::Mat m_mouseGraphicMat;

    bool m_running;
};

#endif // TRACKER_H
