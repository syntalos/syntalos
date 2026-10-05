/*
 * Copyright (C) 2023-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "araviscameramodule.h"

#if defined(signals) && defined(Q_SIGNALS)
#define _SYTMP_QT_SIGNALS_DEFINED
#undef signals
#endif
#include <arv.h>
#ifdef _SYTMP_QT_SIGNALS_DEFINED
#define signals Q_SIGNALS
#undef _SYTMP_QT_SIGNALS_DEFINED
#endif

#include <atomic>

#include "datactl/frametype.h"
#include "configwindow.h"

SYNTALOS_MODULE(AravisCameraModule)

class AravisCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    QIcon m_modIcon;
    ArvConfigWindow *m_configWindow;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    std::shared_ptr<QArvCamera> m_camera;
    std::shared_ptr<QArvDecoder> m_decoder;

    int m_expectedWidth;
    int m_expectedHeight;

public:
    explicit AravisCameraModule(AravisCameraModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_expectedWidth(-1),
          m_expectedHeight(-1)
    {
        QArvCamera::init();

        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));
        m_modIcon = modInfo->icon();
    }

    std::expected<void, QString> initialize() final
    {
        m_configWindow = new ArvConfigWindow(m_log);
        m_configWindow->setWindowIcon(m_modIcon);
        addSettingsWindow(m_configWindow);

        connect(
            m_configWindow,
            &ArvConfigWindow::cameraSelected,
            this,
            [this](const std::shared_ptr<QArvCamera> &camera, const std::shared_ptr<QArvDecoder> &decoder) {
                if (hasWorker()) {
                    // safeguard, this should actually never be possible to happen
                    raiseError(QStringLiteral("Cannot change camera while running!"));
                    return;
                }

                m_camera = camera;
                m_decoder = decoder;
            });

        // set initial window titles
        setName(name());

        return {};
    }

    ~AravisCameraModule() override = default;

    void setName(const QString &name) final
    {
        AbstractModule::setName(name);
    }

    ModuleDriverKind driver() const final
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const final
    {
        return ModuleFeature::REALTIME | ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const RunInfo &) final
    {
        if (!m_camera) {
            raiseError(QStringLiteral("No camera selected!"));
            return false;
        }

        statusMessage("Configuring streams...");
        m_configWindow->setCameraInUseExternal(true);
        const auto tfParams = m_configWindow->currentTransformParams();

        const auto roi = m_camera->getROI();
        // expectedWidth/Height are the dimensions produced directly by Aravis / the decoder
        m_expectedWidth = roi.width();
        m_expectedHeight = roi.height();

        // The runtime rotation pipeline swaps axes for 90°/270°; report the
        // post-rotation size to downstream consumers.
        const bool swapsAxes = tfParams->rot == 1 || tfParams->rot == 3;
        const int outWidth = swapsAxes ? m_expectedHeight : m_expectedWidth;
        const int outHeight = swapsAxes ? m_expectedWidth : m_expectedHeight;
        const auto expectedFps = m_camera->getFPS();
        m_outStream->setMetadataValue("size", MetaSize(outWidth, outHeight));
        m_outStream->setMetadataValue("framerate", expectedFps);

        // start the stream
        m_outStream->start();

        // set up clock synchronizer
        auto clockSync = initClockSynchronizer(expectedFps);
        clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);

        // start the synchronizer
        if (!clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

        setWorker(
            Worker{
                .camera = m_camera,
                .decoder = m_decoder,
                .expectedFps = expectedFps,
                .expectedWidth = m_expectedWidth,
                .expectedHeight = m_expectedHeight,
                .rotation = tfParams->rot,
                .liveTransform = m_configWindow->liveTransformParams(),
                .outStream = m_outStream,
                .clockSync = std::move(clockSync),
            });

        statusMessage("Waiting.");
        return true;
    }

    /**
     * Runs the image acquisition in the module's thread.
     *
     * The thread itself only drives a GLib main loop for the housekeeping, the frames arrive
     * in a callback that is run by the stream thread of Aravis.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<QArvCamera> camera;
        std::shared_ptr<QArvDecoder> decoder;
        double expectedFps;
        int expectedWidth;
        int expectedHeight;

        int rotation; /// rotation in steps of 90°, it can not be changed while we are running
        LiveValue<LiveTransformParams> liveTransform;

        std::shared_ptr<DataStream<Frame>> outStream;
        std::unique_ptr<SecondaryClockSynchronizer> clockSync;

        void run()
        {
            g_autoptr(GMainLoop) loop = g_main_loop_new(nullptr, FALSE);

            // we carry one second of data or 30 frames in the queue
            camera->setFrameQueueSize(expectedFps > 30 ? static_cast<uint>(std::ceil(expectedFps)) + 1 : 30);

            // Mutable per-acquisition state shared with the Aravis frame callback. It lives
            // on the heap and is co-owned by the callback (captured by value), so the
            // callback never dereferences this thread's stack frame even if Aravis delivers
            // a buffer late, during teardown.
            struct AcqState {
                uint64_t frameCount = 0;
                nanoseconds_t sysOffsetToMaster{0};
                guint64 devOffsetToSysNs = 0;
                std::atomic_uint fpsWindowFrameCount{0};

                // The flip & invert settings in use, the frame callback updates them when
                // they are changed in the settings window.
                LiveTransformParams transform;

                // Buffer the decoder writes into, recycled between frames. Only ever touched
                // from the Aravis frame callback, which Aravis runs single-threaded.
                cv::Mat decodeBuf;
            };
            auto acqState = std::make_shared<AcqState>();
            acqState->transform = liveTransform.get();

            // display the connected camera model
            QString cameraStatus;
            {
                const auto camId = camera->getId();
                if (camId.id == nullptr || camId.id[0] == '\0')
                    cameraStatus = QString::fromUtf8(camId.model);
                else
                    cameraStatus = QStringLiteral("%2 (%3)").arg(camId.model, camId.id);
                mod.setStatusMessage(cameraStatus);
            }

            // wait until we actually start acquiring data
            mod.waitForStart();

            // The callback is run by the stream thread of Aravis. It refers to this worker, which
            // outlives that thread: stopAcquisition() joins it before we return.
            auto acqStartResult = camera->startAcquisition(true, true, [this, acqState](ArvBuffer *buffer) {
                if (!mod.running())
                    return;
                if (acqState->frameCount == 0) {
                    // determine the base offset times to the master clock when retrieving the first frame
                    const auto firstMasterTime = mod.timer->timeSinceStartNsec();
                    const auto firstFrameSysTimeNs = arv_buffer_get_system_timestamp(buffer);
                    const auto firstFrameDevTimeNs = arv_buffer_get_timestamp(buffer);

                    acqState->sysOffsetToMaster = nanoseconds_t(firstMasterTime.count() - (gint64)firstFrameSysTimeNs);
                    acqState->devOffsetToSysNs = firstFrameSysTimeNs - firstFrameDevTimeNs;
                }

                auto frameSysTimeNs = arv_buffer_get_system_timestamp(buffer);
                auto frameDevTimeNs = arv_buffer_get_timestamp(buffer);
                if (frameDevTimeNs == 0) {
                    // no timestamp available, use the system timestamp
                    frameDevTimeNs = frameSysTimeNs;
                } else {
                    frameDevTimeNs += acqState->devOffsetToSysNs;
                }
                auto masterTime = std::chrono::duration_cast<microseconds_t>(
                    nanoseconds_t(frameSysTimeNs) + acqState->sysOffsetToMaster);

                if (!decoder)
                    return;

                size_t size = 0;
                const auto data = static_cast<const char *>(arv_buffer_get_data(buffer, &size));
                if (size == 0 || data == nullptr)
                    return;

                clockSync->processTimestamp(masterTime, nsecToUsec(nanoseconds_t(frameDevTimeNs)));

                // Hand the decoder a buffer we exclusively own, so it can recycle the previous
                // frame's pixel memory instead of allocating a new block for every frame.
                // While a subscriber is lagging behind it still holds the frames we published,
                // and matEnsureExclusive() then hands out fresh memory to keep them intact.
                auto &decodeBuf = acqState->decodeBuf;
                const auto decoderCvType = decoder->cvType();
                if (Q_UNLIKELY(decoderCvType < 0)) {
                    // the decoder picks its own output type, so we can not pre-size the buffer
                    // and just make sure it does not overwrite an already published frame
                    decodeBuf.release();
                } else {
                    matEnsureExclusive(decodeBuf, expectedHeight, expectedWidth, decoderCvType);
                }

                decoder->decodeInto(QByteArrayView(data, static_cast<qsizetype>(size)), decodeBuf);

                // shallow reference: the transforms below may rebind img to a differently shaped
                // matrix (rotation), which must not cost us the recycled decode buffer
                cv::Mat img = decodeBuf;

                // sanity check, because sometimes this camera doesn't adhere to the contract...
                if (Q_UNLIKELY(img.cols != expectedWidth || img.rows != expectedHeight)) {
                    mod.raiseError(
                        QStringLiteral("Camera returned frame with unexpected dimensions %1x%2 (expected %3x%4)!")
                            .arg(img.cols)
                            .arg(img.rows)
                            .arg(expectedWidth)
                            .arg(expectedHeight));
                    return;
                }

                // pick up changes the user has made in the settings window
                if (const auto transform = liveTransform.takeIfChanged())
                    acqState->transform = *transform;

                if (acqState->transform.invert) {
                    int bits = img.depth() == CV_8U ? 8 : 16;
                    cv::subtract((1 << bits) - 1, img, img);
                }

                if (acqState->transform.flip != -100)
                    cv::flip(img, img, acqState->transform.flip);

                switch (rotation) {
                case 1:
                    cv::transpose(img, img);
                    cv::flip(img, img, 0);
                    break;

                case 2:
                    cv::flip(img, img, -1);
                    break;

                case 3:
                    cv::transpose(img, img);
                    cv::flip(img, img, 1);
                    break;
                }

                Frame frame;
                frame.index = acqState->frameCount++;
                frame.time = masterTime;
                frame.mat = std::move(img);
                outStream->push(std::move(frame));
                acqState->fpsWindowFrameCount++;
            });

            if (!acqStartResult) {
                mod.raiseError(acqStartResult.error());
                return;
            }

            // Data for our diagnostic callback to check if we are still supposed to be running,
            // and if the camera is running at the designated framerate.
            struct TimeoutData {
                Worker *self;
                GMainLoop *loop;
                std::shared_ptr<AcqState> acqState;
                double expectedFps;
                symaster_timepoint fpsCheckStart = currentTimePoint();
                bool fpsLow = false;
            };

            auto timeoutCb = [](gpointer data) -> gboolean {
                auto state = static_cast<TimeoutData *>(data);
                auto self = state->self;

                if (!self->mod.running()) {
                    g_main_loop_quit(state->loop);
                    return G_SOURCE_REMOVE;
                }

                const auto windowMsec = timeDiffToNowMsec(state->fpsCheckStart).count();
                if (windowMsec <= 2000)
                    return G_SOURCE_CONTINUE;

                const auto frameCount = state->acqState->fpsWindowFrameCount.exchange(0);
                const auto currentFps = (frameCount * 1000.0) / static_cast<double>(windowMsec);

                if (currentFps < state->expectedFps * 0.9) {
                    state->fpsLow = true;
                    self->mod.setStatusMessage(
                        QStringLiteral("<b><font color=\"red\">Framerate (%1 fps) is too low!</font></b>")
                            .arg(currentFps, 0, 'f', 1));
                } else if (state->fpsLow) {
                    state->fpsLow = false;
                    self->mod.setStatusMessage(QString());
                }

                state->fpsCheckStart = currentTimePoint();

                return G_SOURCE_CONTINUE;
            };

            auto timeoutData = new TimeoutData{this, loop, acqState, expectedFps};
            auto timeoutSrc = g_timeout_source_new(250);
            g_source_set_callback(timeoutSrc, timeoutCb, timeoutData, [](gpointer data) {
                delete static_cast<TimeoutData *>(data);
            });

            // only after the run is started, attach the timeout source
            g_source_attach(timeoutSrc, g_main_loop_get_context(loop));

            // run the event loop until we quit
            g_main_loop_run(loop);

            camera->stopAcquisition();
        }
    };

    void stop() final
    {
        statusMessage("Cleaning up...");

        // our thread has finished at this point, and the stream thread of Aravis with it
        if (auto worker = takeWorker<Worker>())
            safeStopSynchronizer(worker->clockSync);

        m_configWindow->setCameraInUseExternal(false);
        statusMessage("Camera stopped.");
        AbstractModule::stop();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &camFeatures) final
    {
        m_configWindow->serializeSettings(settings, camFeatures);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &camFeatures) final
    {
        m_configWindow->loadSettings(settings, camFeatures);
        return true;
    }

protected:
    void usbHotplugEvent(UsbHotplugEventKind kind) final
    {
        if (hasWorker())
            return;
        if (kind == UsbHotplugEventKind::DEVICE_ARRIVED || kind == UsbHotplugEventKind::DEVICES_CHANGE
            || kind == UsbHotplugEventKind::DEVICE_LEFT)
            m_configWindow->refreshCameras();
    }
};

QString AravisCameraModuleInfo::id() const
{
    return QStringLiteral("camera-arv");
}

QString AravisCameraModuleInfo::name() const
{
    return QStringLiteral("Aravis Camera");
}

QString AravisCameraModuleInfo::summary() const
{
    return QStringLiteral("Capture frames with any GenICam-compatible camera.");
}

QString AravisCameraModuleInfo::description() const
{
    return QStringLiteral(
        "Capture frames from many camera devices using the Aravis vision library for GenICam-based cameras.");
}

QString AravisCameraModuleInfo::authors() const
{
    return QStringLiteral(
        "2012-2019 Jure Varlec and Andrej Lajovic, Vega Astronomical Society — Ljubljana<br/>"
        "2023-2026 Matthias Klumpp");
}

QString AravisCameraModuleInfo::license() const
{
    return QStringLiteral("GPL-3.0+");
}

ModuleCategories AravisCameraModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

QColor AravisCameraModuleInfo::color() const
{
    return QColor::fromRgba(qRgba(29, 158, 246, 180)).darker();
}

AbstractModule *AravisCameraModuleInfo::createModule(QObject *parent)
{
    return new AravisCameraModule(this, parent);
}

#include "araviscameramodule.moc"
