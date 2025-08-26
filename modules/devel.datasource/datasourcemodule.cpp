/*
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

#include "datasourcemodule.h"

#include <format>
#include <QInputDialog>
#include <opencv2/opencv.hpp>
#include "datactl/frametype.h"
#include "utils/misc.h"

SYNTALOS_MODULE(DevelDataSourceModule)

class DataSourceModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<DataStream<Frame>> m_frameOut;
    std::shared_ptr<DataStream<TableRow>> m_rowsOut;
    std::shared_ptr<DataStream<FirmataControl>> m_fctlOut;

    std::shared_ptr<DataStream<FloatSignalBlock>> m_floatOut;
    std::shared_ptr<DataStream<IntSignalBlock>> m_intOut;

    int m_fps;
    QSize m_outFrameSize;
    bool m_colorVideo;
    microseconds_t m_prevFrameTime;

    time_t m_prevRowTime;
    time_t m_prevTimeSData;
    int m_prevIntValue;

public:
    explicit DataSourceModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_fps(200),
          m_outFrameSize(QSize(960, 600)),
          m_colorVideo(true)
    {
        m_frameOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Frames"));
        m_rowsOut = registerOutputPort<TableRow>(QStringLiteral("rows-out"), QStringLiteral("Table Rows"));
        m_fctlOut = registerOutputPort<FirmataControl>(QStringLiteral("fctl-out"), QStringLiteral("Firmata Control"));
        m_floatOut = registerOutputPort<FloatSignalBlock>(QStringLiteral("float-out"), QStringLiteral("Sines"));
        m_intOut = registerOutputPort<IntSignalBlock>(QStringLiteral("int-out"), QStringLiteral("Numbers"));
    }

    ~DataSourceModule() override {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::NONE | ModuleFeature::SHOW_SETTINGS;
    }

    void showSettingsUi() override
    {
        if (m_running)
            return;

        bool ok;
        auto intVal = QInputDialog::getInt(
            nullptr, "Configure Debug Data Source", "Video Framerate", m_fps, 2, 10000, 1, &ok);
        if (ok)
            m_fps = intVal;
    }

    bool prepare(const TestSubject &) override
    {
        m_frameOut->setMetadataValue("framerate", (double)m_fps);
        m_frameOut->setMetadataValue("size", m_outFrameSize);
        m_frameOut->start();
        m_prevFrameTime = microseconds_t(0);

        m_rowsOut->setSuggestedDataName(QStringLiteral("table-%1/testvalues").arg(datasetNameSuggestion()));
        m_rowsOut->setMetadataValue(
            "table_header",
            QStringList() << QStringLiteral("Time") << QStringLiteral("Tag") << QStringLiteral("Value"));
        m_rowsOut->start();
        m_prevRowTime = 0;

        m_prevTimeSData = 0;
        m_floatOut->setMetadataValue(
            "signal_names",
            QStringList() << "Sine 1"
                          << "Sine 2"
                          << "Sine 3");
        m_floatOut->setMetadataValue("time_unit", "microseconds");
        m_floatOut->setMetadataValue("data_unit", "au");
        m_floatOut->start();

        m_intOut->setMetadataValue("signal_names", QStringList() << "Int 1");
        m_intOut->setMetadataValue("time_unit", "microseconds");
        m_intOut->setMetadataValue("data_unit", "au");
        m_intOut->start();

        m_fctlOut->start();

        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        startWaitCondition->wait(this);

        size_t dataIndex = 0;
        while (m_running) {
            m_frameOut->push(createFrame_sleep(dataIndex, m_fps));

            auto row = createTablerow();
            if (row.has_value())
                m_rowsOut->push(row.value());

            const auto usec = m_syTimer->timeSinceStartUsec().count();
            const auto msec = m_syTimer->timeSinceStartMsec().count();
            if (((msec / 1000) % 3) == 0) {
                FirmataControl fctl;

                fctl.command = FirmataCommandKind::WRITE_DIGITAL;
                fctl.pinId = 2;
                fctl.pinName = QStringLiteral("custom-pin-name");
                fctl.value = ((msec / 1000) % 2 == 0) ? 1 : 0;
                m_fctlOut->push(fctl);
            }

            if ((usec > m_prevTimeSData) && (msec % 2) == 0) {
                FloatSignalBlock fsb(2, 3);
                fsb.timestamps[0] = usec - 1;
                fsb.timestamps[1] = usec;
                fsb.data(0, 0) = 0.5f * sinf(50 * ((msec - 1) / 20));
                fsb.data(1, 0) = 0.5f * sinf(50 * (msec / 20));

                fsb.data(0, 1) = 0.25f * sinf(50 * ((msec - 1) / 5) + 1.5);
                fsb.data(1, 1) = 0.25f * sinf(50 * (msec / 5) + 1.5);

                fsb.data(0, 2) = 0.4f * sinf(50 * ((msec - 1) / 200));
                fsb.data(1, 2) = 0.4f * sinf(50 * (msec / 200));
                m_floatOut->push(fsb);

                IntSignalBlock isb(2, 1);
                isb.timestamps[0] = usec - 1;
                isb.timestamps[1] = usec;
                if (m_prevIntValue > 10) {
                    isb.data(0, 0) = 8;
                    isb.data(1, 0) = 2;
                    m_prevIntValue = 0;
                } else {
                    isb.data(0, 0) = m_prevIntValue;
                    isb.data(1, 0) = m_prevIntValue;
                    m_prevIntValue++;
                }
                m_intOut->push(isb);

                m_prevTimeSData = usec;
            }

            dataIndex++;
        }
    }

private:
    Frame createFrame_sleep(size_t index, int fps)
    {
        const auto targetIntervalUsec = microseconds_t(static_cast<long>(std::round(1000000.0 / fps)));

        // time when the next frame should be output
        const auto nextFrameTime = m_prevFrameTime + targetIntervalUsec;
        const auto startTime = m_syTimer->timeSinceStartUsec();

        // sleep until it's time for the next frame (if we're ahead of schedule)
        if (startTime < nextFrameTime) {
            const auto sleepDuration = nextFrameTime - startTime;
            if (startTime.count() > 0)
                std::this_thread::sleep_for(sleepDuration);
        }

        const auto width = m_outFrameSize.width();
        const auto height = m_outFrameSize.height();

        // empty image with blue background
        cv::Mat image(height, width, CV_8UC3, cv::Scalar(67, 42, 30));

        // green rectangle
        cv::rectangle(image, cv::Point(10, 10), cv::Point(width - 10, height - 10), cv::Scalar(96, 174, 40), 4);

        // vertical and horizontal orange lines
        cv::line(image, cv::Point(width / 2, 0), cv::Point(width / 2, height), cv::Scalar(0, 116, 247), 4);
        cv::line(image, cv::Point(0, height / 2), cv::Point(width, height / 2), cv::Scalar(0, 116, 247), 4);

        // add text with frame index
        cv::putText(
            image,
            "Frame: " + std::to_string(index),
            cv::Point(24, 240),
            cv::FONT_HERSHEY_SIMPLEX,
            1.2,
            cv::Scalar(249, 249, 249),
            2,
            cv::LINE_AA);

        // create black/white video if needed
        if (!m_colorVideo)
            cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);

        Frame frame(index);
        frame.time = nextFrameTime;
        frame.mat = image;

        m_prevFrameTime = frame.time;
        return frame;
    }

    std::optional<TableRow> createTablerow()
    {
        const auto msec = m_syTimer->timeSinceStartMsec().count();
        if ((msec - m_prevRowTime) < 4000)
            return std::nullopt;
        m_prevRowTime = msec;

        TableRow row;
        row.reserve(3);
        row.append(QString::number(msec));
        row.append((msec % 2) ? QStringLiteral("beta") : QStringLiteral("alpha"));
        row.append(createRandomString(14));

        return row;
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

QIcon DevelDataSourceModuleInfo::icon() const
{
    return QIcon(":/module/devel");
}

ModuleCategories DevelDataSourceModuleInfo::categories() const
{
    return ModuleCategory::SYNTALOS_DEV;
}

AbstractModule *DevelDataSourceModuleInfo::createModule(QObject *parent)
{
    return new DataSourceModule(parent);
}

#include "datasourcemodule.moc"
