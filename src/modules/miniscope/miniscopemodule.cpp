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

        m_rawOut->setMetadataValue("framerate", m_miniscope->fps());
        m_rawOut->setMetadataValue("suggestedDataName", QStringLiteral("%1/msSlice").arg(name().toLower()));
        m_rawOut->setMetadataValue("hasColor", false);

        m_dispOut->setMetadataValue("framerate", m_miniscope->fps());
        m_dispOut->setMetadataValue("suggestedDataName", QStringLiteral("%1/msDisplaySlice").arg(name().toLower()));
        m_dispOut->setMetadataValue("hasColor", false);

        // start the streams
        m_rawOut->start();
        m_dispOut->start();

        // we already start capturing video here, and only start emitting frames later
        if (!m_miniscope->run()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }

        return true;
    }

    void start() override
    {
        // FIXME: We are setting the wrong time offset here! The libminiscope code needs a complete overhaul of its timing functions
        m_miniscope->setCaptureTimeOffset(-1 * std::chrono::duration_cast<std::chrono::nanoseconds>(m_syTimer->startTime().time_since_epoch()));
        m_evTimer->start();

        AbstractModule::start();
    }

    static void on_newRawFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        auto self = static_cast<MiniscopeModule*>(udata);
        if (self->m_running)
            self->m_rawOut->push(Frame(mat, time));
    }

    static void on_newDisplayFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        auto self = static_cast<MiniscopeModule*>(udata);
        if (self->m_running)
            self->m_dispOut->push(Frame(mat, time));
    }

    void checkMSStatus()
    {
        statusMessage(QStringLiteral("FPS: %1 Dropped: %2").arg(m_miniscope->currentFps()).arg(m_miniscope->droppedFramesCount()));
    }

    void stop() override
    {
        m_evTimer->stop();
        m_miniscope->stopRecording();
        m_miniscope->stop();
        m_miniscope->disconnect();
    }

    QByteArray serializeSettings(const QString &) override
    {
        QJsonObject jset;

        jset.insert("scopeCamId", m_miniscope->scopeCamId());
        jset.insert("fps", static_cast<int>(m_miniscope->fps()));

        return jsonObjectToBytes(jset);
    }

    bool loadSettings(const QString &, const QByteArray &data) override
    {
        auto jset = jsonObjectFromBytes(data);

        m_miniscope->setScopeCamId(jset.value("scopeCamId").toInt(0));
        m_miniscope->setFps(jset.value("fps").toInt(20));
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
