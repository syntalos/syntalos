/**
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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

#include "miniscopemodule.h"

#include <QTimer>
#include <QDebug>
#include <miniscope.h>

#include "streams/frametype.h"
#include "miniscopesettingsdialog.h"

using namespace MScope;

class MiniscopeModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<DataStream<Frame>> m_rawOut;
    std::shared_ptr<DataStream<Frame>> m_dispOut;

    QTimer *m_evTimer;
    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;
    bool m_acceptFrames;
    MScope::MiniScope *m_miniscope;
    MiniscopeSettingsDialog *m_settingsDialog;

public:
    explicit MiniscopeModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_miniscope(nullptr),
          m_settingsDialog(nullptr)
    {
        m_rawOut = registerOutputPort<Frame>(QStringLiteral("frames-raw-out"), QStringLiteral("Raw Frames"));
        m_dispOut = registerOutputPort<Frame>(QStringLiteral("frames-disp-out"), QStringLiteral("Display Frames"));

        m_miniscope = new MiniScope;
        m_settingsDialog = new MiniscopeSettingsDialog(m_miniscope);
        addSettingsWindow(m_settingsDialog);

        m_miniscope->setScopeCamId(0);
        setName(name());

        m_miniscope->setOnFrame(&on_newRawFrame, this);
        m_miniscope->setOnDisplayFrame(&on_newDisplayFrame, this);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(200);
        connect(m_evTimer, &QTimer::timeout, this, &MiniscopeModule::checkMSStatus);

        // print output for better debugging
        m_miniscope->setPrintMessagesToStdout(false);
        m_miniscope->setOnMessage([&](const std::string &msg, void*) {
            qDebug().noquote() << name() + QStringLiteral(":") << QString::fromStdString(msg);
        });
    }

    ~MiniscopeModule()
    {
        delete m_miniscope;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    void setName(const QString &name) override
    {
        AbstractModule::setName(name);
        m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
    }

    bool prepare(const TestSubject &) override
    {
        if (!m_miniscope->connect()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }

        m_rawOut->setMetadataValue("framerate", (double) m_miniscope->fps());
        m_rawOut->setMetadataValue("has_color", false);
        m_rawOut->setSuggestedDataName(QStringLiteral("%1/msSlice").arg(datasetNameSuggestion()));

        m_dispOut->setMetadataValue("framerate", (double) m_miniscope->fps());
        m_dispOut->setMetadataValue("has_color", false);
        m_dispOut->setSuggestedDataName(QStringLiteral("%1_display/msDisplaySlice").arg(datasetNameSuggestion()));

        // start the streams
        m_rawOut->start();
        m_dispOut->start();

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer(m_miniscope->fps());
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);

        // permit tolerance of about half a frame
        m_clockSync->setTolerance(microseconds_t(static_cast<long>((1000.0 / m_miniscope->fps()) * 500)));

        // check accuracy every 500msec
        m_clockSync->setCheckInterval(milliseconds_t(500));

        // start the synchronizer
        if (!m_clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

        // do not accept any frames yet
        m_acceptFrames = false;

        // we already start capturing video here, and only start emitting frames later
        if (!m_miniscope->run()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }

        return true;
    }

    void start() override
    {
        m_clockSync->start();

        const auto stdSteadyClockStartTimepoint = std::chrono::steady_clock::now() - m_syTimer->timeSinceStartNsec();
        m_miniscope->setCaptureStartTime(stdSteadyClockStartTimepoint);
        m_evTimer->start();

        AbstractModule::start();
    }

    static void on_newRawFrame(const cv::Mat &mat, milliseconds_t &frameTime, const milliseconds_t &masterRecvTime, const milliseconds_t &deviceTime, void *udata)
    {
        const auto self = static_cast<MiniscopeModule*>(udata);
        if (!self->m_acceptFrames) {
            self->m_acceptFrames = self->m_running? self->m_miniscope->captureStartTimeInitialized() : false;
            if (!self->m_acceptFrames)
                return;
        }
        // use synchronizer to synchronize time
        frameTime = masterRecvTime;
        self->m_clockSync->processTimestamp(frameTime, deviceTime);

        // we don't want to forward dropped frames
        if (mat.empty())
            return;

        self->m_rawOut->push(Frame(mat, frameTime));
    }

    static void on_newDisplayFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        const auto self = static_cast<MiniscopeModule*>(udata);
        if (!self->m_acceptFrames)
            return;
        self->m_dispOut->push(Frame(mat, time));
    }

    void checkMSStatus()
    {
        if (!m_miniscope->running()) {
            if (!m_miniscope->lastError().empty()) {
                raiseError(QString::fromStdString(m_miniscope->lastError()));
                m_evTimer->stop();
                return;
            }
        }
        statusMessage(QStringLiteral("FPS: %1 Dropped: %2").arg(m_miniscope->currentFps()).arg(m_miniscope->droppedFramesCount()));
    }

    void stop() override
    {
        m_evTimer->stop();
        m_miniscope->stop();
        m_miniscope->disconnect();
        m_clockSync->stop();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("scope_cam_id", m_miniscope->scopeCamId());
        settings.insert("fps", static_cast<int>(m_miniscope->fps()));
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_miniscope->setScopeCamId(settings.value("scope_cam_id", 0).toInt());
        m_miniscope->setFps(settings.value("fps", 20).toInt());
        m_settingsDialog->updateValues();
        return true;
    }
};

QString MiniscopeModuleInfo::id() const
{
    return QStringLiteral("miniscope");
}

QString MiniscopeModuleInfo::name() const
{
    return QStringLiteral("Miniscope");
}

QString MiniscopeModuleInfo::description() const
{
    return QStringLiteral("Record fluorescence images from the brain of behaving animals using a UCLA MiniScope.");
}

QPixmap MiniscopeModuleInfo::pixmap() const
{
    return QPixmap(":/module/miniscope");
}

AbstractModule *MiniscopeModuleInfo::createModule(QObject *parent)
{
    return new MiniscopeModule(parent);
}

#include "miniscopemodule.moc"
