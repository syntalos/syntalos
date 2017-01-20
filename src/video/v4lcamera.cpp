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

#include "v4lcamera.h"

#include <QDebug>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <QFileInfo>

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>

using namespace std::chrono;
#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fd, int request, void *arg)
{
    int r;
    do {
        r = ioctl (fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

/* convert from 4:2:2 YUYV interlaced to RGB24 */
/* based on ccvt_yuyv_bgr32() from camstream */
#define SAT(c) \
        if (c & (~255)) { if (c < 0) c = 0; else c = 255; }
static void
yuyv_to_rgb24 (int width, int height, unsigned char *src, unsigned char *dst)
{
   unsigned char *s;
   unsigned char *d;
   int l, c;
   int r, g, b, cr, cg, cb, y1, y2;

   l = height;
   s = src;
   d = dst;
   while (l--) {
      c = width >> 1;
      while (c--) {
         y1 = *s++;
         cb = ((*s - 128) * 454) >> 8;
         cg = (*s++ - 128) * 88;
         y2 = *s++;
         cr = ((*s - 128) * 359) >> 8;
         cg = (cg + (*s++ - 128) * 183) >> 8;

         r = y1 + cr;
         b = y1 + cb;
         g = y1 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

     *d++ = b;
     *d++ = g;
     *d++ = r;

         r = y2 + cr;
         b = y2 + cb;
         g = y2 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

     *d++ = b;
     *d++ = g;
     *d++ = r;
      }
   }
}

static void
uyvy_to_rgb24 (int width, int height, unsigned char *src, unsigned char *dst)
{
   unsigned char *s;
   unsigned char *d;
   int l, c;
   int r, g, b, cr, cg, cb, y1, y2;

   l = height;
   s = src;
   d = dst;
   while (l--) {
      c = width >> 1;
      while (c--) {
         cb = ((*s - 128) * 454) >> 8;
         cg = (*s++ - 128) * 88;
         y1 = *s++;
         cr = ((*s - 128) * 359) >> 8;
         cg = (cg + (*s++ - 128) * 183) >> 8;
         y2 = *s++;

         r = y1 + cr;
         b = y1 + cb;
         g = y1 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

     *d++ = b;
     *d++ = g;
     *d++ = r;

         r = y2 + cr;
         b = y2 + cb;
         g = y2 - cg;
         SAT(r);
         SAT(g);
         SAT(b);

     *d++ = b;
     *d++ = g;
     *d++ = r;
      }
   }
}

V4LCamera::V4LCamera(QObject *parent)
    : MACamera(parent),
      m_cameraFD(-1),
      m_camBuf(nullptr)
{
}

V4LCamera::~V4LCamera()
{
    close();
}

QList<QPair<QString, int>> V4LCamera::getCameraList() const
{
    QList<QPair<QString, int>> res;

    int deviceId = 0;
    while (true) {
        auto path = QStringLiteral("/dev/video%1").arg(deviceId);
        QFileInfo check_file(path);

        if (check_file.exists()) {
            res.append(qMakePair(QStringLiteral("Camera %1").arg(deviceId), deviceId));
            deviceId++;
        } else {
            break;
        }
    }

    return res;
}

void V4LCamera::setError(const QString& message, int code)
{
    if (code == 0)
        m_lastError = message;
    else
        m_lastError = QString("%1 (%2)").arg(message).arg(code);
}

QString V4LCamera::lastError() const
{
    return m_lastError;
}

int V4LCamera::testCameraPixFormat(uint v4lFmt)
{
    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = m_frameSize.width();
    fmt.fmt.pix.height = m_frameSize.height();
    fmt.fmt.pix.pixelformat = v4lFmt;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(m_cameraFD, VIDIOC_S_FMT, &fmt) < 0)
        return 1;

    // check if the tested format was supported
    if (fmt.fmt.pix.pixelformat != v4lFmt)
        return 2;

    return 0;
}

bool V4LCamera::open(int cameraId, const QSize& size)
{
    if (cameraId < 0) {
        setError("Not initialized.");
        return false;
    }

    if (m_cameraFD >= 0) {
        setError("Camera is already opened.");
        return false;
    }

    if (m_camBuf != nullptr) {
        setError("Camera is already opened.");
        return false;
    }

    auto devicePath = QStringLiteral("/dev/video%1").arg(cameraId);

    m_cameraFD = ::open(qPrintable(devicePath), O_RDWR);
    if (m_cameraFD == -1) {
        setError("Failed to open video capture device.");
        return false;
    }

    struct v4l2_capability caps;
    CLEAR(caps);
    if (xioctl(m_cameraFD, VIDIOC_QUERYCAP, &caps) < 0) {
        setError("Failed to query camera capabilities.");
        return false;
    }

    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        setError("Selected device has no video capture ability.");
        return false;
    }

    qDebug() << "Opening camera with resolution:" << size;

    m_pixFmt = PIXELFORMAT_NONE;
    m_frameSize = size;
    auto ret = testCameraPixFormat(V4L2_PIX_FMT_YUYV);
    if (ret == 0) {
        m_pixFmt = PIXELFORMAT_YUYV;
        qDebug() << "Selected YUYV pixel format";
    } else {
        ret = testCameraPixFormat(V4L2_PIX_FMT_UYVY);
    }
    if (ret == 0) {
        m_pixFmt = PIXELFORMAT_UYVY;
        qDebug() << "Selected UYVY pixel format";
    } else {
        if (ret == 1)
            setError(QStringLiteral("IOCTL VIDIOC_S_FMT failed: %1").arg(strerror(errno)));
        else
            setError("Failed to find supported pixel format");
        return false;
    }


    struct v4l2_requestbuffers req;
    CLEAR(req);

    // request buffer
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(m_cameraFD, VIDIOC_REQBUFS, &req) < 0) {
        setError("Unable to request buffer");
        return false;
    }

    // query buffer
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (ioctl(m_cameraFD, VIDIOC_QUERYBUF, &buf) < 0) {
        setError("VIDIOC_QUERYBUF failed");
        return false;
    }

    m_camBuf = mmap(NULL, // start anywhere
                    buf.length,
                    PROT_READ | PROT_WRITE, // required
                    MAP_SHARED, // recommended
                    m_cameraFD,
                    buf.m.offset);
    if (m_camBuf == nullptr) {
        setError("Unable to map memory");
        return false;
    }
    m_camBufLen = buf.length;

    return true;
}

bool V4LCamera::close()
{
    if (m_cameraFD)
        ::close(m_cameraFD);
    m_cameraFD = -1;
    if (m_camBuf != nullptr)
        munmap(m_camBuf, m_camBufLen);

    qDebug() << "V4LCamera closed.";
    return true;
}

bool V4LCamera::setFramerate(double fps)
{
    // TODO
    Q_UNUSED(fps);
    return true;
}

QPair<time_t, cv::Mat> V4LCamera::getFrame()
{
    QPair<time_t, cv::Mat> frame;
    cv::Mat mat;

    getFrame(&frame.first, mat);
    frame.second = mat;

    return frame;
}

void V4LCamera::getFrame(time_t *time, cv::Mat& buffer)
{
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    // put buffer into incoming queue
    if (xioctl(m_cameraFD, VIDIOC_QBUF, &buf) < 0) {
        setError("VIDIOC_QBUF failed");
        return;
    }

    // activate streaming
    if (xioctl(m_cameraFD, VIDIOC_STREAMON, &buf.type) < 0) {
        setError("VIDIOC_STREAMON failed.");
        return;
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(m_cameraFD, &fds);
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    auto ret = select(m_cameraFD+1, &fds, NULL, NULL, &tv);
    if (ret < 0) {
        setError("Unable to wait for frame");
        return;
    }
    if (ret == 0) {
        setError("Camera read timeout");
        return;
    }

    // get millisecond-resolution timestamp
    auto ms = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
    *time = ms.count();

    if(xioctl(m_cameraFD, VIDIOC_DQBUF, &buf) < 0) {
        setError("Unable to retrieve frame");
        return;
    }

    buffer.create(m_frameSize.height(),
                  m_frameSize.width(),
                  CV_8UC3);

    if (m_pixFmt == PIXELFORMAT_YUYV) {
        yuyv_to_rgb24(m_frameSize.width(),
                      m_frameSize.height(),
                      (unsigned char*) m_camBuf,
                      (unsigned char*) buffer.data);
    } else if (m_pixFmt == PIXELFORMAT_UYVY) {
        uyvy_to_rgb24(m_frameSize.width(),
                      m_frameSize.height(),
                      (unsigned char*) m_camBuf,
                      (unsigned char*) buffer.data);
    }

    // disable streaming
    if (xioctl(m_cameraFD, VIDIOC_STREAMOFF, &buf.type) < 0) {
        setError("VIDIOC_STREAMOFF failed.");
        return;
    }
}

QList<QSize> V4LCamera::getResolutionList(int cameraId)
{
    QList<QSize> res;

    cv::VideoCapture camera;
    if (!camera.open(cameraId))
        return res;

    res.append(QSize(camera.get(CV_CAP_PROP_FRAME_WIDTH), camera.get(CV_CAP_PROP_FRAME_HEIGHT)));

    camera.release();
    return res;
}
