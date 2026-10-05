/**
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "triledtrackermodule.h"

#include "datactl/frametype.h"
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

public:
    explicit TriLedTrackerModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_inPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_trackStream = registerOutputPort<Frame>(
            QStringLiteral("track-video"),
            QStringLiteral("Tracking Visualization"));
        m_animalStream = registerOutputPort<Frame>(
            QStringLiteral("animal-video"),
            QStringLiteral("Animal Visualization"));
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

    bool prepare(const RunInfo &info) override
    {
        auto subjectId = info.subject.id;
        if (subjectId.isEmpty())
            subjectId = QStringLiteral("SIU"); // subject ID unknown

        m_dataStream->setSuggestedDataName(QStringLiteral("%1/triLedTrack").arg(datasetNameSuggestion()));
        m_trackStream->setSuggestedDataName(QStringLiteral("%1_trackvideo/trackVideo").arg(datasetNameSuggestion()));
        m_animalStream->setSuggestedDataName(QStringLiteral("%1_subjvid/subjInfoVideo").arg(datasetNameSuggestion()));

        // don't even try to do anything in case we are not subscribed to a
        // frame source
        if (!m_inPort->hasSubscription()) {
            setStateDormant();
            return true;
        }

        const double MAX_FPS = 30; // we never want more than 30fps for tracking
        auto frameSub = m_inPort->subscription();
        frameSub->setThrottleItemsPerSec(MAX_FPS);

        const auto outFramerate = frameSub->metadataValue(QStringLiteral("framerate"), MAX_FPS);
        m_trackStream->setMetadataValue("framerate", outFramerate);
        m_animalStream->setMetadataValue("framerate", outFramerate);
        m_trackStream->start();
        m_animalStream->start();

        // create new tracker and have it initialize the data output stream
        auto tracker = std::make_unique<Tracker>(m_dataStream, subjectId);
        if (!tracker->initialize()) {
            raiseError(tracker->lastError());
            return false;
        }

        setWorker(
            Worker{
                .frameSub = frameSub,
                .trackStream = m_trackStream,
                .animalStream = m_animalStream,
                .tracker = std::move(tracker),
            });

        return true;
    }

    /**
     * Tracks the subject in the received frames, in a dedicated thread.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<StreamSubscription<Frame>> frameSub;
        std::shared_ptr<DataStream<Frame>> trackStream;
        std::shared_ptr<DataStream<Frame>> animalStream;
        std::unique_ptr<Tracker> tracker;

        // dimensions of the maze as found by the tracker, read by the module once the run has stopped
        std::optional<MetaStringMap> mazeDimensions{};

        void run()
        {
            // wait until we actually start
            mod.waitForStart();

            while (mod.running()) {
                const auto mFrame = frameSub->next();
                // no value means the subscription has been terminated
                if (!mFrame.has_value())
                    break;
                const auto &frame = mFrame.value();

                cv::Mat infoMat;
                cv::Mat trackMat;
                cv::Mat frameMat = frame.mat;
                tracker->analyzeFrame(frameMat, usecToMsec(frame.time), &trackMat, &infoMat);

                trackStream->push(Frame(trackMat, frame.time));
                animalStream->push(Frame(infoMat, frame.time));
            }

            mazeDimensions = tracker->finalize();
        }
    };

    void stop() override
    {
        // our thread has finished at this point, so we can fetch the result of the tracker
        if (auto worker = takeWorker<Worker>(); worker && worker->mazeDimensions) {
            // store maze dimension metadata - since or metadata storage suggestion to possible
            // table-saving modules is to store data in a set named after our module, we will
            // possibly not create our default dataset here but instead fetch an already existing one.
            // in that event, we "hijack" the dataset and add a few more attributes to it.
            auto dset = createDefaultDataset();
            if (dset.get() != nullptr)
                dset->insertAttribute("maze_dimensions", *worker->mazeDimensions);
        }

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

ModuleCategories TriLedTrackerModuleInfo::categories() const
{
    return ModuleCategory::PROCESSING;
}

AbstractModule *TriLedTrackerModuleInfo::createModule(QObject *parent)
{
    return new TriLedTrackerModule(parent);
}

#include "triledtrackermodule.moc"
