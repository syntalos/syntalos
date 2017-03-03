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

#include "config.h"
#include "videotracker.h"

#include <QDebug>
#include <QThread>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QThreadPool>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include <ktar.h>

#ifdef USE_UEYE_CAMERA
#include "ueyecamera.h"
#endif
#include "genericcamera.h"
#include "utils.h"


VideoTracker::VideoTracker(QObject *parent)
    : QObject(parent),
      m_resolution(QSize(1280, 1024)),
      m_cameraId(-1)
{
    m_framerate = 20;
    m_exportResolution = QSize(1024, 768);
    m_camera = nullptr;
    m_autoGain = false;
    m_exposureTime = 8;

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

QString VideoTracker::lastError() const
{
    return m_lastError;
}

void VideoTracker::setResolution(const QSize &size)
{
    m_resolution = size;
    qDebug() << "Camera resolution selected:" << size;
}

void VideoTracker::setCameraId(QVariant cameraId)
{
    m_cameraId = cameraId;
    qDebug() << "Selected camera:" << m_cameraId;
}

QVariant VideoTracker::cameraId() const
{
    return m_cameraId;
}

static bool qsizeBiggerThan(const QSize &s1, const QSize &s2)
{
    auto s1v = s1.width() + s1.height();
    auto s2v = s2.width() + s2.height();

    return s1v > s2v;
}

QList<QSize> VideoTracker::resolutionList(QVariant cameraId)
{
#ifdef USE_UEYE_CAMERA
    auto camera = new UEyeCamera();
#else
    auto camera = new GenericCamera();
#endif
    auto ret = camera->getResolutionList(cameraId);
    delete camera;

    qSort(ret.begin(), ret.end(), qsizeBiggerThan);
    return ret;
}

void VideoTracker::setFramerate(int fps)
{
    m_framerate = fps;
    qDebug() << "Camera framerate set to" << fps << "FPS";
}

int VideoTracker::framerate() const
{
    return m_framerate;
}

void VideoTracker::emitErrorFinished(const QString &message)
{
    emit error(message);
    m_lastError = message;
    moveToThread(QApplication::instance()->thread()); // give back our object to the main thread

    // we no longer need the camera, it's safe to close it
    closeCamera();

    emit finished();
}

bool VideoTracker::openCamera()
{
#ifdef USE_UEYE_CAMERA
    m_camera = new UEyeCamera;
#else
    m_camera = new GenericCamera;
#endif

    if (!m_camera->open(m_cameraId, m_resolution)) {
        emitErrorFinished(m_camera->lastError());
        delete m_camera;
        m_camera = nullptr;
        return false;
    }

    m_camera->setConfFile(m_uEyeConfigFile);
    m_camera->setAutoGain(m_autoGain);
    m_camera->setExposureTime(m_exposureTime);
    m_camera->setFramerate(m_framerate);
    m_camera->setGPIOFlash(m_gpioFlash);

    return true;
}

bool VideoTracker::closeCamera()
{
    if (m_running)
        return false;
    if (m_camera == nullptr)
        return false;

    delete m_camera;
    m_camera = nullptr;

    return true;
}

void VideoTracker::setUEyeConfigFile(const QString &fileName)
{
    m_uEyeConfigFile = fileName;
}

QString VideoTracker::uEyeConfigFile() const
{
    return m_uEyeConfigFile;
}

void VideoTracker::setGPIOFlash(bool enabled)
{
    m_gpioFlash = enabled;
}

bool VideoTracker::gpioFlash() const
{
    return m_gpioFlash;
}

void VideoTracker::setExperimentKind(ExperimentKind::Kind kind)
{
    m_experimentKind = kind;
}

void VideoTracker::setStartTimestamp(const time_t time)
{
    m_startTime = time;
    emit syncClock();
}

class FrameSaveTask : public QRunnable
{
private:
    QString m_frameBasePath;
    time_t m_timeSinceStart;
    cv::Mat m_frame;
    QSize m_exportResolution;

public:
    FrameSaveTask(const QString& frameBasePath, const time_t& timeSinceStart,
                  const QSize& exportRes, cv::Mat frameImg)
    {
        m_frameBasePath = frameBasePath;
        m_timeSinceStart = timeSinceStart;
        m_frame = frameImg;
        m_exportResolution = exportRes;
    }

    void run()
    {
        // store downscaled frame on disk
        cv::Mat emat;

        cv::resize(m_frame,
                   emat,
                   cv::Size(m_exportResolution.width(), m_exportResolution.height()));
        cv::imwrite(QString("%1%2.jpg").arg(m_frameBasePath).arg(m_timeSinceStart).toStdString(), emat);
    }
};

void VideoTracker::storeFrameAndWait(const QPair<time_t, cv::Mat> &frame, const QString& frameBasePath,
                                     time_t *lastFrameTime, const time_t& timeSinceStart, const time_t& frameInterval)
{
    // enqueue frame to be stored on disk by a separate thread.
    // the runnable is disposed of automatically.
    auto task = new FrameSaveTask(frameBasePath, timeSinceStart,
                                  m_exportResolution, frame.second);
    QThreadPool::globalInstance()->start(task);

    // wait the remaining time before requesting the next frame
    time_t remainingTime;
    if ((*lastFrameTime) != frame.first)
        remainingTime = frameInterval - (frame.first - (*lastFrameTime));
    else
        remainingTime = 0; // we fetched the same frame twice - directly jump to the next one
    (*lastFrameTime) = frame.first;

    if (remainingTime > 0)
        QThread::usleep(remainingTime * 1000); // sleep remainingTime msecs
}

bool VideoTracker::runTracking(const QString& frameBasePath)
{
    // prepare position output CSV file
    auto posInfoPath = QStringLiteral("%1/%2_positions.csv").arg(m_exportDir).arg(m_mouseId);
    QFile posInfoFile(posInfoPath);
    if (!posInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitErrorFinished("Unable to open position CSV file for writing.");
        return false;
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

    m_triggered = false;
    emit recordingReady();
    // spinlock until we received the ready signal
    while (!m_triggered) { QCoreApplication::processEvents(); }

    auto frameInterval = 1000 / m_framerate; // framerate in FPS, interval msec delay

    auto firstFrame = true;
    time_t lastFrameTime = 0;
    m_running = true;
    while (m_running) {
        auto frame = m_camera->getFrame();
        if (frame.first < 0) {
            // we have an invalid frame, ignore it
            continue;
        }

        // assume first frame is starting point
        if (firstFrame) {
            firstFrame = false;
            setStartTimestamp(frame.first);
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
        storeFrameAndWait(frame, frameBasePath,
                          &lastFrameTime, timeSinceStart, frameInterval);
    }

    m_startTime = 0;

    // store details about our recording
    // get a frame and store details
    auto finalFrame = m_camera->getFrame();

    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_exportDir).arg(m_mouseId);
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
        emitErrorFinished("Unable to open video info file for writing.");
        return false;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    return true;
}

bool VideoTracker::runRecordingOnly(const QString &frameBasePath)
{
    m_triggered = false;
    emit recordingReady();
    // spinlock until we received the ready signal
    while (!m_triggered) { QCoreApplication::processEvents(); }

    auto frameInterval = 1000 / m_framerate; // framerate in FPS, interval msec delay

    auto firstFrame = true;
    time_t lastFrameTime = 0;
    m_running = true;
    while (m_running) {
        auto frame = m_camera->getFrame();
        if (frame.first < 0) {
            // we have an invalid frame, ignore it
            continue;
        }
        // assume first frame is starting point
        if (firstFrame) {
            setStartTimestamp(frame.first);
            firstFrame = false;
            lastFrameTime = frame.first - (frameInterval / 1.5);
        }
        auto timeSinceStart = frame.first - m_startTime;
        emit newFrame(timeSinceStart, frame.second);

        // enqueue frame to be stored on disk and wait the remaining time before requesting a new frame
        storeFrameAndWait(frame, frameBasePath,
                          &lastFrameTime, timeSinceStart, frameInterval);
    }

    m_startTime = 0;

    // store details about our recording
    // get a frame and store details
    auto finalFrame = m_camera->getFrame();

    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_exportDir).arg(m_mouseId);
    QJsonObject vInfo;
    vInfo.insert("frameWidth", finalFrame.second.cols);
    vInfo.insert("frameHeight", finalFrame.second.rows);

    vInfo.insert("exportWidth", m_exportResolution.width());
    vInfo.insert("exportHeight", m_exportResolution.height());

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emitErrorFinished("Unable to open video info file for writing.");
        return false;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    return true;
}

void VideoTracker::run()
{
    if (m_exportDir.isEmpty()) {
        emitErrorFinished("No visual analysis export location is set.");
        return;
    }

    if (m_mouseId.isEmpty()) {
        emitErrorFinished("No mouse ID is set.");
        return;
    }

    if (m_camera == nullptr) {
        emitErrorFinished("Camera was not opened.");
        return;
    }

    // create storage location for frames
    auto frameBaseDir = QStringLiteral("%1/frames").arg(m_exportDir);
    QDir().mkpath(frameBaseDir);
    auto frameBasePath = QStringLiteral("%1/%2_").arg(frameBaseDir).arg(m_mouseId);

    bool ret;
    if (m_experimentKind == ExperimentKind::KindMaze)
        ret = runTracking(frameBasePath);
    else
        ret = runRecordingOnly(frameBasePath);


    // finalize
    m_startTime = 0;
    moveToThread(QApplication::instance()->thread()); // give back our object to the main thread
    emit finished();

    // we no longer need the camera, it's safe to close it
    closeCamera();

    qDebug() << "Finished video.";
}

static
std::vector<cv::Point2f> findCornerBlobs(cv::Mat grayMat)
{
    cv::Mat binaryMat(grayMat.size(), grayMat.type());

    // get a binary matrix using adaptive gaussian thresholding
    cv::adaptiveThreshold(grayMat, binaryMat, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV, 17, 3.4);
    // remove noise aggressively
    cv::morphologyEx(binaryMat, binaryMat, cv::MORPH_CLOSE, cv::Mat::ones(10, 10, binaryMat.type()));
    cv::morphologyEx(binaryMat, binaryMat, cv::MORPH_CLOSE, cv::Mat::ones(8, 8, binaryMat.type()));

    // detect blobs
    cv::SimpleBlobDetector detector;
    std::vector<cv::KeyPoint> keypoints;
    detector.detect(binaryMat, keypoints);

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
double calculateTriangleGamma(VideoTracker::LEDTriangle& tri)
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
cv::Point2f calculateTriangleCentroid(VideoTracker::LEDTriangle& tri)
{
    auto x = (tri.red.x + tri.green.x + tri.blue.x) / 3;
    auto y = (tri.red.y + tri.green.y + tri.blue.y) / 3;

    return cv::Point2f(x, y);
}

static
double calculateTriangleTurnAngle(VideoTracker::LEDTriangle& tri)
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

VideoTracker::LEDTriangle VideoTracker::trackPoints(time_t time, const cv::Mat &image)
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
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(255, 0, 0), -1);
    res.red = maxLoc;

    // green maximum
    maxLoc = findMaxColorBrightness(image, grayMat, cv::Scalar(0, 200, 0), cv::Scalar(110, 255, 180));
    if (maxLoc.x > 0)
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(0, 255, 0), -1);
    res.green = maxLoc;

    // blue maximum
    maxLoc = findMaxColorBrightness(image, grayMat, cv::Scalar(210, 0, 0), cv::Scalar(255, 240, 70));
    if (maxLoc.x > 0)
        cv::circle(trackMat, maxLoc, 6, cv::Scalar(0, 0, 255), -1);
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
        cv::line(trackMat, m_mazeRect[0],  m_mazeRect[1], cv::Scalar(255, 50, 50), 2);
        cv::line(trackMat, m_mazeRect[2],  m_mazeRect[3], cv::Scalar(255, 50, 50), 2);

        cv::line(trackMat, m_mazeRect[0],  m_mazeRect[2], cv::Scalar(255, 50, 50), 2);
        cv::line(trackMat, m_mazeRect[1],  m_mazeRect[3], cv::Scalar(255, 50, 50), 2);

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
                    m_mouseId.toStdString(),
                    cv::Point(m_mouseGraphicMat.cols - (m_mouseId.length() * 18) - 6, 20),
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

void VideoTracker::stop()
{
    m_running = false;
}

void VideoTracker::triggerRecording()
{
    m_triggered = true;
}

void VideoTracker::setDataLocation(const QString& dir)
{
    m_exportDir = dir;
}

void VideoTracker::setMouseId(const QString& mid)
{
    m_mouseId = mid;
}

void VideoTracker::setAutoGain(bool enabled)
{
    m_autoGain = enabled;
}

QList<QPair<QString, QVariant>> VideoTracker::getCameraList() const
{
#ifdef USE_UEYE_CAMERA
    auto camera = new UEyeCamera;
#else
    auto camera = new GenericCamera;
#endif
    auto ret = camera->getCameraList();
    delete camera;

    return ret;
}

void VideoTracker::setExportResolution(const QSize& size)
{
    m_exportResolution = size;
}

QSize VideoTracker::exportResolution() const
{
    return m_exportResolution;
}

void VideoTracker::setExposureTime(double value)
{
    m_exposureTime = value;
    qDebug() << "Exposure time set to" << value;
}

bool VideoTracker::makeFrameTarball()
{
    auto frameDirPath = QStringLiteral("%1/frames").arg(m_exportDir);
    QDir frameDir(frameDirPath);

    // check if we have work to do
    if (!frameDir.exists())
        return true;

    auto frameTarFname = QStringLiteral("%1/%2_frames.tar.gz").arg(m_exportDir).arg(m_mouseId);
    KTar tar(frameTarFname);
    if (!tar.open(QIODevice::WriteOnly)) {
        m_lastError = "Unable to open tarball for writing.";
        return false;
    }

    auto files = frameDir.entryList(QDir::Files);
    auto max = files.count();

    auto i = 0;
    foreach (auto fname, files) {
        if (!tar.addLocalFile(frameDir.absoluteFilePath(fname), fname)) {
            m_lastError = QStringLiteral("Could not add frame '%1' images to tarball.").arg(fname);
            return false;
        }
        emit progress(max, i);
        i++;
    }

    tar.close();
    frameDir.removeRecursively();

    return true;
}
