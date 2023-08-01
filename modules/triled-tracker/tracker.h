/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef TRACKER_H
#define TRACKER_H

#include <QFile>
#include <opencv2/imgproc.hpp>

#include "streams/frametype.h"

class Tracker : public QObject
{
    Q_OBJECT
public:
    struct LEDTriangle {
        cv::Point red;   // a
        cv::Point green; // b
        cv::Point blue;  // c

        cv::Point2f center;

        double gamma;     // angle at c
        double turnAngle; // triangle turn angle
    };

    explicit Tracker(std::shared_ptr<DataStream<TableRow>> dataStream, const QString &subjectId);
    ~Tracker();

    QString lastError() const;

    bool initialize();
    void analyzeFrame(const cv::Mat &frame, const milliseconds_t time, cv::Mat *trackingFrame, cv::Mat *infoFrame);
    QVariantHash finalize();

private:
    void setError(const QString &msg);

    LEDTriangle trackPoints(const cv::Mat &image, cv::Mat *infoFrame, cv::Mat *trackingFrame);

    bool m_initialized;
    bool m_firstFrame;
    QString m_lastError;

    QString m_subjectId;
    std::shared_ptr<DataStream<TableRow>> m_dataStream;

    std::vector<cv::Point2f> m_mazeRect;
    uint m_mazeFindTrialCount;
    cv::Mat m_mouseGraphicMat;
};

#endif // TRACKER_H
