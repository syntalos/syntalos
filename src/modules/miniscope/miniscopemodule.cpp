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
    QString m_recStorageDir;
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

        if (name() == QStringLiteral("Miniscope"))
            m_settingsDialog->setRecName(QStringLiteral("msSlice"));

        m_miniscope->setScopeCamId(0);
        setName(name());

        m_miniscope->setOnFrame(&on_newRawFrame, this);
        m_miniscope->setOnDisplayFrame(&on_newDisplayFrame, this);

        m_evTimer = new QTimer(this);
        m_evTimer->setInterval(0);
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

    bool prepare(const QString &storageRootDir, const TestSubject &) override
    {
        m_recStorageDir = QStringLiteral("%1/miniscope").arg(storageRootDir);
        if (!makeDirectory(m_recStorageDir))
            return false;
        if (m_settingsDialog->recName().isEmpty()) {
            raiseError("Miniscope recording name is empty. Please specify it in the settings to continue.");
            return false;
        }

        m_miniscope->setVideoFilename(QStringLiteral("%1/%2").arg(m_recStorageDir).arg(m_settingsDialog->recName()).toStdString());

        if (!m_miniscope->connect()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }

        m_rawOut->setMetadataVal("framerate", m_miniscope->fps());
        m_dispOut->setMetadataVal("framerate", m_miniscope->fps());

        // start the streams
        m_rawOut->start(name());
        m_dispOut->start(name());

        // we already start capturing video here, and only launch the recording when later
        if (!m_miniscope->run()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }

        return true;
    }

    void start() override
    {
        m_miniscope->setCaptureStartTimepoint(m_timer->startTime());
        if (!m_miniscope->startRecording())
            raiseError(QString::fromStdString(m_miniscope->lastError()));
        m_evTimer->start();
    }

    static void on_newRawFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        auto self = static_cast<MiniscopeModule*>(udata);
        self->m_rawOut->push(Frame(mat, time));
    }

    static void on_newDisplayFrame(const cv::Mat &mat, const milliseconds_t &time, void *udata)
    {
        auto self = static_cast<MiniscopeModule*>(udata);
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

        return jsonObjectToBytes(jset);
    }

    bool loadSettings(const QString &, const QByteArray &data) override
    {
        auto jset = jsonObjectFromBytes(data);
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
    return QStringLiteral("Record fluorescence images using a UCLA MiniScope.");
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
