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

SYNTALOS_MODULE(TriLedTrackerModule)

class TriLedTrackerModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<Frame>> m_inPort;
    std::shared_ptr<DataStream<Frame>> m_trackStream;
    std::shared_ptr<DataStream<Frame>> m_animalStream;
    std::shared_ptr<DataStream<TableRow>> m_dataStream;

    QString m_subjectId;

public:
    explicit TriLedTrackerModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_inPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_trackStream = registerOutputPort<Frame>(QStringLiteral("track-video"), QStringLiteral("Tracking Visualization"));
        m_animalStream = registerOutputPort<Frame>(QStringLiteral("animal-video"), QStringLiteral("Animal Visualization"));
        m_dataStream = registerOutputPort<TableRow>(QStringLiteral("track-data"), QStringLiteral("Tracking Data"));
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::NONE;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        m_subjectId = testSubject.id;
        if (m_subjectId.isEmpty())
            m_subjectId = QStringLiteral("SIU"); // subject ID unknown

        m_dataStream->setSuggestedDataName(QStringLiteral("%1/triLedTrack").arg(datasetNameSuggestion()));
        m_trackStream->setSuggestedDataName(QStringLiteral("%1_trackvideo/trackVideo").arg(datasetNameSuggestion()));
        m_animalStream->setSuggestedDataName(QStringLiteral("%1_subjvid/subjInfoVideo").arg(datasetNameSuggestion()));

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

        const double MAX_FPS = 30; // we never want more than 30fps for tracking
        auto frameSub = m_inPort->subscription();
        frameSub->setThrottleItemsPerSec(MAX_FPS);

        const auto outFramerate = frameSub->metadataValue(QStringLiteral("framerate"), MAX_FPS);
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

        // store maze dimension metadata - since or metadata storage suggestion to possible
        // table-saving modules is to store data in a set named after our module, we will
        // possibly not create our default dataset here but instead fetch an already existing one.
        // in that event, we "hijack" the dataset and add a few more attributes to it.
        auto dset = getOrCreateDefaultDataset();
        dset->insertAttribute("maze_dimensions", tracker->finalize());

        delete tracker;
    }

    void stop() override
    {
        statusMessage(QStringLiteral("Tracker stopped."));
        AbstractModule::stop();
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

QIcon TriLedTrackerModuleInfo::icon() const
{
    return QIcon(":/module/triled-tracker");
}

AbstractModule *TriLedTrackerModuleInfo::createModule(QObject *parent)
{
    return new TriLedTrackerModule(parent);
}

#include "triledtrackermodule.moc"
