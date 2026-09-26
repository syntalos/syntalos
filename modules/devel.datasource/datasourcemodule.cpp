/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <random>
#include <QFormLayout>
#include <QSpinBox>
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
    std::shared_ptr<DataStream<LineCommand>> m_lcmdOut;
    std::shared_ptr<DataStream<LineReading>> m_lrdOut;

    std::shared_ptr<DataStream<SignalBlockF32>> m_floatOut;
    std::shared_ptr<DataStream<SignalBlockI32>> m_int32Out;
    std::shared_ptr<DataStream<SignalBlockI16>> m_int16Out;
    std::shared_ptr<DataStream<SignalBlockU16>> m_uint16Out;

    enum class FrameContent {
        TEST_CARD, /// flat synthetic test card
        CAMERA     /// smooth, slowly changing image with sensor-like noise
    };

    /// Weights of the low/high test frequency for each channel of a signal port
    using ChannelMix = std::vector<std::pair<double, double>>;

    int m_fps;
    QSize m_outFrameSize;
    bool m_colorVideo;
    FrameContent m_frameContent;
    cv::Mat m_testCard;
    cv::Mat m_scene;
    std::vector<cv::Mat> m_noise;
    microseconds_t m_prevFrameTime;

    time_t m_prevRowTime;
    int m_rowsPerTick; /// table rows emitted per frame tick, 0 = one row every two seconds

    // sample-rate driven, deterministic signal generation
    double m_sampleRate;
    double m_freqLow;
    double m_freqHigh;
    uint64_t m_sampleCount;

    // channel count of all signal ports, zero selects the default channel layout
    int m_signalChannels;
    double m_noiseLevel;
    std::vector<float> m_noiseTable; /// deterministic Gaussian noise, indexed by sample and channel
    ChannelMix m_floatMix;
    ChannelMix m_int32Mix;
    ChannelMix m_int16Mix;
    ChannelMix m_uint16Mix;

    // Edge-triggered digital line state for the LineReading output
    static constexpr int kNumLines = 3;
    int m_lineState[kNumLines];

public:
    explicit DataSourceModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_fps(200),
          m_outFrameSize(QSize(960, 600)),
          m_colorVideo(true),
          m_frameContent(FrameContent::TEST_CARD),
          m_rowsPerTick(0),
          m_sampleRate(2000.0),
          m_freqLow(10.0),
          m_freqHigh(300.0),
          m_sampleCount(0),
          m_signalChannels(0),
          m_noiseLevel(0.1)
    {
        m_frameOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Frames"));
        m_rowsOut = registerOutputPort<TableRow>(QStringLiteral("rows-out"), QStringLiteral("Table Rows"));
        m_lcmdOut = registerOutputPort<LineCommand>(QStringLiteral("linecmd-out"), QStringLiteral("Line Control"));
        m_lrdOut = registerOutputPort<LineReading>(QStringLiteral("linerd-out"), QStringLiteral("Line Readings"));
        m_floatOut = registerOutputPort<SignalBlockF32>(QStringLiteral("float-out"), QStringLiteral("Floats"));
        m_int32Out = registerOutputPort<SignalBlockI32>(QStringLiteral("i32-out"), QStringLiteral("I32 Integers"));
        m_int16Out = registerOutputPort<SignalBlockI16>(QStringLiteral("i16-out"), QStringLiteral("I16 Integers"));
        m_uint16Out = registerOutputPort<SignalBlockU16>(QStringLiteral("u16-out"), QStringLiteral("U16 Integers"));
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

        QDialog dlg;
        dlg.setWindowTitle(QStringLiteral("Configure Debug Data Source"));
        auto layout = new QFormLayout(&dlg);

        auto fpsSpin = new QSpinBox(&dlg);
        fpsSpin->setRange(2, 10000);
        fpsSpin->setValue(m_fps);
        layout->addRow(QStringLiteral("Video Framerate"), fpsSpin);

        auto widthSpin = new QSpinBox(&dlg);
        widthSpin->setRange(kMinFrameEdge, kMaxFrameEdge);
        widthSpin->setValue(m_outFrameSize.width());
        layout->addRow(QStringLiteral("Frame Width"), widthSpin);

        auto heightSpin = new QSpinBox(&dlg);
        heightSpin->setRange(kMinFrameEdge, kMaxFrameEdge);
        heightSpin->setValue(m_outFrameSize.height());
        layout->addRow(QStringLiteral("Frame Height"), heightSpin);

        auto contentCombo = new QComboBox(&dlg);
        contentCombo->addItem(QStringLiteral("Test Card"), frameContentToString(FrameContent::TEST_CARD));
        contentCombo->addItem(QStringLiteral("Camera-like"), frameContentToString(FrameContent::CAMERA));
        contentCombo->setCurrentIndex(contentCombo->findData(frameContentToString(m_frameContent)));
        layout->addRow(QStringLiteral("Frame Content"), contentCombo);

        auto colorCheck = new QCheckBox(&dlg);
        colorCheck->setChecked(m_colorVideo);
        layout->addRow(QStringLiteral("Color Video"), colorCheck);

        auto rateSpin = new QDoubleSpinBox(&dlg);
        rateSpin->setRange(1.0, kMaxSampleRate);
        rateSpin->setDecimals(0);
        rateSpin->setSuffix(QStringLiteral(" Hz"));
        rateSpin->setValue(m_sampleRate);
        layout->addRow(QStringLiteral("Signal Sample Rate"), rateSpin);

        auto channelsSpin = new QSpinBox(&dlg);
        channelsSpin->setRange(0, kMaxSignalChannels);
        channelsSpin->setSpecialValueText(QStringLiteral("Default"));
        channelsSpin->setValue(m_signalChannels);
        layout->addRow(QStringLiteral("Signal Channels"), channelsSpin);

        auto noiseSpin = new QDoubleSpinBox(&dlg);
        noiseSpin->setRange(0.0, 1.0);
        noiseSpin->setSingleStep(0.05);
        noiseSpin->setDecimals(2);
        noiseSpin->setToolTip(QStringLiteral(
            "Gaussian noise added to every signal channel, relative to the signal "
            "amplitude. Makes the data compress like real recordings."));
        noiseSpin->setValue(m_noiseLevel);
        layout->addRow(QStringLiteral("Signal Noise"), noiseSpin);

        auto rowsSpin = new QSpinBox(&dlg);
        rowsSpin->setRange(0, kMaxRowsPerTick);
        rowsSpin->setSpecialValueText(QStringLiteral("One every 2 s"));
        rowsSpin->setToolTip(
            QStringLiteral("Table rows emitted on every video frame tick, to generate many small items."));
        rowsSpin->setValue(m_rowsPerTick);
        layout->addRow(QStringLiteral("Table Rows per Tick"), rowsSpin);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        layout->addRow(buttons);

        if (dlg.exec() != QDialog::Accepted)
            return;

        m_fps = fpsSpin->value();
        m_outFrameSize = QSize(widthSpin->value(), heightSpin->value());
        m_frameContent = frameContentFromString(contentCombo->currentData().toString());
        m_colorVideo = colorCheck->isChecked();
        m_sampleRate = rateSpin->value();
        m_signalChannels = channelsSpin->value();
        m_noiseLevel = noiseSpin->value();
        m_rowsPerTick = rowsSpin->value();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("fps"), m_fps);
        settings.insert(QStringLiteral("color_video"), m_colorVideo);
        settings.insert(QStringLiteral("frame_width"), m_outFrameSize.width());
        settings.insert(QStringLiteral("frame_height"), m_outFrameSize.height());
        settings.insert(QStringLiteral("frame_content"), frameContentToString(m_frameContent));
        settings.insert(QStringLiteral("signal_channels"), m_signalChannels);
        settings.insert(QStringLiteral("noise_level"), m_noiseLevel);
        settings.insert(QStringLiteral("sample_rate"), m_sampleRate);
        settings.insert(QStringLiteral("test_freq_low"), m_freqLow);
        settings.insert(QStringLiteral("test_freq_high"), m_freqHigh);
        settings.insert(QStringLiteral("rows_per_tick"), m_rowsPerTick);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        const int fps = settings.value(QStringLiteral("fps"), 200).toInt();
        m_fps = std::clamp(fps, 2, 10000);
        m_colorVideo = settings.value(QStringLiteral("color_video"), true).toBool();
        m_outFrameSize = QSize(
            std::clamp(settings.value(QStringLiteral("frame_width"), 960).toInt(), kMinFrameEdge, kMaxFrameEdge),
            std::clamp(settings.value(QStringLiteral("frame_height"), 600).toInt(), kMinFrameEdge, kMaxFrameEdge));
        m_frameContent = frameContentFromString(settings.value(QStringLiteral("frame_content")).toString());
        m_signalChannels = std::clamp(
            settings.value(QStringLiteral("signal_channels"), 0).toInt(),
            0,
            kMaxSignalChannels);
        m_sampleRate = std::clamp(
            settings.value(QStringLiteral("sample_rate"), 2000.0).toDouble(),
            1.0,
            kMaxSampleRate);
        m_noiseLevel = std::clamp(settings.value(QStringLiteral("noise_level"), 0.1).toDouble(), 0.0, 1.0);
        m_freqLow = settings.value(QStringLiteral("test_freq_low"), 10.0).toDouble();
        m_freqHigh = settings.value(QStringLiteral("test_freq_high"), 300.0).toDouble();
        m_rowsPerTick = std::clamp(settings.value(QStringLiteral("rows_per_tick"), 0).toInt(), 0, kMaxRowsPerTick);

        return true;
    }

    bool prepare(const RunInfo &) override
    {
        m_frameOut->setMetadataValue("framerate", (double)m_fps);
        m_frameOut->setMetadataValue("size", MetaSize(m_outFrameSize.width(), m_outFrameSize.height()));
        m_frameOut->start();
        m_prevFrameTime = microseconds_t(0);
        m_testCard.release();
        m_scene.release();
        m_noise.clear();
        if (m_frameOut->hasSubscribers()) {
            if (m_frameContent == FrameContent::CAMERA)
                createCameraScene();
            else
                createTestCard();
        }

        m_rowsOut->setSuggestedDataName(QStringLiteral("table-%1/testvalues").arg(datasetNameSuggestion()));
        m_rowsOut->setMetadataValue("table_header", MetaArray{"Time", "Tag", "Value"});
        m_rowsOut->start();
        m_prevRowTime = 0;

        m_sampleCount = 0;
        setupSignalChannels(
            m_floatOut,
            m_floatMix,
            {
                "Low",
                "High",
                "Low+High"
        },
            {{1, 0}, {0, 1}, {1, 1}});
        m_floatOut->setMetadataValue("time_unit", "microseconds");
        m_floatOut->setMetadataValue("data_unit", "au");
        m_floatOut->setMetadataValue("sample_rate", m_sampleRate);
        m_floatOut->start();

        setupSignalChannels(
            m_int32Out,
            m_int32Mix,
            {
                "Int Low"
        },
            {{1, 0}});
        m_int32Out->setMetadataValue("time_unit", "microseconds");
        m_int32Out->setMetadataValue("data_unit", "au");
        m_int32Out->setMetadataValue("sample_rate", m_sampleRate);
        m_int32Out->start();

        setupSignalChannels(
            m_int16Out,
            m_int16Mix,
            {
                "I16 Low",
                "I16 High"
        },
            {{1, 0}, {0, 1}});
        m_int16Out->setMetadataValue("time_unit", "microseconds");
        m_int16Out->setMetadataValue("data_unit", "au");
        m_int16Out->setMetadataValue("sample_rate", m_sampleRate);
        m_int16Out->start();

        setupSignalChannels(
            m_uint16Out,
            m_uint16Mix,
            {
                "U16 Low",
                "U16 High"
        },
            {{1, 0}, {0, 1}});
        m_uint16Out->setMetadataValue("time_unit", "microseconds");
        m_uint16Out->setMetadataValue("data_unit", "au");
        m_uint16Out->setMetadataValue("sample_rate", m_sampleRate);
        m_uint16Out->start();

        m_lcmdOut->start();

        for (int i = 0; i < kNumLines; ++i)
            m_lineState[i] = -1; // force an initial reading on the first evaluation
        m_lrdOut->setMetadataValue("time_unit", "microseconds");
        m_lrdOut->setMetadataValue("data_unit", "ttl");
        m_lrdOut->setMetadataValue("is_digital", true);
        m_lrdOut->start();

        return true;
    }

    void runThread(OptionalWaitCondition *startWaitCondition) override
    {
        startWaitCondition->wait(this);

        size_t dataIndex = 0;
        while (m_running) {
            // we always pace the loop by the framerate, but only create data that somebody wants
            const auto frameTime = waitForNextFrameTime(m_fps);
            if (m_frameOut->hasSubscribers())
                m_frameOut->push(createFrame(dataIndex, frameTime));

            if (m_rowsPerTick > 0) {
                for (int i = 0; i < m_rowsPerTick; ++i)
                    m_rowsOut->push(createTablerow(dataIndex * m_rowsPerTick + i));
            } else if (auto row = createTablerow()) {
                m_rowsOut->push(row.value());
            }

            const auto msec = m_syTimer->timeSinceStartMsec().count();
            if (((msec / 1000) % 3) == 0) {
                LineCommand lcmd(LineCommandKind::WRITE_DIGITAL, 2);
                lcmd.value = ((msec / 1000) % 2 == 0) ? 1 : 0;
                m_lcmdOut->push(lcmd);
            }

            // Edge-triggered LineReading output
            {
                const auto nowUs = m_syTimer->timeSinceStartUsec();
                const double sec = nowUs.count() / 1e6;
                const long secsInt = static_cast<long>(sec);

                int desired[kNumLines];
                desired[0] = secsInt % 2;                         // ~1 s high / ~1 s low
                desired[1] = (secsInt / 4) % 2;                   // toggles every 4 s -> long quiet gaps
                desired[2] = (std::fmod(sec, 5.0) < 0.1) ? 1 : 0; // brief pulse every 5 s

                for (int i = 0; i < kNumLines; ++i) {
                    if (desired[i] == m_lineState[i])
                        continue;
                    m_lineState[i] = desired[i];

                    LineReading lr;
                    lr.lineId = static_cast<uint16_t>(i);
                    lr.value = static_cast<uint32_t>(desired[i]);
                    lr.time = nowUs;
                    m_lrdOut->push(lr);
                }
            }

            // Deterministic, sample-rate-driven signal generation. Each value is a
            // pure function of the running sample index, so a recorded run is exactly
            // reproducible regardless of wall-clock pacing. One block of blockLen
            // samples is emitted per loop iteration; the loop is paced to m_fps by
            // the frame sleep above, so the effective rate is ~m_fps*blockLen.
            const int blockLen = std::max(1, static_cast<int>(std::lround(m_sampleRate / m_fps)));
            if (m_noiseTable.empty())
                m_noiseTable = createNoiseTable();

            VectorXu64 timestamps(blockLen);
            std::vector<double> lo(blockLen);
            std::vector<double> hi(blockLen);
            for (int i = 0; i < blockLen; ++i) {
                const uint64_t n = m_sampleCount + static_cast<uint64_t>(i);
                const double t = static_cast<double>(n) / m_sampleRate;

                timestamps[i] = static_cast<uint64_t>(std::llround(static_cast<double>(n) * 1e6 / m_sampleRate));
                lo[i] = 0.5 * std::sin(2.0 * M_PI * m_freqLow * t);
                hi[i] = 0.5 * std::sin(2.0 * M_PI * m_freqHigh * t);
            }
            const uint64_t blockStart = m_sampleCount;
            m_sampleCount += static_cast<uint64_t>(blockLen);

            if (m_floatOut->hasSubscribers())
                m_floatOut->push(
                    createSignalBlock<SignalBlockF32>(
                        timestamps,
                        lo,
                        hi,
                        m_floatMix,
                        m_noiseTable,
                        m_noiseLevel,
                        blockStart,
                        [](double v) {
                            return static_cast<float>(v);
                        }));
            if (m_int32Out->hasSubscribers())
                m_int32Out->push(
                    createSignalBlock<SignalBlockI32>(
                        timestamps,
                        lo,
                        hi,
                        m_int32Mix,
                        m_noiseTable,
                        m_noiseLevel,
                        blockStart,
                        [](double v) {
                            return static_cast<int32_t>(std::lround(1000.0 * v));
                        }));
            // signed 16-bit: exercise the negative half of the range as well
            if (m_int16Out->hasSubscribers())
                m_int16Out->push(
                    createSignalBlock<SignalBlockI16>(
                        timestamps,
                        lo,
                        hi,
                        m_int16Mix,
                        m_noiseTable,
                        m_noiseLevel,
                        blockStart,
                        [](double v) {
                            return static_cast<int16_t>(std::lround(1000.0 * v));
                        }));
            if (m_uint16Out->hasSubscribers())
                m_uint16Out->push(
                    createSignalBlock<SignalBlockU16>(
                        timestamps,
                        lo,
                        hi,
                        m_uint16Mix,
                        m_noiseTable,
                        m_noiseLevel,
                        blockStart,
                        [](double v) {
                            return static_cast<uint16_t>(std::lround(2000.0 + 1000.0 * v));
                        }));

            dataIndex++;
        }

        m_testCard.release();
        m_scene.release();
        m_noise.clear();
    }

private:
    static constexpr int kMinFrameEdge = 16;
    static constexpr int kMaxFrameEdge = 8192;
    static constexpr int kMaxSignalChannels = 32768;
    static constexpr int kMaxRowsPerTick = 1000;
    static constexpr double kMaxSampleRate = 1000000.0;

    static QString frameContentToString(FrameContent content)
    {
        return content == FrameContent::CAMERA ? QStringLiteral("camera") : QStringLiteral("testcard");
    }

    static FrameContent frameContentFromString(const QString &str)
    {
        return str == QStringLiteral("camera") ? FrameContent::CAMERA : FrameContent::TEST_CARD;
    }

    /**
     * Set channel names and the frequency mix of each channel for a signal port.
     * Uses the given default layout, unless a channel count was configured explicitly.
     */
    template<typename T>
    void setupSignalChannels(
        const std::shared_ptr<DataStream<T>> &stream,
        ChannelMix &mix,
        const MetaArray &defaultNames,
        const ChannelMix &defaultMix)
    {
        if (m_signalChannels <= 0) {
            mix = defaultMix;
            stream->setMetadataValue("signal_names", defaultNames);
            return;
        }

        // give every channel its own, deterministic blend of the two test frequencies
        MetaArray names;
        mix.clear();
        for (int c = 0; c < m_signalChannels; ++c) {
            const double w = ((c * 7) % 16) / 15.0;
            mix.emplace_back(1.0 - w, w);
            names.push_back(std::format("Ch{}", c + 1));
        }
        stream->setMetadataValue("signal_names", names);
    }

    static constexpr size_t kNoiseTableSize = 1 << 16;

    /**
     * A table of standard-normal samples with a fixed seed: the noise is a pure function
     * of sample index and channel, so runs stay reproducible.
     */
    static std::vector<float> createNoiseTable()
    {
        std::vector<float> table(kNoiseTableSize);
        std::mt19937 rng(0x5EED);
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (auto &v : table)
            v = dist(rng);
        return table;
    }

    template<typename SB, typename Conv>
    static SB createSignalBlock(
        const VectorXu64 &timestamps,
        const std::vector<double> &lo,
        const std::vector<double> &hi,
        const ChannelMix &mix,
        const std::vector<float> &noise,
        double noiseLevel,
        uint64_t firstSample,
        Conv convert)
    {
        const auto blockLen = lo.size();
        SB sb(blockLen, mix.size());
        sb.timestamps = timestamps;
        // the signals swing +/- 0.5, the noise level is relative to that amplitude
        const double noiseGain = noiseLevel * 0.5;
        for (size_t c = 0; c < mix.size(); ++c) {
            const auto [wLo, wHi] = mix[c];
            // every channel walks the table at a different offset
            const size_t noiseBase = static_cast<size_t>(firstSample) + c * 7919u;
            for (size_t i = 0; i < blockLen; ++i) {
                const double n = noiseGain * noise[(noiseBase + i) & (kNoiseTableSize - 1)];
                sb.data(i, c) = convert(wLo * lo[i] + wHi * hi[i] + n);
            }
        }

        return sb;
    }

    /**
     * Render a scene that looks roughly like camera data to an encoder: Soft color
     * gradients with a few brighter discs for edges. It is larger than the frame, so
     * we can slowly pan over it at runtime, and we add one of a few sensor-noise
     * images to each frame. That way, creating a frame stays cheap.
     */
    void createCameraScene()
    {
        constexpr int kNoiseCount = 4;
        const cv::Size frameSize(m_outFrameSize.width(), m_outFrameSize.height());
        const cv::Size sceneSize(frameSize.width * 5 / 4, frameSize.height * 5 / 4);

        // a few random colors from a teal-to-violet palette, scaled up to smooth gradients
        cv::RNG rng(0x5Eed);
        cv::Mat seed(6, 10, CV_8UC3);
        rng.fill(seed, cv::RNG::UNIFORM, cv::Scalar(85, 90, 60), cv::Scalar(140, 200, 230));
        cv::cvtColor(seed, seed, cv::COLOR_HSV2BGR);
        cv::resize(seed, m_scene, sceneSize, 0, 0, cv::INTER_CUBIC);

        for (int i = 0; i < 14; ++i) {
            const cv::Point center(rng.uniform(0, sceneSize.width), rng.uniform(0, sceneSize.height));
            const auto radius = rng.uniform(sceneSize.height / 24, sceneSize.height / 7);
            const auto color = cv::Scalar(m_scene.at<cv::Vec3b>(center)) * rng.uniform(1.15, 1.5);
            cv::circle(m_scene, center, radius, color, cv::FILLED, cv::LINE_AA);
        }
        if (!m_colorVideo)
            cv::cvtColor(m_scene, m_scene, cv::COLOR_BGR2GRAY);

        // noise is centered around 128, so we can apply it with a single saturating operation
        for (int i = 0; i < kNoiseCount; ++i) {
            cv::Mat noise(frameSize, m_scene.type());
            rng.fill(noise, cv::RNG::NORMAL, 128, 5);
            m_noise.push_back(noise);
        }
    }

    /**
     * Sleep until the next frame is due, and return its acquisition timestamp.
     */
    microseconds_t waitForNextFrameTime(int fps)
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

        // We pace by the nominal schedule to not drift, but stamp the frame with the time
        // it was actually "acquired" at, like a camera would do.
        m_prevFrameTime = nextFrameTime;
        return m_syTimer->timeSinceStartUsec();
    }

    Frame createFrame(size_t index, const microseconds_t &frameTime)
    {
        const auto width = m_outFrameSize.width();
        const auto height = m_outFrameSize.height();

        if (!m_scene.empty()) {
            // pan slowly over the scene, independent of the framerate
            const double sec = frameTime.count() / 1e6;
            const cv::Rect view(
                static_cast<int>((m_scene.cols - width) * (0.5 + 0.5 * std::sin(2.0 * M_PI * sec / 20.0))),
                static_cast<int>((m_scene.rows - height) * (0.5 + 0.5 * std::cos(2.0 * M_PI * sec / 13.0))),
                width,
                height);

            Frame frame(index);
            frame.time = frameTime;
            cv::addWeighted(m_scene(view), 1.0, m_noise[index % m_noise.size()], 1.0, -128.0, frame.mat);
            cv::putText(
                frame.mat,
                "Frame: " + numToString(index),
                cv::Point(24, height / 2),
                cv::FONT_HERSHEY_SIMPLEX,
                1.2,
                cv::Scalar(249, 249, 249),
                2,
                cv::LINE_AA);
            return frame;
        }

        // every frame needs its own pixel buffer, as consumers may hold on to the previous one
        Frame frame(index);
        frame.time = frameTime;
        frame.mat = m_testCard.clone();
        cv::putText(
            frame.mat,
            "Frame: " + numToString(index),
            cv::Point(24, 240),
            cv::FONT_HERSHEY_SIMPLEX,
            1.2,
            cv::Scalar(249, 249, 249),
            2,
            cv::LINE_AA);

        return frame;
    }

    /**
     * Pre-render the static part of the test card once; frames are stamped copies of it,
     * which is far cheaper than drawing the card for every frame.
     */
    void createTestCard()
    {
        const auto width = m_outFrameSize.width();
        const auto height = m_outFrameSize.height();

        // blue background
        m_testCard = cv::Mat(height, width, CV_8UC3, cv::Scalar(67, 42, 30));

        // green rectangle
        cv::rectangle(m_testCard, cv::Point(10, 10), cv::Point(width - 10, height - 10), cv::Scalar(96, 174, 40), 4);

        // vertical and horizontal orange lines
        cv::line(m_testCard, cv::Point(width / 2, 0), cv::Point(width / 2, height), cv::Scalar(0, 116, 247), 4);
        cv::line(m_testCard, cv::Point(0, height / 2), cv::Point(width, height / 2), cv::Scalar(0, 116, 247), 4);

        if (!m_colorVideo)
            cv::cvtColor(m_testCard, m_testCard, cv::COLOR_BGR2GRAY);
    }

    /**
     * A row every two seconds, with a random value.
     */
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

    /**
     * The n-th row of a high-rate stream. The value is derived from the row number,
     * so creating it is cheap and the stream is reproducible.
     */
    TableRow createTablerow(size_t rowNumber)
    {
        TableRow row;
        row.reserve(3);
        row.append(numToString(m_syTimer->timeSinceStartMsec().count()));
        row.append((rowNumber % 2) ? std::string("beta") : std::string("alpha"));
        row.append(numToString(rowNumber));
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
