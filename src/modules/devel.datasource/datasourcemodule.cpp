/*
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

#include "datasourcemodule.h"
#include "streams/frametype.h"

#include <opencv2/imgproc.hpp>

class DataSourceModule : public AbstractModule
{
private:
    std::shared_ptr<DataStream<Frame>> m_frameOut;

public:
    explicit DataSourceModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_frameOut = registerOutputPort<Frame>("frames-out", "Frames");
    }

    ~DataSourceModule() override
    {

    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::RUN_THREADED;
    }

    bool prepare(const QString &, const TestSubject &) override
    {
        m_frameOut->setMetadataVal("src_mod_name", name());
        m_frameOut->setMetadataVal("framerate", 200);
        m_frameOut->setMetadataVal("size", QSize(800, 600));
        m_frameOut->start();

        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        startWaitCondition->wait(this);

        size_t dataIndex = 0;
        while (m_running) {
            m_frameOut->push(create_frames_200Hz(dataIndex));

            dataIndex++;
        }
    }

private:
    Frame create_frames_200Hz(size_t index)
    {
        Frame frame;

        frame.mat = cv::Mat(cv::Size(800, 600), CV_8UC3);
        frame.mat.setTo(cv::Scalar(67, 42, 30));
        cv::putText(frame.mat,
                    std::string("Frame ") + std::to_string(index),
                    cv::Point(24, 240),
                    cv::FONT_HERSHEY_COMPLEX,
                    1.5,
                    cv::Scalar(255,255,255));
        frame.time = m_timer->timeSinceStartMsec();

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return frame;
    }
};

QString DevelDataSourceModuleInfo::id() const
{
    return QStringLiteral("devel.datasource");
}

QString DevelDataSourceModuleInfo::name() const
{
    return QStringLiteral("Devel: DataSource");
}

QString DevelDataSourceModuleInfo::description() const
{
    return QStringLiteral("Developer module generating different artificial data.");
}

QPixmap DevelDataSourceModuleInfo::pixmap() const
{
    return QPixmap(":/module/devel");
}

AbstractModule *DevelDataSourceModuleInfo::createModule(QObject *parent)
{
    return new DataSourceModule(parent);
}
