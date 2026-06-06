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

#include <algorithm>
#include <cmath>
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
    std::shared_ptr<DataStream<LineCommand>> m_fctlOut;

    std::shared_ptr<DataStream<SignalBlockF32>> m_floatOut;
    std::shared_ptr<DataStream<SignalBlockI32>> m_intOut;
    std::shared_ptr<DataStream<SignalBlockU16>> m_uint16Out;

    int m_fps;
    QSize m_outFrameSize;
    bool m_colorVideo;
    microseconds_t m_prevFrameTime;

    time_t m_prevRowTime;

    // sample-rate driven, deterministic signal generation
    double m_sampleRate;
    double m_freqLow;
    double m_freqHigh;
    uint64_t m_sampleCount;

public:
    explicit DataSourceModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_fps(200),
          m_outFrameSize(QSize(960, 600)),
          m_colorVideo(true),
          m_sampleRate(2000.0),
          m_freqLow(10.0),
          m_freqHigh(300.0),
          m_sampleCount(0)
    {
        m_frameOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Frames"));
        m_rowsOut = registerOutputPort<TableRow>(QStringLiteral("rows-out"), QStringLiteral("Table Rows"));
        m_fctlOut = registerOutputPort<LineCommand>(QStringLiteral("fctl-out"), QStringLiteral("Line Control"));
        m_floatOut = registerOutputPort<SignalBlockF32>(QStringLiteral("float-out"), QStringLiteral("Sines"));
        m_intOut = registerOutputPort<SignalBlockI32>(QStringLiteral("int-out"), QStringLiteral("Numbers"));
        m_uint16Out = registerOutputPort<SignalBlockU16>(QStringLiteral("uint16-out"), QStringLiteral("U16 Counters"));
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
            nullptr,
            "Configure Debug Data Source",
            "Video Framerate",
            m_fps,
            2,
            10000,
            1,
            &ok);
        if (ok)
            m_fps = intVal;
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("fps"), m_fps);
        settings.insert(QStringLiteral("color_video"), m_colorVideo);
        settings.insert(QStringLiteral("sample_rate"), m_sampleRate);
        settings.insert(QStringLiteral("test_freq_low"), m_freqLow);
        settings.insert(QStringLiteral("test_freq_high"), m_freqHigh);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        const int fps = settings.value(QStringLiteral("fps"), 200).toInt();
        m_fps = std::clamp(fps, 2, 10000);
        m_colorVideo = settings.value(QStringLiteral("color_video"), true).toBool();
        m_sampleRate = std::clamp(settings.value(QStringLiteral("sample_rate"), 2000.0).toDouble(), 1.0, 1000000.0);
        m_freqLow = settings.value(QStringLiteral("test_freq_low"), 10.0).toDouble();
        m_freqHigh = settings.value(QStringLiteral("test_freq_high"), 300.0).toDouble();

        return true;
    }

    bool prepare(const TestSubject &) override
    {
        m_frameOut->setMetadataValue("framerate", (double)m_fps);
        m_frameOut->setMetadataValue("size", MetaSize(m_outFrameSize.width(), m_outFrameSize.height()));
        m_frameOut->start();
        m_prevFrameTime = microseconds_t(0);

        m_rowsOut->setSuggestedDataName(QStringLiteral("table-%1/testvalues").arg(datasetNameSuggestion()));
        m_rowsOut->setMetadataValue("table_header", MetaArray{"Time", "Tag", "Value"});
        m_rowsOut->start();
        m_prevRowTime = 0;

        m_sampleCount = 0;
        m_floatOut->setMetadataValue("signal_names", MetaArray{"Low", "High", "Low+High"});
        m_floatOut->setMetadataValue("time_unit", "microseconds");
        m_floatOut->setMetadataValue("data_unit", "au");
        m_floatOut->setMetadataValue("sample_rate", m_sampleRate);
        m_floatOut->start();

        m_intOut->setMetadataValue("signal_names", MetaArray{"Int Low"});
        m_intOut->setMetadataValue("time_unit", "microseconds");
        m_intOut->setMetadataValue("data_unit", "au");
        m_intOut->setMetadataValue("sample_rate", m_sampleRate);
        m_intOut->start();

        m_uint16Out->setMetadataValue("signal_names", MetaArray{"U16 Low", "U16 High"});
        m_uint16Out->setMetadataValue("time_unit", "microseconds");
        m_uint16Out->setMetadataValue("data_unit", "au");
        m_uint16Out->setMetadataValue("sample_rate", m_sampleRate);
        m_uint16Out->start();

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

            const auto msec = m_syTimer->timeSinceStartMsec().count();
            if (((msec / 1000) % 3) == 0) {
                LineCommand lcmd(LineCommandKind::WRITE_DIGITAL, 2);
                lcmd.value = ((msec / 1000) % 2 == 0) ? 1 : 0;
                m_fctlOut->push(lcmd);
            }

            // Deterministic, sample-rate-driven signal generation. Each value is a
            // pure function of the running sample index, so a recorded run is exactly
            // reproducible regardless of wall-clock pacing. One block of blockLen
            // samples is emitted per loop iteration; the loop is paced to m_fps by
            // the frame sleep above, so the effective rate is ~m_fps*blockLen.
            const int blockLen = std::max(1, static_cast<int>(std::lround(m_sampleRate / m_fps)));

            SignalBlockF32 fsb(blockLen, 3);
            SignalBlockI32 isb(blockLen, 1);
            SignalBlockU16 usb(blockLen, 2);
            for (int i = 0; i < blockLen; ++i) {
                const uint64_t n = m_sampleCount + static_cast<uint64_t>(i);
                const double t = static_cast<double>(n) / m_sampleRate;
                const uint64_t ts = static_cast<uint64_t>(std::llround(static_cast<double>(n) * 1e6 / m_sampleRate));

                const double lo = 0.5 * std::sin(2.0 * M_PI * m_freqLow * t);
                const double hi = 0.5 * std::sin(2.0 * M_PI * m_freqHigh * t);

                fsb.timestamps[i] = ts;
                fsb.data(i, 0) = static_cast<float>(lo);
                fsb.data(i, 1) = static_cast<float>(hi);
                fsb.data(i, 2) = static_cast<float>(lo + hi);

                isb.timestamps[i] = ts;
                isb.data(i, 0) = static_cast<int32_t>(std::lround(1000.0 * lo));

                usb.timestamps[i] = ts;
                usb.data(i, 0) = static_cast<uint16_t>(std::lround(2000.0 + 1000.0 * lo));
                usb.data(i, 1) = static_cast<uint16_t>(std::lround(2000.0 + 1000.0 * hi));
            }
            m_sampleCount += static_cast<uint64_t>(blockLen);

            m_floatOut->push(std::move(fsb));
            m_intOut->push(std::move(isb));
            m_uint16Out->push(std::move(usb));

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
            "Frame: " + numToString(index),
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
        if ((msec - m_prevRowTime) < 2000)
            return std::nullopt;
        m_prevRowTime = msec;

        TableRow row;
        row.reserve(3);
        row.append(numToString(msec));
        row.append((msec % 2) ? std::string("beta") : std::string("alpha"));
        row.append(createRandomString(14).toStdString());

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
