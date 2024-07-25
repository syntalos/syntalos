/**
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "miniscopemodule.h"

#include <QDebug>
#include <QTimer>
#include <miniscope.h>

#include "miniscopesettingsdialog.h"
#include "datactl/frametype.h"

SYNTALOS_MODULE(MiniscopeModule)

using namespace MScope;

class MiniscopeModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<DataStream<Frame>> m_rawOut;
    std::shared_ptr<DataStream<Frame>> m_dispOut;
    std::shared_ptr<DataStream<FloatSignalBlock>> m_bnoVecOut;
    std::shared_ptr<DataStream<TableRow>> m_bnoTabOut;

    std::vector<float> m_lastOrientationVec;

    QTimer *m_evTimer;
    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;
    bool m_acceptFrames;
    uint64_t m_recFrameCount;

    MScope::Miniscope *m_miniscope;
    QFile *m_valChangeLogFile;
    MiniscopeSettingsDialog *m_settingsDialog;

public:
    explicit MiniscopeModule(ModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_acceptFrames(false),
          m_recFrameCount(0),
          m_miniscope(nullptr),
          m_settingsDialog(nullptr)
    {
        m_rawOut = registerOutputPort<Frame>(QStringLiteral("frames-raw-out"), QStringLiteral("Raw Frames"));
        m_dispOut = registerOutputPort<Frame>(QStringLiteral("frames-disp-out"), QStringLiteral("Display Frames"));
        m_bnoVecOut = registerOutputPort<FloatSignalBlock>(
            QStringLiteral("bno-raw-out"), QStringLiteral("Orientation Vector"));
        m_bnoTabOut = registerOutputPort<TableRow>(QStringLiteral("bno-tab-out"), QStringLiteral("Orientation Rows"));

        m_bnoVecOut->setMetadataValue("time_unit", "milliseconds");
        m_bnoVecOut->setMetadataValue("data_unit", "au");
        m_bnoVecOut->setMetadataValue(
            "signal_names",
            QStringList() << "qw"
                          << "qx"
                          << "qy"
                          << "qz");

        m_valChangeLogFile = new QFile();

        m_miniscope = new Miniscope();
        m_settingsDialog = new MiniscopeSettingsDialog(m_miniscope);
        m_settingsDialog->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDialog);

        m_miniscope->setScopeCamId(0);
        setName(name());

        m_miniscope->setOnFrame(&on_newRawFrame, this);
        m_miniscope->setOnDisplayFrame(&on_newDisplayFrame, this);
        m_miniscope->setOnControlValueChange(&on_controlValueChanged, this);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(200);
        connect(m_evTimer, &QTimer::timeout, this, &MiniscopeModule::checkMSStatus);

        // show status messages
        m_miniscope->setOnStatusMessage([&](const QString &msg, void *) {
            setStatusMessage(msg);
        });
    }

    ~MiniscopeModule()
    {
        if (m_miniscope->isConnected())
            m_miniscope->disconnect();
        delete m_miniscope;

        if (m_valChangeLogFile->isOpen())
            m_valChangeLogFile->close();
        delete m_valChangeLogFile;
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

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        // ensure the currently displayed device info is accurate
        m_settingsDialog->updateCurrentDeviceName();
    }

    bool prepare(const TestSubject &) override
    {
        // do not accept any frames yet
        m_acceptFrames = false;

        // obtain logfile location to write control change information to
        auto dstore = createDefaultDataset();
        if (dstore.get() == nullptr)
            return false;

        const auto valChangeLogFname = dstore->setDataFile("ctlvalue-changes.csv");
        m_valChangeLogFile->setFileName(valChangeLogFname);
        if (!m_valChangeLogFile->open(QFile::WriteOnly | QFile::Truncate)) {
            raiseError(QStringLiteral("Unable to open control value change logfile for writing!"));
            return false;
        }

        // write logfile header
        {
            QTextStream tsout(m_valChangeLogFile);
            tsout << "Time"
                  << ";"
                  << "ID"
                  << ";"
                  << "Display Value"
                  << ";"
                  << "Device Value"
                  << ";"
                  << "\n";
        }

        // connect Miniscope if it isn't connected yet
        // (we do something ugly here and keep a working connection in the background,
        // as reconnecting a DAQ box that has already been connected once frequently fails)
        if (!m_miniscope->isConnected()) {
            if (!m_miniscope->connect()) {
                raiseError(m_miniscope->lastError());
                return false;
            }
        }

        // we already start capturing video here, and only start emitting frames later
        if (!m_miniscope->run()) {
            raiseError(m_miniscope->lastError());
            return false;
        }

        // re-apply previously adjusted control settings and disable
        // controls we don't want changed
        m_settingsDialog->setRunning(true);

        // we need to set the framerate-related stuff after the miniscope has been started, so
        // we will get the right, final FPS value
        m_rawOut->setMetadataValue("framerate", (double)m_miniscope->fps());
        m_rawOut->setMetadataValue("has_color", false);
        m_rawOut->setSuggestedDataName(QStringLiteral("%1/mscope").arg(datasetNameSuggestion()));

        m_dispOut->setMetadataValue("framerate", (double)m_miniscope->fps());
        m_dispOut->setMetadataValue("has_color", true);
        m_dispOut->setSuggestedDataName(QStringLiteral("%1_display/mscope_display").arg(datasetNameSuggestion()));

        m_bnoTabOut->setMetadataValue(
            "table_header",
            QStringList() << "Time [ms]"
                          << "qw"
                          << "qx"
                          << "qy"
                          << "qz");
        m_bnoTabOut->setSuggestedDataName(QStringLiteral("%1_bno/orientation").arg(datasetNameSuggestion()));

        m_bnoVecOut->setMetadataValue("time_unit", "milliseconds");
        m_bnoVecOut->setMetadataValue("data_unit", "au");
        m_bnoVecOut->setMetadataValue(
            "signal_names",
            QStringList() << "qw"
                          << "qx"
                          << "qy"
                          << "qz");
        m_bnoVecOut->setSuggestedDataName(QStringLiteral("%1_bno/orientation").arg(datasetNameSuggestion()));

        // start the streams
        m_rawOut->start();
        m_dispOut->start();
        if (m_miniscope->hasHeadOrientationSupport()) {
            if (m_bnoTabOut->hasSubscribers())
                m_bnoTabOut->start();
            if (m_bnoVecOut->hasSubscribers())
                m_bnoVecOut->start();
        }

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer(m_miniscope->fps());
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD);

        // start the synchronizer
        if (!m_clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

        return true;
    }

    void start() override
    {
        m_clockSync->start();

        const auto stdSteadyClockStartTimepoint = std::chrono::steady_clock::now() - m_syTimer->timeSinceStartNsec();
        m_recFrameCount = 0;
        m_miniscope->setCaptureStartTime(stdSteadyClockStartTimepoint);
        m_evTimer->start();

        // FIXME: sometimes the Miniscope appears to forget its settings between runs,
        // even if we just have resubmitted them in the prepare() step. This is a workaround
        // to ensure we never record with e.g. gain set to zero by accident (we simply resubmit
        // the values 1sec after experiment start)
        QTimer::singleShot(1000, [&]() {
            m_settingsDialog->applyValues();
        });

        AbstractModule::start();
    }

    static void on_newRawFrame(
        const cv::Mat &mat,
        milliseconds_t &frameTime,
        const milliseconds_t &masterRecvTime,
        const milliseconds_t &deviceTime,
        const std::vector<float> &orientation,
        void *udata)
    {
        const auto self = static_cast<MiniscopeModule *>(udata);
        if (!self->m_acceptFrames) {
            self->m_acceptFrames = self->m_running ? self->m_miniscope->captureStartTimeInitialized() : false;
            if (!self->m_acceptFrames)
                return;
        }
        // use synchronizer to synchronize time
        auto updatedFrameTime = std::chrono::duration_cast<microseconds_t>(masterRecvTime);
        self->m_clockSync->processTimestamp(updatedFrameTime, deviceTime);
        frameTime = usecToMsec(updatedFrameTime);

        // we don't want to forward dropped frames
        if (mat.empty())
            return;

        self->m_rawOut->push(Frame(mat, self->m_recFrameCount++, updatedFrameTime));

        if (orientation[4] < 0.05) {
            if (self->m_lastOrientationVec == orientation)
                return;
            self->m_lastOrientationVec = orientation;

            if (self->m_bnoTabOut->active()) {
                const auto row = TableRow(
                    QStringList() << QString::number(updatedFrameTime.count()) << QString::number(orientation[0])
                                  << QString::number(orientation[1]) << QString::number(orientation[2])
                                  << QString::number(orientation[3]));
                self->m_bnoTabOut->push(row);
            }

            if (self->m_bnoVecOut->active()) {
                FloatSignalBlock sblock(1, 4);
                sblock.timestamps(0, 0) = updatedFrameTime.count();
                sblock.data(0, 0) = orientation[0];
                sblock.data(0, 1) = orientation[1];
                sblock.data(0, 2) = orientation[2];
                sblock.data(0, 3) = orientation[3];

                self->m_bnoVecOut->push(sblock);
            }
        }
    }

    static void on_newDisplayFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        const auto self = static_cast<MiniscopeModule *>(udata);
        if (!self->m_acceptFrames)
            return;
        self->m_dispOut->push(Frame(mat, msecToUsec(time)));
    }

    static void on_controlValueChanged(const QString &id, double dispValue, double devValue, void *udata)
    {
        const auto self = static_cast<MiniscopeModule *>(udata);
        if (!self->m_valChangeLogFile->isOpen())
            return;

        auto timestamp = self->m_syTimer->timeSinceStartMsec().count();
        if (!self->m_running)
            timestamp = 0;

        QTextStream tsout(self->m_valChangeLogFile);
        tsout << timestamp << ";" << id << ";" << dispValue << ";" << devValue << ";"
              << "\n";
    }

    void checkMSStatus()
    {
        if (!m_miniscope->isRunning()) {
            if (!m_miniscope->lastError().isEmpty()) {
                raiseError(m_miniscope->lastError());
                m_evTimer->stop();
                return;
            }
        }
        statusMessage(QStringLiteral("FPS: %1 Dropped: %2")
                          .arg(m_miniscope->currentFps())
                          .arg(m_miniscope->droppedFramesCount()));
        m_settingsDialog->setCurrentPixRangeValues(m_miniscope->minFluor(), m_miniscope->maxFluor());
    }

    void stop() override
    {
        m_evTimer->stop();
        m_miniscope->stop();
        m_settingsDialog->setRunning(false);

        if (m_valChangeLogFile->isOpen())
            m_valChangeLogFile->close();

        // NOTE: We do intentionally not always reconnect and disconnect the Miniscope, because
        // doing so requires the device to be power-cycled frequently to reset.
        // So once we have a stable connection, we keep the device connected forever, unless
        // an error happens or the video device ID is changed (in which case we must reconnect)

        safeStopSynchronizer(m_clockSync);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("scope_cam_id", m_miniscope->scopeCamId());
        settings.insert("device_type", m_miniscope->deviceType());
        settings.insert("orientation_indicator", m_miniscope->isBNOIndicatorVisible());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_miniscope->setScopeCamId(settings.value("scope_cam_id", 0).toInt());
        m_settingsDialog->setDeviceType(settings.value("device_type").toString());
        m_settingsDialog->setOrientationIndicatorVisible(settings.value("orientation_indicator", true).toBool());
        m_settingsDialog->readCurrentValues();
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
    return QStringLiteral("Record fluorescence images from the brain of behaving animals using a UCLA Miniscope.");
}

ModuleCategories MiniscopeModuleInfo::categories() const
{
    return ModuleCategory::DEVICES;
}

AbstractModule *MiniscopeModuleInfo::createModule(QObject *parent)
{
    return new MiniscopeModule(this, parent);
}

#include "miniscopemodule.moc"
