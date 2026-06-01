/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lccamera.h"

#include <algorithm>
#include <array>
#include <condition_variable>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <map>
#include <mutex>
#include <ostream>
#include <queue>
#include <streambuf>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDir>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// libcamera uses 'slots', 'signals' and 'emit' as ordinary identifiers, which
// clash with Qt's keyword macros. So we apply the usual workaround.
#if defined(signals) && defined(Q_SIGNALS)
#define _SYTMP_QT_KW_DEFINED
#undef slots
#undef signals
#undef emit
#endif
#include <libcamera/libcamera.h>
#ifdef _SYTMP_QT_KW_DEFINED
#define signals Q_SIGNALS
#define slots   Q_SLOTS
#define emit    Q_EMIT
#undef _SYTMP_QT_KW_DEFINED
#endif

using namespace libcamera;

static std::shared_ptr<Camera> resolveCamera(const QString &id);

/**
 * @brief Forward one libcamera log line to a Quill logger.
 *
 * libcamera has no structured logging, only preformatted text lines like
 * "[timestamp] [tid] LEVEL category file:line message". To stay robust against
 * format changes we log the whole line verbatim and only scan the first few tokens
 * for a level keyword, so a real error still surfaces at error severity.
 */
static void emitLibcameraLine(QuillLogger *logger, const std::string &line)
{
    // scan the leading tokens (level normally sits after the [ts] [tid] brackets)
    std::string_view sv(line);
    std::string_view level;
    for (int token = 0; token < 5 && !sv.empty(); token++) {
        while (!sv.empty() && sv.front() == ' ')
            sv.remove_prefix(1);
        const auto end = sv.find(' ');
        const std::string_view tok = sv.substr(0, end);
        if (tok == "DEBUG" || tok == "INFO" || tok == "WARN" || tok == "ERROR" || tok == "FATAL") {
            level = tok;
            break;
        }
        if (end == std::string_view::npos)
            break;
        sv.remove_prefix(end + 1);
    }

    if (level == "DEBUG")
        LOG_DEBUG(logger, "{}", line);
    else if (level == "WARN")
        LOG_WARNING(logger, "{}", line);
    else if (level == "ERROR")
        LOG_ERROR(logger, "{}", line);
    else if (level == "FATAL")
        LOG_CRITICAL(logger, "{}", line);
    else // INFO or unrecognised
        LOG_INFO(logger, "{}", line);
}

/**
 * @brief std::streambuf that forwards complete lines to a Quill logger.
 */
class QuillLogStreambuf : public std::streambuf
{
public:
    explicit QuillLogStreambuf(QuillLogger *logger)
        : m_logger(logger)
    {
    }

protected:
    // libcamera serializes writes to its log output and emits each message as one complete,
    // newline-terminated write, so no locking is needed here.
    int overflow(int ch) override
    {
        if (ch != traits_type::eof())
            appendChar(static_cast<char>(ch));
        return ch;
    }

    std::streamsize xsputn(const char *s, std::streamsize n) override
    {
        for (std::streamsize i = 0; i < n; i++)
            appendChar(s[i]);
        return n;
    }

private:
    void appendChar(char c)
    {
        if (c == '\n') {
            if (!m_buffer.empty()) {
                emitLibcameraLine(m_logger, m_buffer);
                m_buffer.clear();
            }
        } else if (c != '\r') {
            m_buffer.push_back(c);
        }
    }

    QuillLogger *m_logger;
    std::string m_buffer;
};

/**
 * @brief Route libcamera's logging through Quill (once, process-wide).
 *
 * Must run before the CameraManager starts so its own startup messages are captured.
 * The stream/streambuf are intentionally leaked so they live for the whole process and
 * avoid static-destruction ordering issues with late libcamera logging.
 */
static void installLibcameraLogBridge()
{
    static auto logBuf = new QuillLogStreambuf(getLogger("libcamera"));
    static auto logStream = new std::ostream(logBuf);
    libcamera::logSetStream(logStream, false);
}

/**
 * @brief Lazily-created, process-wide libcamera CameraManager.
 *
 * libcamera mandates a single CameraManager per process; it owns the camera
 * objects we hand out, so it is kept alive for the whole process lifetime.
 */
static std::shared_ptr<CameraManager> globalCameraManager()
{
    static std::shared_ptr<CameraManager> manager;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        // redirect libcamera's stderr logging into Quill before anything starts
        installLibcameraLogBridge();

        auto mgr = std::make_shared<CameraManager>();
        if (mgr->start() < 0)
            return;
        manager = mgr;
    });

    return manager;
}

struct MappedPlane {
    void *base;
    size_t length;
};

/**
 * @brief  Camera lifecycle phase
 *
 * Streaming covers both "started" and "fully connected", as
 * nothing can fail between those two points.
 */
enum class CamState {
    Disconnected, /// no camera acquired
    Acquired,     /// acquire() succeeded, not yet streaming
    Streaming,    /// start() succeeded; streaming and ready for getFrame()
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
class LcCameraData
{
public:
    QuillLogger *log{};
    symaster_timepoint startTime;

    QString camId;
    std::shared_ptr<Camera> cam;
    std::unique_ptr<CameraConfiguration> config;
    std::unique_ptr<FrameBufferAllocator> allocator;
    Stream *stream{};
    std::vector<std::unique_ptr<Request>> requests;
    std::map<FrameBuffer *, MappedPlane> mapped;

    cv::Size frameSize{640, 480};
    unsigned int stride{0};
    double fps{30};
    QString pixFmt;
    PixelFormat activeFormat;

    bool autoExposure{true};
    double exposureTime{10000};
    double gain{1.0};
    double brightness{0.0};
    double contrast{1.0};
    double saturation{1.0};

    // V4L2 power-line frequency; default to 50 Hz (1)
    int powerLineFreq{1};

    // control availability for the connected camera
    bool ctlAe{false};
    bool ctlExposure{false};
    bool ctlGain{false};
    bool ctlBrightness{false};
    bool ctlContrast{false};
    bool ctlSaturation{false};
    bool ctlFrameDuration{false};

    CamState state{CamState::Disconnected};

    // teardown signal
    bool stopping{false};
    QString lastError;

    std::mutex queueMutex;
    std::condition_variable queueCond;
    // completed requests paired with the master-clock time at which they arrived
    std::queue<std::pair<Request *, symaster_timepoint>> completedQueue;

    uint64_t frameIndex{0};
};
#pragma GCC diagnostic pop

/**
 * @brief The camera to operate on: the already-connected one, or a transient lookup
 *        (without acquiring) for capability enumeration before connecting.
 */
static std::shared_ptr<Camera> currentCamera(LcCameraData *d)
{
    return d->state == CamState::Streaming ? d->cam : resolveCamera(d->camId);
}

LcCamera::LcCamera(QuillLogger *logger)
    : d(new LcCameraData())
{
    d->log = logger;
}

LcCamera::~LcCamera()
{
    disconnect();
}

void LcCamera::setLogger(QuillLogger *logger)
{
    d->log = logger;
}

void LcCamera::fail(const QString &msg)
{
    d->lastError = msg;
    if (d->log)
        LOG_ERROR(d->log, "{}", msg.toStdString());
}

void LcCamera::setCameraId(const QString &id)
{
    d->camId = id;
}

QString LcCamera::cameraId() const
{
    return d->camId;
}

void LcCamera::setStartTime(const symaster_timepoint &time)
{
    d->startTime = time;
}

cv::Size LcCamera::resolution() const
{
    return d->frameSize;
}

void LcCamera::setResolution(const cv::Size &size)
{
    d->frameSize = size;
}

double LcCamera::framerate() const
{
    return d->fps;
}

void LcCamera::setFramerate(double fps)
{
    d->fps = fps;
}

QString LcCamera::pixelFormat() const
{
    return d->pixFmt;
}

void LcCamera::setPixelFormat(const QString &fmt)
{
    d->pixFmt = fmt;
}

bool LcCamera::autoExposure() const
{
    return d->autoExposure;
}

void LcCamera::setAutoExposure(bool enabled)
{
    d->autoExposure = enabled;
}

double LcCamera::exposureTime() const
{
    return d->exposureTime;
}

void LcCamera::setExposureTime(double micros)
{
    d->exposureTime = micros;
}

double LcCamera::gain() const
{
    return d->gain;
}

void LcCamera::setGain(double value)
{
    d->gain = value;
}

double LcCamera::brightness() const
{
    return d->brightness;
}

void LcCamera::setBrightness(double value)
{
    d->brightness = value;
}

double LcCamera::contrast() const
{
    return d->contrast;
}

void LcCamera::setContrast(double value)
{
    d->contrast = value;
}

double LcCamera::saturation() const
{
    return d->saturation;
}

void LcCamera::setSaturation(double value)
{
    d->saturation = value;
}

int LcCamera::powerLineFrequency() const
{
    return d->powerLineFreq;
}

void LcCamera::setPowerLineFrequency(int value)
{
    d->powerLineFreq = value;
}

/**
 * @brief Resolve the V4L2 device node backing a libcamera camera.
 *
 * libcamera exposes the device's dev_t numbers via the SystemDevices property;
 * we match those against the /dev/video* nodes. Returns an empty string if the
 * camera is not backed by a V4L2 device (e.g. a non-UVC pipeline).
 */
static QString v4l2NodeForCamera(const std::shared_ptr<Camera> &cam)
{
    if (!cam)
        return QString();
    const auto devices = cam->properties().get(properties::SystemDevices);
    if (!devices)
        return QString();

    const QDir devDir(QStringLiteral("/dev"));
    const auto nodes = devDir.entryList(QStringList() << QStringLiteral("video*"), QDir::System);
    for (const int64_t devNum : *devices) {
        for (const auto &node : nodes) {
            const QString path = devDir.filePath(node);
            struct stat st;
            if (stat(qPrintable(path), &st) == 0 && S_ISCHR(st.st_mode) && st.st_rdev == static_cast<dev_t>(devNum))
                return path;
        }
    }

    return {};
}

/**
 * @brief Run @p fn with an opened fd of the camera's V4L2 node; returns false if unavailable.
 */
template<typename Fn>
static bool withV4l2Node(const std::shared_ptr<Camera> &cam, Fn &&fn)
{
    const QString path = v4l2NodeForCamera(cam);
    if (path.isEmpty())
        return false;
    const int fd = ::open(qPrintable(path), O_RDWR | O_NONBLOCK);
    if (fd < 0)
        return false;
    const bool ok = fn(fd);
    ::close(fd);

    return ok;
}

bool LcCamera::powerLineFrequencySupported()
{
    auto cam = currentCamera(d.get());
    return withV4l2Node(cam, [](int fd) {
        struct v4l2_queryctrl q;
        memset(&q, 0, sizeof(q));
        q.id = V4L2_CID_POWER_LINE_FREQUENCY;
        if (ioctl(fd, VIDIOC_QUERYCTRL, &q) != 0)
            return false;
        return (q.flags & V4L2_CTRL_FLAG_DISABLED) == 0;
    });
}

int LcCamera::readPowerLineFrequency()
{
    auto cam = currentCamera(d.get());
    int value = -1;
    withV4l2Node(cam, [&value](int fd) {
        struct v4l2_control c;
        memset(&c, 0, sizeof(c));
        c.id = V4L2_CID_POWER_LINE_FREQUENCY;
        if (ioctl(fd, VIDIOC_G_CTRL, &c) != 0)
            return false;
        value = c.value;
        return true;
    });

    return value;
}

bool LcCamera::isConnected() const
{
    return d->state == CamState::Streaming;
}

QString LcCamera::lastError() const
{
    return d->lastError;
}

/**
 * @brief Resolve the libcamera Camera for the selected ID.
 *
 * Returns the already-connected camera if we have one, otherwise looks it up
 * transiently (without acquiring it) for capability enumeration.
 */
static std::shared_ptr<Camera> resolveCamera(const QString &id)
{
    auto mgr = globalCameraManager();
    if (!mgr || id.isEmpty())
        return nullptr;

    return mgr->get(id.toStdString());
}

QList<QPair<QString, QString>> LcCamera::availableCameras()
{
    QList<QPair<QString, QString>> result;
    auto mgr = globalCameraManager();
    if (!mgr)
        return result;

    for (const auto &cam : mgr->cameras()) {
        const QString id = QString::fromStdString(cam->id());
        QString display = id;
        const auto model = cam->properties().get(properties::Model);
        if (model)
            display = QString::fromStdString(std::string(*model));
        result.append(qMakePair(display, id));
    }

    return result;
}

QList<QString> LcCamera::readPixelFormats()
{
    QList<QString> result;
    auto cam = currentCamera(d.get());
    if (!cam)
        return result;

    auto config = cam->generateConfiguration({StreamRole::VideoRecording});
    if (!config || config->empty())
        return result;

    for (const auto &pf : config->at(0).formats().pixelformats())
        result.append(QString::fromStdString(pf.toString()));

    return result;
}

QList<cv::Size> LcCamera::readFrameSizes(const QString &pixfmt)
{
    QList<cv::Size> result;
    auto cam = currentCamera(d.get());
    if (!cam)
        return result;

    auto config = cam->generateConfiguration({StreamRole::VideoRecording});
    if (!config || config->empty())
        return result;

    const auto pf = PixelFormat::fromString(pixfmt.toStdString());
    if (!pf.isValid())
        return result;

    for (const auto &size : config->at(0).formats().sizes(pf))
        result.append(cv::Size(size.width, size.height));

    // present discrete sizes largest-first for a tidy selection list
    std::sort(result.begin(), result.end(), [](const cv::Size &a, const cv::Size &b) {
        return (a.width * a.height) > (b.width * b.height);
    });
    return result;
}

LcControlRange LcCamera::controlRange(const QString &name)
{
    LcControlRange range;
    auto cam = currentCamera(d.get());
    if (!cam)
        return range;

    const auto &infoMap = cam->controls();

    auto readFloat = [&](const ControlId *id) {
        auto it = infoMap.find(id);
        if (it == infoMap.end())
            return;
        range.available = true;
        range.min = it->second.min().get<float>();
        range.max = it->second.max().get<float>();
        range.def = it->second.def().get<float>();
    };

    if (name == QLatin1String("exposure")) {
        auto it = infoMap.find(&controls::ExposureTime);
        if (it != infoMap.end()) {
            range.available = true;
            range.min = it->second.min().get<int32_t>();
            range.max = it->second.max().get<int32_t>();
            range.def = it->second.def().get<int32_t>();
        }
    } else if (name == QLatin1String("gain")) {
        readFloat(&controls::AnalogueGain);
    } else if (name == QLatin1String("brightness")) {
        readFloat(&controls::Brightness);
    } else if (name == QLatin1String("contrast")) {
        readFloat(&controls::Contrast);
    } else if (name == QLatin1String("saturation")) {
        readFloat(&controls::Saturation);
    } else if (name == QLatin1String("ae")) {
        range.available = infoMap.find(&controls::AeEnable) != infoMap.end();
    }

    return range;
}

/**
 * @brief Pixel formats we know how to convert to BGR in getFrame().
 */
static bool isSupportedFormat(const PixelFormat &fmt)
{
    return fmt == formats::YUYV || fmt == formats::UYVY || fmt == formats::NV12 || fmt == formats::NV21
           || fmt == formats::YUV420 || fmt == formats::MJPEG || fmt == formats::RGB888 || fmt == formats::BGR888
           || fmt == formats::XRGB8888;
}

static void applyControls(LcCameraData *d, ControlList &ctrls)
{
    if (d->ctlAe)
        ctrls.set(controls::AeEnable, d->autoExposure);
    if (!d->autoExposure && d->ctlExposure)
        ctrls.set(controls::ExposureTime, static_cast<int32_t>(d->exposureTime));
    if (d->ctlGain)
        ctrls.set(controls::AnalogueGain, static_cast<float>(d->gain));
    if (d->ctlBrightness)
        ctrls.set(controls::Brightness, static_cast<float>(d->brightness));
    if (d->ctlContrast)
        ctrls.set(controls::Contrast, static_cast<float>(d->contrast));
    if (d->ctlSaturation)
        ctrls.set(controls::Saturation, static_cast<float>(d->saturation));

    if (d->ctlFrameDuration && d->fps > 0) {
        const int64_t frameDurUs = static_cast<int64_t>(1.0e6 / d->fps);
        ctrls.set(controls::FrameDurationLimits, {frameDurUs, frameDurUs});
    }
}

bool LcCamera::connect()
{
    if (d->state == CamState::Streaming)
        return true;

    auto mgr = globalCameraManager();
    if (!mgr) {
        fail(QStringLiteral("libcamera could not be initialized on this system."));
        return false;
    }

    d->cam = mgr->get(d->camId.toStdString());
    if (!d->cam) {
        fail(QStringLiteral("Selected camera '%1' was not found.").arg(d->camId));
        return false;
    }

    if (d->cam->acquire() < 0) {
        fail(QStringLiteral("Could not acquire camera (is it in use by another application?)."));
        d->cam.reset();
        return false;
    }
    d->state = CamState::Acquired;

    // apply the power-line (anti-flicker) frequency out-of-band via V4L2, since
    // libcamera does not expose this control for UVC cameras
    if (d->powerLineFreq >= 0) {
        const bool plfOk = withV4l2Node(d->cam, [this](int fd) {
            struct v4l2_control c;
            memset(&c, 0, sizeof(c));
            c.id = V4L2_CID_POWER_LINE_FREQUENCY;
            c.value = d->powerLineFreq;
            return ioctl(fd, VIDIOC_S_CTRL, &c) == 0;
        });
        if (!plfOk && d->log)
            LOG_WARNING(d->log, "Could not set the power-line frequency on the camera device.");
    }

    d->config = d->cam->generateConfiguration({StreamRole::VideoRecording});
    if (!d->config || d->config->empty()) {
        fail(QStringLiteral("Could not generate a camera stream configuration."));
        disconnect();
        return false;
    }

    auto &cfg = d->config->at(0);
    const auto pf = PixelFormat::fromString(d->pixFmt.toStdString());
    if (pf.isValid())
        cfg.pixelFormat = pf;
    cfg.size = Size(d->frameSize.width, d->frameSize.height);
    cfg.bufferCount = 4;

    if (d->config->validate() == CameraConfiguration::Invalid) {
        fail(QStringLiteral("The requested stream configuration is invalid."));
        disconnect();
        return false;
    }

    if (d->cam->configure(d->config.get()) < 0) {
        fail(QStringLiteral("Failed to apply the stream configuration to the camera."));
        disconnect();
        return false;
    }

    // adopt the (possibly adjusted) effective configuration
    d->activeFormat = cfg.pixelFormat;
    d->frameSize = cv::Size(cfg.size.width, cfg.size.height);
    d->stride = cfg.stride;
    d->stream = cfg.stream();

    if (!isSupportedFormat(d->activeFormat)) {
        fail(QStringLiteral("Pixel format '%1' is not supported by this module yet.")
                 .arg(QString::fromStdString(d->activeFormat.toString())));
        disconnect();
        return false;
    }

    // determine which controls this camera supports
    const auto &infoMap = d->cam->controls();
    d->ctlAe = infoMap.find(&controls::AeEnable) != infoMap.end();
    d->ctlExposure = infoMap.find(&controls::ExposureTime) != infoMap.end();
    d->ctlGain = infoMap.find(&controls::AnalogueGain) != infoMap.end();
    d->ctlBrightness = infoMap.find(&controls::Brightness) != infoMap.end();
    d->ctlContrast = infoMap.find(&controls::Contrast) != infoMap.end();
    d->ctlSaturation = infoMap.find(&controls::Saturation) != infoMap.end();
    d->ctlFrameDuration = infoMap.find(&controls::FrameDurationLimits) != infoMap.end();

    // allocate frame buffers
    d->allocator = std::make_unique<FrameBufferAllocator>(d->cam);
    if (d->allocator->allocate(d->stream) < 0) {
        fail(QStringLiteral("Failed to allocate camera frame buffers."));
        disconnect();
        return false;
    }

    const auto &buffers = d->allocator->buffers(d->stream);
    for (const auto &buffer : buffers) {
        // map the underlying dmabuf; planes of one buffer share a single fd here,
        // so a single mapping spanning all planes is sufficient
        const auto &planes = buffer->planes();
        const int fd = planes[0].fd.get();
        size_t length = 0;
        for (const auto &plane : planes)
            length = std::max<size_t>(length, static_cast<size_t>(plane.offset) + plane.length);

        void *mem = mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
        if (mem == MAP_FAILED) {
            fail(QStringLiteral("Failed to memory-map a camera frame buffer."));
            disconnect();
            return false;
        }
        d->mapped[buffer.get()] = {mem, length};

        auto request = d->cam->createRequest();
        if (!request) {
            fail(QStringLiteral("Failed to create a capture request."));
            disconnect();
            return false;
        }
        if (request->addBuffer(d->stream, buffer.get()) < 0) {
            fail(QStringLiteral("Failed to associate a buffer with a capture request."));
            disconnect();
            return false;
        }
        applyControls(d.get(), request->controls());
        d->requests.push_back(std::move(request));
    }

    // completed requests are delivered on libcamera's pipeline thread (direct
    // connection, since LcCamera is not a libcamera Object); we only enqueue them
    d->cam->requestCompleted.connect(this, [this](Request *request) {
        if (request->status() == Request::RequestCancelled)
            return;
        // stamp the arrival time immediately, here on the pipeline thread, so it is
        // not skewed by how long the frame later waits in our queue
        const auto arrival = symaster_clock::now();
        {
            std::lock_guard<std::mutex> lock(d->queueMutex);
            d->completedQueue.push({request, arrival});
        }
        d->queueCond.notify_one();
    });

    if (d->cam->start() < 0) {
        fail(QStringLiteral("Failed to start the camera."));
        d->cam->requestCompleted.disconnect(this);
        disconnect();
        return false;
    }
    // streaming now; nothing below can fail, so this is also the fully-connected state
    d->state = CamState::Streaming;

    d->stopping = false;
    d->frameIndex = 0;
    for (auto &request : d->requests)
        d->cam->queueRequest(request.get());

    return true;
}

/**
 * @brief Convert the mapped pixel buffer of a completed request to a BGR cv::Mat.
 *
 * The returned Mat always owns its data, so the underlying buffer can be safely
 * re-queued afterwards.
 */
static cv::Mat convertToBgr(LcCameraData *d, FrameBuffer *buffer, void *base)
{
    const auto &planes = buffer->planes();
    const auto *p0 = static_cast<const uint8_t *>(base) + planes[0].offset;
    const int w = d->frameSize.width;
    const int h = d->frameSize.height;
    const int stride = static_cast<int>(d->stride > 0 ? d->stride : static_cast<unsigned int>(w) * 2);

    cv::Mat out;
    const auto &fmt = d->activeFormat;

    if (fmt == formats::MJPEG) {
        const size_t bytesused = buffer->metadata().planes()[0].bytesused;
        cv::Mat raw(1, static_cast<int>(bytesused), CV_8UC1, const_cast<uint8_t *>(p0));
        out = cv::imdecode(raw, cv::IMREAD_COLOR);
    } else if (fmt == formats::YUYV) {
        cv::Mat m(h, w, CV_8UC2, const_cast<uint8_t *>(p0), stride);
        cv::cvtColor(m, out, cv::COLOR_YUV2BGR_YUYV);
    } else if (fmt == formats::UYVY) {
        cv::Mat m(h, w, CV_8UC2, const_cast<uint8_t *>(p0), stride);
        cv::cvtColor(m, out, cv::COLOR_YUV2BGR_UYVY);
    } else if (fmt == formats::NV12) {
        cv::Mat m(h + h / 2, w, CV_8UC1, const_cast<uint8_t *>(p0));
        cv::cvtColor(m, out, cv::COLOR_YUV2BGR_NV12);
    } else if (fmt == formats::NV21) {
        cv::Mat m(h + h / 2, w, CV_8UC1, const_cast<uint8_t *>(p0));
        cv::cvtColor(m, out, cv::COLOR_YUV2BGR_NV21);
    } else if (fmt == formats::YUV420) {
        cv::Mat m(h + h / 2, w, CV_8UC1, const_cast<uint8_t *>(p0));
        cv::cvtColor(m, out, cv::COLOR_YUV2BGR_I420);
    } else if (fmt == formats::RGB888) {
        // libcamera RGB888 is stored B,G,R in memory on little-endian -> already BGR
        cv::Mat m(h, w, CV_8UC3, const_cast<uint8_t *>(p0), stride);
        out = m.clone();
    } else if (fmt == formats::BGR888) {
        // libcamera BGR888 is stored R,G,B in memory on little-endian
        cv::Mat m(h, w, CV_8UC3, const_cast<uint8_t *>(p0), stride);
        cv::cvtColor(m, out, cv::COLOR_RGB2BGR);
    } else if (fmt == formats::XRGB8888) {
        cv::Mat m(h, w, CV_8UC4, const_cast<uint8_t *>(p0), stride);
        cv::cvtColor(m, out, cv::COLOR_BGRA2BGR);
    }

    return out;
}

bool LcCamera::getFrame(Frame &frame, SecondaryClockSynchronizer *clockSync)
{
    if (d->state != CamState::Streaming)
        return false;

    Request *request = nullptr;
    symaster_timepoint arrival;
    {
        std::unique_lock<std::mutex> lock(d->queueMutex);
        if (!d->queueCond.wait_for(lock, std::chrono::seconds(5), [this]() {
                return !d->completedQueue.empty() || d->stopping;
            })) {
            // timed out waiting for a frame
            return false;
        }
        if (d->stopping || d->completedQueue.empty())
            return false;
        std::tie(request, arrival) = d->completedQueue.front();
        d->completedQueue.pop();
    }

    // the master-clock receive time was captured when the frame arrived (in the
    // completion callback), so it is unaffected by any backlog/delay in our queue
    auto frameRecvTime = std::chrono::duration_cast<microseconds_t>(arrival - d->startTime);

    auto requeue = [&]() {
        request->reuse(Request::ReuseBuffers);
        applyControls(d.get(), request->controls());
        d->cam->queueRequest(request);
    };

    // discard frames that were already buffered before the run officially started
    // (the camera streams from connect(), but startTime is only set in start())
    if (frameRecvTime.count() < 0) {
        requeue();
        return false;
    }

    const auto &buffers = request->buffers();
    auto bufIt = buffers.find(d->stream);
    if (bufIt == buffers.end()) {
        requeue();
        return false;
    }
    FrameBuffer *buffer = bufIt->second;

    if (buffer->metadata().status != FrameMetadata::FrameSuccess) {
        requeue();
        return false;
    }

    auto mapIt = d->mapped.find(buffer);
    if (mapIt == d->mapped.end()) {
        requeue();
        return false;
    }

    cv::Mat mat = convertToBgr(d.get(), buffer, mapIt->second.base);
    if (mat.empty()) {
        requeue();
        return false;
    }

    if (mat.cols != d->frameSize.width || mat.rows != d->frameSize.height) {
        fail(QStringLiteral("Received frame with unexpected dimensions (%1x%2, expected %3x%4).")
                 .arg(mat.cols)
                 .arg(mat.rows)
                 .arg(d->frameSize.width)
                 .arg(d->frameSize.height));
        requeue();
        return false;
    }

    // the kernel capture timestamp (nanoseconds, CLOCK_BOOTTIME) is far more precise than
    // our observation moment, so it can lock onto the device's frame cadence and remove
    // our observation jitter. processTimestamp() adjusts frameRecvTime in place.
    const auto devTime = std::chrono::duration_cast<microseconds_t>(
        nanoseconds_t(static_cast<int64_t>(buffer->metadata().timestamp)));
    clockSync->processTimestamp(frameRecvTime, devTime);

    frame.mat = mat;
    frame.time = frameRecvTime;
    frame.index = d->frameIndex++;

    requeue();
    return true;
}

void LcCamera::disconnect()
{
    // wake up any blocked getFrame() call
    {
        std::lock_guard<std::mutex> lock(d->queueMutex);
        d->stopping = true;
    }
    d->queueCond.notify_all();

    if (d->cam) {
        d->cam->requestCompleted.disconnect(this);
        if (d->state == CamState::Streaming)
            d->cam->stop();
    }

    {
        std::lock_guard<std::mutex> lock(d->queueMutex);
        decltype(d->completedQueue) empty;
        std::swap(d->completedQueue, empty);
    }

    d->requests.clear();

    for (const auto &entry : d->mapped)
        munmap(entry.second.base, entry.second.length);
    d->mapped.clear();

    if (d->allocator && d->stream) {
        d->allocator->free(d->stream);
        d->allocator.reset();
    }

    if (d->cam) {
        if (d->state != CamState::Disconnected)
            d->cam->release();
        d->cam.reset();
    }

    d->config.reset();
    d->stream = nullptr;
    d->state = CamState::Disconnected;
}
