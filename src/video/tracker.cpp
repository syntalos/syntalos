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

#include "tracker.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QThreadPool>
#include <QTextStream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include "modules/genericcamera/genericcamera.h"

Tracker::Tracker(Barrier barrier, GenericCamera *cam, int framerate, const QString& exportDir, const QString &frameBasePath,
                 const QString& subjectId, const QSize &exportRes)
    : QObject(nullptr),
      m_barrier(barrier),
      m_camera(cam),
      m_framerate(framerate),
      m_subjectId(subjectId),
      m_exportDir(exportDir),
      m_frameBasePath(frameBasePath),
      m_exportResolution(exportRes)
{
    // load mouse graphic from resource store
    QFile file(":/images/mouse-top.png");
    if(file.open(QIODevice::ReadOnly)) {
        qint64 sz = file.size();
        std::vector<uchar> buf(sz);
        file.read((char*) buf.data(), sz);
        m_mouseGraphicMat = cv::imdecode(buf, cv::IMREAD_COLOR);
    } else {
        qCritical() << "Unable to load mouse image from internal resources.";
    }
}

bool Tracker::running() const
{
    return m_running;
}

void Tracker::storeFrameAndWait(const QPair<time_t, cv::Mat> &frame, const QString& frameBasePath,
                                     time_t *lastFrameTime, const time_t& timeSinceStart, const time_t& frameInterval)
{
    // store downscaled frame on disk
    cv::Mat emat;
    cv::resize(frame.second,
               emat,
               cv::Size(m_exportResolution.width(), m_exportResolution.height()));
    cv::imwrite(QString("%1%2.jpg").arg(frameBasePath).arg(timeSinceStart).toStdString(), emat);

    // wait the remaining time before requesting the next frame
    time_t remainingTime;
    if ((*lastFrameTime) != frame.first)
        remainingTime = frameInterval - (frame.first - (*lastFrameTime));
    else
        remainingTime = 0; // we fetched the same frame twice - directly jump to the next one
    (*lastFrameTime) = frame.first;

    if (remainingTime > 2) // > 2 instead of > 0 to really only sleep when there is *much* delay needed
        QThread::usleep(remainingTime * 1000); // sleep remainingTime msecs
}

void Tracker::runTracking()
{
    Q_ASSERT(!m_exportDir.isEmpty());
    Q_ASSERT(!m_subjectId.isEmpty());
    Q_ASSERT(m_camera != nullptr);

    m_running = true;

    // prepare position output CSV file
    auto posInfoPath = QStringLiteral("%1/%2_positions.csv").arg(m_exportDir).arg(m_subjectId);
    QFile posInfoFile(posInfoPath);
    if (!posInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitError("Unable to open position CSV file for writing.");
        return;
    }

    QTextStream posInfoOut(&posInfoFile);
    posInfoOut << "Time (msec)" << ";"
                  "Red X" << ";" << "Red Y" << ";"
                  "Green X" << ";" << "Green Y" << ";"
                  "Blue X" << ";" << "Blue Y" << ";"
                  "Center X" << ";" << "Center Y" << ";"
                  "Turn Angle (deg)" << "\n";

    // clear maze position data
    m_mazeRect = std::vector<cv::Point2f>();
    m_mazeFindTrialCount = 0;

    auto frameInterval = 1000 / m_framerate; // framerate in FPS, interval msec delay
    auto firstFrame = true;
    time_t lastFrameTime = 0;

    // wait for barrier release, if there is any
    waitOnBarrier();

    while (m_running) {
        auto frame = m_camera->getFrame();
        if (frame.first < 0) {
            // we have an invalid frame, ignore it
            continue;
        }

        // assume first frame is starting point
        if (firstFrame) {
            firstFrame = false;
            m_startTime = frame.first;
            lastFrameTime = frame.first - (frameInterval / 1.5);
        }
        auto timeSinceStart = frame.first - m_startTime;
        emit newFrame(timeSinceStart, frame.second);

        // do the tracking on the source frame
        auto triangle = trackPoints(timeSinceStart, frame.second);

        // the layout of the CSV file is:
        //  time;Red X;Red Y; Green X; Green Y; Yellow X; Yellow Y

        // store time value
        posInfoOut << timeSinceStart << ";";

        // red
        posInfoOut << triangle.red.x << ";" << triangle.red.y << ";";
        // green
        posInfoOut << triangle.green.x << ";" << triangle.green.y << ";";
        // blue
        posInfoOut << triangle.blue.x << ";" << triangle.blue.y << ";";
        // center
        posInfoOut << triangle.center.x << ";" << triangle.center.y << ";";

        // turn angle
        posInfoOut << triangle.turnAngle << ";";

        posInfoOut << "\n";

        // enqueue frame to be stored on disk and wait the remaining time before requesting a new frame
        storeFrameAndWait(frame, m_frameBasePath,
                          &lastFrameTime, timeSinceStart, frameInterval);
    }

    m_startTime = 0;

    // store details about our recording
    // get a frame and store details
    auto finalFrame = m_camera->getFrame();

    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_exportDir).arg(m_subjectId);
    QJsonObject vInfo;
    vInfo.insert("frameWidth", finalFrame.second.cols);
    vInfo.insert("frameHeight", finalFrame.second.rows);

    vInfo.insert("exportWidth", m_exportResolution.width());
    vInfo.insert("exportHeight", m_exportResolution.height());

    if (m_mazeRect.size() == 4) {
        QJsonObject mazeInfo;
        mazeInfo.insert("topLeftX", m_mazeRect[0].x);
        mazeInfo.insert("topLeftY", m_mazeRect[0].y);

        mazeInfo.insert("topRightX", m_mazeRect[1].x);
        mazeInfo.insert("topRightY", m_mazeRect[1].y);

        mazeInfo.insert("bottomLeftX", m_mazeRect[2].x);
        mazeInfo.insert("bottomLeftY", m_mazeRect[2].y);

        mazeInfo.insert("bottomRightX", m_mazeRect[3].x);
        mazeInfo.insert("bottomRightY", m_mazeRect[3].y);

        vInfo.insert("mazePos", mazeInfo);
    }

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitError("Unable to open video info file for writing.");
        return;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    emitFinishedSuccess();
}

void Tracker::runRecordingOnly()
{
    Q_ASSERT(!m_exportDir.isEmpty());
    Q_ASSERT(!m_subjectId.isEmpty());
    Q_ASSERT(m_camera != nullptr);

    m_running = true;

    auto frameInterval = 1000 / m_framerate; // framerate in FPS, interval msec delay
    auto firstFrame = true;
    time_t lastFrameTime = 0;

    // wait for barrier release, if there is any
    waitOnBarrier();

    while (m_running) {
        auto frame = m_camera->getFrame();
        if (frame.first < 0) {
            // we have an invalid frame, ignore it
            continue;
        }
        // assume first frame is starting point
        if (firstFrame) {
            m_startTime = frame.first;
            firstFrame = false;
            lastFrameTime = frame.first - (frameInterval / 1.5);
        }
        auto timeSinceStart = frame.first - m_startTime;
        emit newFrame(timeSinceStart, frame.second);

        // enqueue frame to be stored on disk and wait the remaining time before requesting a new frame
        storeFrameAndWait(frame, m_frameBasePath,
                          &lastFrameTime, timeSinceStart, frameInterval);
    }

    m_startTime = 0;

    // store details about our recording
    // get a frame and store details
    auto finalFrame = m_camera->getFrame();

    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_exportDir).arg(m_subjectId);
    QJsonObject vInfo;
    vInfo.insert("frameWidth", finalFrame.second.cols);
    vInfo.insert("frameHeight", finalFrame.second.rows);

    vInfo.insert("exportWidth", m_exportResolution.width());
    vInfo.insert("exportHeight", m_exportResolution.height());

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitError("Unable to open video info file for writing.");
        return;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    emitFinishedSuccess();
}

void Tracker::stop()
{
    m_running = false;
}

void Tracker::emitError(const QString &msg)
{
    emit finished(false, msg);
}

void Tracker::emitFinishedSuccess()
{
    emit finished(true, QString());
}

void Tracker::waitOnBarrier()
{
    m_barrier.wait();
}

static
std::vector<cv::Point2f> findCornerBlobs(const cv::Mat& grayMat)
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

static
cv::Point findMaxColorBrightness(const cv::Mat& image, const cv::Mat& imageGray, cv::Scalar minColors, cv::Scalar maxColors)
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
    cv::minMaxLoc(colorMat,
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

static
double calculateTriangleGamma(Tracker::LEDTriangle& tri)
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
    auto cLen = qSqrt(qPow((tri.red.x   - tri.green.x), 2) + qPow((tri.red.y   - tri.green.y), 2));
    auto bLen = qSqrt(qPow((tri.red.x   - tri.blue.x), 2)  + qPow((tri.red.y   - tri.blue.y), 2));
    auto aLen = qSqrt(qPow((tri.green.x - tri.blue.x), 2)  + qPow((tri.green.y - tri.blue.y), 2));

    // calculate gamma angle at the blue LED
    auto gamma = qAcos((qPow(bLen, 2) + qPow(aLen, 2) - qPow(cLen, 2)) / (2 * aLen * bLen));
    tri.gamma = qRadiansToDegrees(gamma);

    return tri.gamma;
}

static
cv::Point2f calculateTriangleCentroid(Tracker::LEDTriangle& tri)
{
    auto x = (tri.red.x + tri.green.x + tri.blue.x) / 3;
    auto y = (tri.red.y + tri.green.y + tri.blue.y) / 3;

    return cv::Point2f(x, y);
}

static
double calculateTriangleTurnAngle(Tracker::LEDTriangle& tri)
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

static bool
cvRectFuzzyEqual(const std::vector<cv::Point2f>& a, const std::vector<cv::Point2f>& b, const uint tolerance = 2)
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

Tracker::LEDTriangle Tracker::trackPoints(time_t time, const cv::Mat &image)
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
        cv::putText(trackMat,
                    QStringLiteral("y%1").arg(res.gamma).toStdString(),
                    cv::Point(res.blue.x + 7, res.blue.y + 7),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(100, 100, 255));

    // find the maze
    if (m_mazeRect.size() == 4) {
        // draw maze rect
        cv::line(trackMat, m_mazeRect[0],  m_mazeRect[1], cv::Scalar(40, 120, 120), 2);
        cv::line(trackMat, m_mazeRect[2],  m_mazeRect[3], cv::Scalar(40, 120, 120), 2);

        cv::line(trackMat, m_mazeRect[0],  m_mazeRect[2], cv::Scalar(40, 120, 120), 2);
        cv::line(trackMat, m_mazeRect[1],  m_mazeRect[3], cv::Scalar(40, 120, 120), 2);

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
        cv::putText(infoMat,
                    QStringLiteral("X: %2 Y: %3").arg(res.center.x).arg(res.center.y).toStdString(),
                    cv::Point(6, 20),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.8,
                    cv::Scalar(255, 180, 180));
        cv::putText(infoMat,
                    m_subjectId.toStdString(),
                    cv::Point(m_mouseGraphicMat.cols - (m_subjectId.length() * 18) - 6, 20),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.8,
                    cv::Scalar(255, 180, 180));
    } else {
        infoMat.setTo(cv::Scalar(0, 0, 0));
        cv::putText(infoMat,
                    "Oh no, we do not know where the test subject is!",
                    cv::Point(14, (m_mouseGraphicMat.rows / 2) - 8),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.6,
                    cv::Scalar(100, 100, 255));
    }

    // emit info graphic
    emit newInfoGraphic(infoMat);

    // show video frame
    emit newTrackingFrame(time, trackMat);

    return res;
}
