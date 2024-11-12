/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include <QDebug>
#if defined(signals) && defined(Q_SIGNALS)
#define _SYTMP_QT_SIGNALS_DEFINED
#undef signals
#endif
#include <arv.h>
#ifdef _SYTMP_QT_SIGNALS_DEFINED
#define signals Q_SIGNALS
#undef _SYTMP_QT_SIGNALS_DEFINED
#endif

#include "datactl/frametype.h"
#include "configwindow.h"

SYNTALOS_MODULE(AravisCameraModule)

class AravisCameraModule : public AbstractModule
{
    Q_OBJECT
private:
    QIcon m_modIcon;
    ArvConfigWindow *m_configWindow;
    std::atomic_bool m_stopped;
    std::shared_ptr<DataStream<Frame>> m_outStream;

    std::shared_ptr<QArvCamera> m_camera;
    std::shared_ptr<QArvDecoder> m_decoder;
    TransformParams *m_tfParams;

public:
    explicit AravisCameraModule(AravisCameraModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_stopped(true)
    {
        QArvCamera::init();

        m_outStream = registerOutputPort<Frame>(QStringLiteral("video"), QStringLiteral("Video"));
        m_modIcon = modInfo->icon();
    }

    bool initialize() final
    {
        m_configWindow = new ArvConfigWindow(QStringLiteral("%1-%2").arg(id()).arg(index()));
        m_configWindow->setWindowIcon(m_modIcon);
        addSettingsWindow(m_configWindow);

        connect(
            m_configWindow,
            &ArvConfigWindow::cameraSelected,
            this,
            [this](const std::shared_ptr<QArvCamera> &camera, const std::shared_ptr<QArvDecoder> &decoder) {
                if (m_running || !m_stopped) {
                    // safeguard, this should actually never be possible to happen
                    raiseError(QStringLiteral("Cannot change camera while running!"));
                    return;
                }

                m_camera = camera;
                m_decoder = decoder;
            });

        // set initial window titles
        setName(name());

        return true;
    }

    ~AravisCameraModule() {}

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

    bool prepare(const TestSubject &) final
    {
        if (!m_camera) {
            raiseError(QStringLiteral("No camera selected!"));
            return false;
        }

        statusMessage("Configuring streams...");
        m_configWindow->setCameraInUseExternal(true);
        m_tfParams = m_configWindow->currentTransformParams();

        const auto roi = m_camera->getROI();
        // set the required stream metadata for video capture
        m_outStream->setMetadataValue("size", QSize(roi.width(), roi.height()));
        m_outStream->setMetadataValue("framerate", m_camera->getFPS());

        // start the stream
        m_outStream->start();

        statusMessage("Waiting.");
        return true;
    }

    void runThread(OptionalWaitCondition *waitCondition) final
    {
        g_autoptr(GMainLoop) loop = g_main_loop_new(nullptr, FALSE);

        m_camera->setFrameQueueSize(16);

        // set up clock synchronizer
        const auto clockSync = initClockSynchronizer(m_camera->getFPS());
        clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);

        // start the synchronizer
        if (!clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return;
        }

        auto timeoutCb = [](gpointer data) -> gboolean {
            auto pair = static_cast<std::pair<AravisCameraModule *, GMainLoop *> *>(data);
            auto self = pair->first;
            auto loop = pair->second;

            if (!self->m_running) {
                g_main_loop_quit(loop);
                return G_SOURCE_REMOVE;
            }

            return G_SOURCE_CONTINUE;
        };

        auto timeoutData = new std::pair<AravisCameraModule *, GMainLoop *>(this, loop);
        auto timeoutSrc = g_timeout_source_new(250);
        g_source_set_callback(timeoutSrc, timeoutCb, timeoutData, [](gpointer data) {
            delete static_cast<std::pair<AravisCameraModule *, GMainLoop *> *>(data);
        });

        // display the connected camera model
        {
            const auto camId = m_camera->getId();
            if (camId.id == nullptr || camId.id[0] == '\0')
                statusMessage(QString::fromUtf8(camId.model));
            else
                statusMessage(QStringLiteral("%2 (%3)").arg(camId.model, camId.id));
        }

        // wait until we actually start acquiring data
        waitCondition->wait(this);

        uint64_t frameCount = 0;
        nanoseconds_t sysOffsetToMaster;
        guint64 devOffsetToSysNs = 0;
        m_camera->startAcquisition(
            true, true, [this, &frameCount, &sysOffsetToMaster, &devOffsetToSysNs, &clockSync](ArvBuffer *buffer) {
                if (!m_running)
                    return;
                if (frameCount == 0) {
                    // determine the base offset times to the master clock when retrieving the first frame
                    auto firstMasterTime = m_syTimer->timeSinceStartNsec();
                    auto firstFrameSysTimeNs = arv_buffer_get_system_timestamp(buffer);
                    auto firstFrameDevTimeNs = arv_buffer_get_timestamp(buffer);

                    sysOffsetToMaster = nanoseconds_t(firstMasterTime.count() - (gint64)firstFrameSysTimeNs);
                    devOffsetToSysNs = firstFrameSysTimeNs - firstFrameDevTimeNs;
                }

                auto frameSysTimeNs = arv_buffer_get_system_timestamp(buffer);
                auto frameDevTimeNs = arv_buffer_get_timestamp(buffer);
                if (frameDevTimeNs == 0) {
                    // no timestamp available, use the system timestamp
                    frameDevTimeNs = frameSysTimeNs;
                } else {
                    frameDevTimeNs += devOffsetToSysNs;
                }
                auto masterTime = std::chrono::duration_cast<microseconds_t>(
                    nanoseconds_t(frameSysTimeNs) + sysOffsetToMaster);

                QByteArray frame;
                size_t size;
                const void *data;
                data = arv_buffer_get_data(buffer, &size);
                frame = QByteArray::fromRawData(static_cast<const char *>(data), size);

                if (frame.isEmpty())
                    return;
                if (!m_decoder)
                    return;

                clockSync->processTimestamp(masterTime, nsecToUsec(nanoseconds_t(frameDevTimeNs)));

                m_decoder->decode(frame);
                cv::Mat img = m_decoder->getCvImage();

                if (m_tfParams->invert) {
                    int bits = img.depth() == CV_8U ? 8 : 16;
                    cv::subtract((1 << bits) - 1, img, img);
                }

                if (m_tfParams->flip != -100)
                    cv::flip(img, img, m_tfParams->flip);

                switch (m_tfParams->rot) {
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

                Frame syFrame(img, frameCount++, masterTime);
                m_outStream->push(syFrame);
            });

        // only after the run is started, attach the timeout source
        g_source_attach(timeoutSrc, g_main_loop_get_context(loop));

        // run the event loop until we quit
        g_main_loop_run(loop);

        m_camera->stopAcquisition();
        safeStopSynchronizer(clockSync);
        m_stopped = true;
    }

    void stop() final
    {
        statusMessage("Cleaning up...");

        m_running = false;
        while (!m_stopped) {
            appProcessEvents();
        }

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

    void usbHotplugEvent(UsbHotplugEventKind) final
    {
        if (!m_stopped)
            return;
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
        "2023-2024 Matthias Klumpp");
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
