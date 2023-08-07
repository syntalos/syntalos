/**
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "tracker.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

Tracker::Tracker(std::shared_ptr<DataStream<TableRow>> dataStream, const QString &subjectId)
    : QObject(nullptr),
      m_initialized(false),
      m_subjectId(subjectId),
      m_dataStream(dataStream)
{
    // load mouse graphic from resource store
    QFile file(":/images/mouse-top.png");
    if (file.open(QIODevice::ReadOnly)) {
        auto sz = file.size();
        std::vector<uchar> buf(static_cast<size_t>(sz));
        file.read((char *)buf.data(), sz);
        m_mouseGraphicMat = cv::imdecode(buf, cv::IMREAD_COLOR);
    } else {
        qCritical() << "Unable to load mouse image from internal resources.";
    }
}

Tracker::~Tracker()
{
    finalize();
}

QString Tracker::lastError() const
{
    return m_lastError;
}

void Tracker::setError(const QString &msg)
{
    m_lastError = msg;
}

bool Tracker::initialize()
{
    if (m_initialized) {
        setError("Tried to initialize tracker twice.");
        return false;
    }

    // set position header and start the output data stream
    m_dataStream->setMetadataValue(
        "table_header",
        QStringList() << QStringLiteral("Time") << QStringLiteral("Red X") << QStringLiteral("Red Y")
                      << QStringLiteral("Green X") << QStringLiteral("Green Y") << QStringLiteral("Blue X")
                      << QStringLiteral("Blue Y") << QStringLiteral("Center X") << QStringLiteral("Center Y")
                      << QStringLiteral("Turn Angle (deg)"));
    m_dataStream->start();

    // clear maze position data
    m_mazeRect = std::vector<cv::Point2f>();
    m_mazeFindTrialCount = 0;

    m_firstFrame = true;
    m_initialized = true;
    return true;
}

void Tracker::analyzeFrame(const cv::Mat &frame, const milliseconds_t time, cv::Mat *trackingFrame, cv::Mat *infoFrame)
{
    // do the tracking on the source frame
    auto triangle = trackPoints(frame, infoFrame, trackingFrame);

    TableRow posInfo;
    posInfo.reserve(10);

    // store time value
    posInfo.append(QString::number(time.count()));

    // red
    posInfo.append(QString::number(triangle.red.x));
    posInfo.append(QString::number(triangle.red.y));

    // green
    posInfo.append(QString::number(triangle.green.x));
    posInfo.append(QString::number(triangle.green.y));

    // blue
    posInfo.append(QString::number(triangle.blue.x));
    posInfo.append(QString::number(triangle.blue.y));

    // center
    posInfo.append(QString::number(triangle.center.x));
    posInfo.append(QString::number(triangle.center.y));

    // turn angle
    posInfo.append(QString::number(triangle.turnAngle));

    m_dataStream->push(posInfo);
}

QVariantHash Tracker::finalize()
{
    if (!m_initialized)
        return QVariantHash();

    QVariantHash mazeInfo;
    if (m_mazeRect.size() == 4) {
        mazeInfo.insert("top_left_x", m_mazeRect[0].x);
        mazeInfo.insert("top_left_y", m_mazeRect[0].y);

        mazeInfo.insert("top_right_x", m_mazeRect[1].x);
        mazeInfo.insert("top_right_y", m_mazeRect[1].y);

        mazeInfo.insert("bottom_left_x", m_mazeRect[2].x);
        mazeInfo.insert("bottom_left_y", m_mazeRect[2].y);

        mazeInfo.insert("bottom_right_x", m_mazeRect[3].x);
        mazeInfo.insert("bottom_right_y", m_mazeRect[3].y);
    }

    return mazeInfo;
}

static std::vector<cv::Point2f> findCornerBlobs(const cv::Mat &grayMat)
{
    cv::Mat blurMap(grayMat.size(), grayMat.type());

    // remove noise aggressively
    cv::morphologyEx(grayMat, blurMap, cv::MORPH_CLOSE, cv::Mat::ones(32, 32, blurMap.type()));

    // set blob detector parameters
    cv::SimpleBlobDetector::Params params;
    params.filterByArea = true;
    params.filterByCircularity = false;
    params.filterByColor = false;
    params.filterByConvexity = false;
    params.minArea = static_cast<float>(blurMap.size().width) / 4;
    params.maxArea = static_cast<float>(blurMap.size().width) / 2;
    params.minDistBetweenBlobs = static_cast<float>(blurMap.size().width) / 32;
    params.minThreshold = 8;

    // detect blobs
    cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(blurMap, keypoints);

    std::vector<cv::Point2f> mazeRect(4);
    // check if we have enough keypoints for a rectangle
    if (keypoints.size() < 4)
        return std::vector<cv::Point2f>();

    // sort points by being close to the top and being large
    std::sort(keypoints.begin(), keypoints.end(), [&](cv::KeyPoint a, cv::KeyPoint b) {
        return (a.pt.y * (a.size / 2)) < (b.pt.y * (b.size / 2));
    });

    // position tl and tr coordinates
    if (keypoints[0].pt.x < keypoints[1].pt.x) {
        mazeRect[0] = keypoints[0].pt;
        mazeRect[1] = keypoints[1].pt;
    } else {
        mazeRect[0] = keypoints[1].pt;
        mazeRect[1] = keypoints[0].pt;
    }

    // sort points by being close to the bottom
    std::sort(keypoints.begin(), keypoints.end(), [&](cv::KeyPoint a, cv::KeyPoint b) {
        return (a.pt.y * (a.size / 2)) > (b.pt.y * (b.size / 2));
    });

    // position bl and br coordinates
    if (keypoints[0].pt.x < keypoints[1].pt.x) {
        mazeRect[2] = keypoints[0].pt;
        mazeRect[3] = keypoints[1].pt;
    } else {
        mazeRect[2] = keypoints[1].pt;
        mazeRect[3] = keypoints[0].pt;
    }

#if 0
    cv::Mat im_with_keypoints;
    cv::drawKeypoints(binaryMat, keypoints, im_with_keypoints, cv::Scalar(0, 0, 255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

    // draw polygon
    for( int j = 0; j < 4; j++ ) {
        cv::line(im_with_keypoints, mazeRect[j],  mazeRect[(j+1)%4], cv::Scalar(255), 3, 8);
    }

    // Show blobs
    cv::imshow("keypoints", im_with_keypoints);
#endif

    // sanity check
    if (mazeRect[0] == mazeRect[3])
        return std::vector<cv::Point2f>();

    return mazeRect;
}

static cv::Point findMaxColorBrightness(
    const cv::Mat &image,
    const cv::Mat &imageGray,
    cv::Scalar minColors,
    cv::Scalar maxColors)
{
    double maxVal;
    cv::Point maxLoc;
    cv::Mat colorMaskMat(image.size(), image.type());
    cv::Mat colorMat(image.size(), image.type());

    // create matrix with pixels in range
    cv::inRange(image, minColors, maxColors, colorMaskMat);
    // remove noise
    cv::morphologyEx(colorMaskMat, colorMaskMat, cv::MORPH_CLOSE, cv::Mat::ones(6, 6, colorMaskMat.type()));

    // find maximum
    imageGray.copyTo(colorMat, colorMaskMat);
    cv::minMaxLoc(
        colorMat,
        nullptr, // minimum value
        &maxVal,
        nullptr, // minimum location (Point),
        &maxLoc,
        cv::Mat());
    if (((maxLoc.x == 0) && (maxLoc.y == 0)) && (maxVal == 0)) {
        // we didn't find a maximum, our tracking dot vanished. Set an invalid point.
        maxLoc.x = -1;
        maxLoc.y = -1;
    }

    return maxLoc;
}

static double calculateTriangleGamma(Tracker::LEDTriangle &tri)
{
    // sanity checks
    if ((tri.red.x < 0) || (tri.red.y < 0) || (tri.green.x < 0) || (tri.green.x < 0)) {
        tri.gamma = -1;
        return -1;
    }
    if (tri.blue.x < 0) {
        if (tri.blue.y < 0) {
            // looks like we haven't found the LED triangle at all...
            tri.gamma = -1;
            return -1;
        }
        // this means the mouse has tilted its head so much that the blue LED isn't
        // visible anymore, which would amount to a 180° "flat" gamma angle
        tri.gamma = 180;
        return tri.gamma;
    }

    // calculate triangle side lengths
    auto cLen = qSqrt(qPow((tri.red.x - tri.green.x), 2) + qPow((tri.red.y - tri.green.y), 2));
    auto bLen = qSqrt(qPow((tri.red.x - tri.blue.x), 2) + qPow((tri.red.y - tri.blue.y), 2));
    auto aLen = qSqrt(qPow((tri.green.x - tri.blue.x), 2) + qPow((tri.green.y - tri.blue.y), 2));

    // calculate gamma angle at the blue LED
    auto gamma = qAcos((qPow(bLen, 2) + qPow(aLen, 2) - qPow(cLen, 2)) / (2 * aLen * bLen));
    tri.gamma = qRadiansToDegrees(gamma);

    return tri.gamma;
}

static cv::Point2f calculateTriangleCentroid(Tracker::LEDTriangle &tri)
{
    auto x = (tri.red.x + tri.green.x + tri.blue.x) / 3;
    auto y = (tri.red.y + tri.green.y + tri.blue.y) / 3;

    return cv::Point2f(x, y);
}

static double calculateTriangleTurnAngle(Tracker::LEDTriangle &tri)
{
    if (tri.red.x <= 0 && tri.green.x <= 0 && tri.blue.x <= 0) {
        // looks like we don't know where the triangle is
        tri.center.x = -1;
        tri.center.y = -1;
        tri.turnAngle = -1;
        return -1;
    }

    tri.center = calculateTriangleCentroid(tri);

    // create two vectors, one from the centroid to the blue LED and one pointing straight
    // to the X axis.
    auto a = cv::Vec2d(tri.center.x - tri.blue.x, tri.center.y - tri.blue.y);
    auto b = cv::Vec2d(0, tri.center.y);

    // dot product formula
    auto abDot = a.dot(b);

    auto aLen = qSqrt(qPow(a[0], 2) + qPow(a[1], 2));
    auto bLen = qSqrt(qPow(b[0], 2) + qPow(b[1], 2));

    auto angle = qRadiansToDegrees(qAcos(abDot / (aLen * bLen)));

    // correct the angle
    if (tri.center.x < tri.blue.x)
        angle = 360 - angle;

    tri.turnAngle = angle;
    return angle;
}

static bool cvRectFuzzyEqual(
    const std::vector<cv::Point2f> &a,
    const std::vector<cv::Point2f> &b,
    const uint tolerance = 2)
{
    if (a.size() != 4)
        return false;
    if (b.size() != 4)
        return false;

    for (uint i = 0; i < 4; i++) {
        if (a[i].x - b[i].x > tolerance)
            return false;

        if (a[i].y - b[i].y > tolerance)
            return false;
    }

    return true;
}

Tracker::LEDTriangle Tracker::trackPoints(const cv::Mat &image, cv::Mat *infoFrame, cv::Mat *trackingFrame)
{
    cv::Point maxLoc;
    LEDTriangle res;

    cv::Mat trackMat(image.size(), image.type());
    cv::Mat grayMat(image.size(), image.type());

    cv::cvtColor(image, grayMat, cv::COLOR_RGB2GRAY);
    cv::cvtColor(grayMat, trackMat, cv::COLOR_GRAY2RGBA);

    // colors are in BGR

    // red maximum
    maxLoc = findMaxColorBrightness(image, grayMat, cv::Scalar(0, 0, 180), cv::Scalar(80, 80, 255));
    if (maxLoc.x > 0)
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(0, 0, 255), -1); // BGR colors
    res.red = maxLoc;

    // green maximum
    maxLoc = findMaxColorBrightness(image, grayMat, cv::Scalar(0, 220, 0), cv::Scalar(110, 255, 180));
    if (maxLoc.x > 0)
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(0, 255, 0), -1); // BGR colors
    res.green = maxLoc;

    // blue maximum
    maxLoc = findMaxColorBrightness(image, grayMat, cv::Scalar(210, 0, 0), cv::Scalar(255, 240, 70));
    if (maxLoc.x > 0)
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(255, 0, 0), -1); // BGR colors
    res.blue = maxLoc;

    // calculate gamma angle
    res.gamma = calculateTriangleGamma(res);
    if (res.gamma > 0)
        cv::putText(
            trackMat,
            QStringLiteral("y%1").arg(res.gamma).toStdString(),
            cv::Point(res.blue.x + 7, res.blue.y + 7),
            cv::FONT_HERSHEY_SIMPLEX,
            0.6,
            cv::Scalar(100, 100, 255));

    // find the maze
    if (m_mazeRect.size() == 4) {
        // draw maze rect
        cv::line(trackMat, m_mazeRect[0], m_mazeRect[1], cv::Scalar(40, 120, 120), 2);
        cv::line(trackMat, m_mazeRect[2], m_mazeRect[3], cv::Scalar(40, 120, 120), 2);

        cv::line(trackMat, m_mazeRect[0], m_mazeRect[2], cv::Scalar(40, 120, 120), 2);
        cv::line(trackMat, m_mazeRect[1], m_mazeRect[3], cv::Scalar(40, 120, 120), 2);

        // we need to try to find the maze a few times, to not make assumptions based
        // on a bad initial image delivered by the camera warming up.
        if (m_mazeFindTrialCount < 5) {
            auto rect = findCornerBlobs(grayMat);
            if (cvRectFuzzyEqual(rect, m_mazeRect)) {
                m_mazeRect = rect;
                m_mazeFindTrialCount++;
            } else {
                m_mazeFindTrialCount = 0;
                m_mazeRect = rect;
            }
        }
    } else {
        m_mazeFindTrialCount = 0;
        // try to find maze position if we don't know it already
        m_mazeRect = findCornerBlobs(grayMat);
    }

    // calculate mouse turn angle and display it in an infographic
    auto angle = calculateTriangleTurnAngle(res);
    res.turnAngle = angle;

    cv::Mat infoMat(m_mouseGraphicMat.size(), m_mouseGraphicMat.type());

    // rotate mouse image if we have a valid angle
    if (res.turnAngle > 0) {
        cv::Point2f matCenter(m_mouseGraphicMat.cols / 2.0F, m_mouseGraphicMat.rows / 2.0F);
        auto rotMat = cv::getRotationMatrix2D(matCenter, angle, 1.0);

        cv::warpAffine(m_mouseGraphicMat, infoMat, rotMat, m_mouseGraphicMat.size());
    }

    // display position in infographic
    if (res.center.x >= 0) {
        cv::putText(
            infoMat,
            QStringLiteral("X: %2 Y: %3").arg(res.center.x).arg(res.center.y).toStdString(),
            cv::Point(6, 20),
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,
            cv::Scalar(255, 180, 180));
        cv::putText(
            infoMat,
            m_subjectId.toStdString(),
            cv::Point(m_mouseGraphicMat.cols - (m_subjectId.length() * 18) - 6, 20),
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,
            cv::Scalar(255, 180, 180));
    } else {
        infoMat.setTo(cv::Scalar(0, 0, 0));
        cv::putText(
            infoMat,
            "Oh no, we do not know where the test subject is!",
            cv::Point(14, (m_mouseGraphicMat.rows / 2) - 8),
            cv::FONT_HERSHEY_SIMPLEX,
            0.6,
            cv::Scalar(100, 100, 255));
    }

    (*infoFrame) = infoMat;
    (*trackingFrame) = trackMat;

    return res;
}
