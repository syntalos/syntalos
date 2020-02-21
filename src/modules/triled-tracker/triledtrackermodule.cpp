/**
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "triledtrackermodule.h"

#include <QDebug>

#include "streams/frametype.h"
#include "tracker.h"

class TriLedTrackerModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    std::shared_ptr<DataStream<Frame>> m_trackStream;
    std::shared_ptr<DataStream<Frame>> m_animalStream;
    std::shared_ptr<DataStream<TableRow>> m_dataStream;

    QString m_subjectId;

    QVariantHash m_mazeInfo;

public:
    explicit TriLedTrackerModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_inPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_trackStream = registerOutputPort<Frame>(QStringLiteral("track-video"), QStringLiteral("Tracking Visualization"));
        m_animalStream = registerOutputPort<Frame>(QStringLiteral("animal-video"), QStringLiteral("Animal Visualization"));
        m_dataStream = registerOutputPort<TableRow>(QStringLiteral("track-data"), QStringLiteral("Tracking Data"));
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_THREADED;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        m_mazeInfo.clear();

        m_subjectId = testSubject.id;
        if (m_subjectId.isEmpty())
            m_subjectId = QStringLiteral("SIU"); // subject ID unknown

        m_dataStream->setSuggestedDataName(QStringLiteral("tracking/triLedTrack%1-%2").arg(index()).arg(m_subjectId));
        m_trackStream->setSuggestedDataName(QStringLiteral("tracking/trackVideo%1").arg(index()));
        m_animalStream->setSuggestedDataName(QStringLiteral("tracking/subjInfoVideo%1").arg(index()));

        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        // don't even try to do anything in case we are not subscribed to a
        // frame source
        if (!m_inPort->hasSubscription()) {
            setStateIdle();
            return;
        }
        auto frameSub = m_inPort->subscription();
        frameSub->setThrottleItemsPerSec(30); // we never want more than 30fps for tracking

        const auto outFramerate = frameSub->metadata().value(QStringLiteral("framerate"), 30);
        m_trackStream->setMetadataValue(QStringLiteral("framerate"), outFramerate);
        m_animalStream->setMetadataValue(QStringLiteral("framerate"), outFramerate);
        m_trackStream->start();
        m_animalStream->start();

        // create new tracker and have it initialize the data output stream
        auto tracker = new Tracker(m_dataStream, m_subjectId);
        if (!tracker->initialize()) {
            raiseError(tracker->lastError());
            delete tracker;
            return;
        }

        // wait until we actually start
        startWaitCondition->wait(this);

        while (m_running) {
            const auto mFrame = frameSub->next();
            // no value means the subscription has been terminated
            if (!mFrame.has_value())
                break;
            const auto frame = mFrame.value();

            cv::Mat infoMat;
            cv::Mat trackMat;
            tracker->analyzeFrame(frame.mat, frame.time, &trackMat, &infoMat);

            m_trackStream->push(Frame(trackMat, frame.time));
            m_animalStream->push(Frame(infoMat, frame.time));
        }

        m_mazeInfo = tracker->finalize();
        delete tracker;
    }

    void stop() override
    {
        statusMessage(QStringLiteral("Tracker stopped."));
        AbstractModule::stop();
    }

    QVariantHash experimentMetadata() override
    {
        if (m_mazeInfo.isEmpty())
            return QVariantHash();
        QVariantHash map;
        map.insert(QStringLiteral("MazeDimensions"), m_mazeInfo);
        return map;
    }
};

QString TriLedTrackerModuleInfo::id() const
{
    return QStringLiteral("triled-tracker");
}

QString TriLedTrackerModuleInfo::name() const
{
    return QStringLiteral("TriLED Tracker");
}

QString TriLedTrackerModuleInfo::description() const
{
    return QStringLiteral("Track subject behavior via a three-LED triangle mounted on its head.");
}

QPixmap TriLedTrackerModuleInfo::pixmap() const
{
    return QPixmap(":/module/triled-tracker");
}

AbstractModule *TriLedTrackerModuleInfo::createModule(QObject *parent)
{
    return new TriLedTrackerModule(parent);
}

#include "triledtrackermodule.moc"
