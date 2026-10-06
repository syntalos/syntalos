/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "prismmodule.h"

#include <opencv2/imgproc.hpp>

#include "datactl/frametype.h"
#include "prismctldialog.h"

SYNTALOS_MODULE(PrismModule)

// Mapping from user-visible channel index to OpenCV plane index in a BGR/BGRA image.
// User indices: 0=Red, 1=Green, 2=Blue, 3=Alpha
// OpenCV BGR planes: 0=Blue, 1=Green, 2=Red, 3=Alpha
static const int CHAN_TO_OCV_PLANE[4] = {2, 1, 0, 3};

// Inverse: plane index (BGR order) → user channel index
// plane 0 (B) → user ch 2 (Blue)
// plane 1 (G) → user ch 1 (Green)
// plane 2 (R) → user ch 0 (Red)
// plane 3 (A) → user ch 3 (Alpha)
static const int OCV_PLANE_TO_CHAN[4] = {2, 1, 0, 3};

static const char *CHAN_IDS[4] = {"r", "g", "b", "a"};
static const char *CHAN_TITLES[4] = {"Red", "Green", "Blue", "Alpha"};

class PrismModule : public AbstractModule
{
    Q_OBJECT

private:
    PrismCtlDialog *m_settingsDlg;

    // Current configuration (mirrors dialog state, updated in updatePortConfiguration)
    PrismMode m_currentMode;
    bool m_channelEnabled[4];

    // Split / Grayscale mode input port
    std::shared_ptr<StreamInputPort<Frame>> m_mainIn;

    // Split mode (one output stream per channel)
    std::shared_ptr<DataStream<Frame>> m_channelStreams[4];

    // Grayscale mode: single output stream
    std::shared_ptr<DataStream<Frame>> m_grayOut;

    // Combine mode: one input port per channel
    std::shared_ptr<StreamInputPort<Frame>> m_channelInputs[4];

    // Combine mode: merged output stream
    std::shared_ptr<DataStream<Frame>> m_combinedOut;

public:
    explicit PrismModule(PrismModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_currentMode(PrismMode::SPLIT)
    {
        for (int i = 0; i < 4; i++) {
            m_channelEnabled[i] = (i < 3); // R, G, B enabled by default; A disabled
            m_channelStreams[i] = nullptr;
            m_channelInputs[i] = nullptr;
        }

        m_settingsDlg = new PrismCtlDialog;
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &PrismCtlDialog::settingsChanged, this, &PrismModule::updatePortConfiguration);

        // Set up the initial port layout (split mode, R+G+B enabled)
        updatePortConfiguration();
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    /**
     * @brief Rebuild input/output ports to match the current mode and channel selection.
     *
     * Must only be called from the main (UI) thread and only when the module is not running.
     */
    void updatePortConfiguration()
    {
        clearInPorts();
        clearOutPorts();

        // Reset all port-holding member variables
        m_mainIn = nullptr;
        m_grayOut = nullptr;
        m_combinedOut = nullptr;
        for (int i = 0; i < 4; i++) {
            m_channelStreams[i] = nullptr;
            m_channelInputs[i] = nullptr;
        }

        m_currentMode = m_settingsDlg->mode();
        for (int i = 0; i < 4; i++)
            m_channelEnabled[i] = m_settingsDlg->channelEnabled(i);

        switch (m_currentMode) {
        case PrismMode::SPLIT:
            m_mainIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames In"));
            for (int ch = 0; ch < 4; ch++) {
                if (!m_channelEnabled[ch])
                    continue;
                m_channelStreams[ch] = registerOutputPort<Frame>(
                    QStringLiteral("frames-") + QString::fromUtf8(CHAN_IDS[ch]),
                    QString::fromUtf8(CHAN_TITLES[ch]));
            }
            break;

        case PrismMode::COMBINE:
            for (int ch = 0; ch < 4; ch++) {
                if (!m_channelEnabled[ch])
                    continue;
                m_channelInputs[ch] = registerInputPort<Frame>(
                    QStringLiteral("frames-") + QString::fromUtf8(CHAN_IDS[ch]),
                    QString::fromUtf8(CHAN_TITLES[ch]));
            }
            m_combinedOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Combined"));
            break;

        case PrismMode::GRAYSCALE:
            m_mainIn = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
            m_grayOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Grayscale"));
            break;
        }
    }

    bool prepare(const RunInfo &) override
    {
        m_settingsDlg->setRunning(true);

        switch (m_currentMode) {
        case PrismMode::SPLIT:
        case PrismMode::GRAYSCALE:
            return prepareSingleInput();

        case PrismMode::COMBINE:
            return prepareCombine();
        }

        return true;
    }

    /**
     * Splits, combines or converts incoming frames, depending on the selected mode.
     */
    struct Worker {
        WorkerContext mod{};
        PrismMode mode;
        std::array<bool, 4> channelEnabled;

        // Split / Grayscale mode: input subscription
        std::shared_ptr<StreamSubscription<Frame>> mainSub;

        // Split mode (one output stream per channel)
        std::array<std::shared_ptr<DataStream<Frame>>, 4> channelStreams;

        // Grayscale mode: single output stream
        std::shared_ptr<DataStream<Frame>> grayOut;

        // Combine mode: one subscription per connected channel
        std::array<std::shared_ptr<StreamSubscription<Frame>>, 4> channelSubs;

        // Combine mode: merged output stream
        std::shared_ptr<DataStream<Frame>> combinedOut;
        cv::Size expectedSize;

        // per-channel frame buffers for combine mode
        std::array<std::optional<Frame>, 4> channelBuffers{};
        uint64_t outFrameIndex = 0;

        void setup(WorkerEvents &ev)
        {
            if (mode != PrismMode::COMBINE) {
                ev.onData(mainSub, [this] {
                    onMainFrameReceived();
                });
                return;
            }

            // Register event callbacks only for the connected channel subscriptions
            for (size_t ch = 0; ch < channelSubs.size(); ++ch) {
                if (channelSubs[ch])
                    ev.onData(channelSubs[ch], [this, ch] {
                        onChannelReceived(ch);
                    });
            }
        }

        void onMainFrameReceived()
        {
            const auto maybeFrame = mainSub->peekNext();
            if (!maybeFrame.has_value())
                return;
            const auto &frame = maybeFrame.value();

            if (mode == PrismMode::GRAYSCALE)
                processGrayscale(frame);
            else
                processSplit(frame);
        }

        void processSplit(const Frame &frame) const
        {
            // Clone to ensure we own the data before splitting
            std::vector<cv::Mat> planes;
            cv::split(frame.mat.clone(), planes);

            for (int ch = 0; ch < 4; ch++) {
                if (!channelStreams[ch])
                    continue;
                const int planeIdx = CHAN_TO_OCV_PLANE[ch];
                if (planeIdx >= static_cast<int>(planes.size()))
                    continue;
                channelStreams[ch]->push(Frame(planes[planeIdx], frame.index, frame.time));
            }
        }

        void processGrayscale(const Frame &frame) const
        {
            cv::Mat gray;
            const int channels = frame.mat.channels();
            if (channels == 1) {
                gray = frame.mat; // already grayscale
            } else if (channels == 3) {
                cv::cvtColor(frame.mat, gray, cv::COLOR_BGR2GRAY);
            } else if (channels == 4) {
                cv::cvtColor(frame.mat, gray, cv::COLOR_BGRA2GRAY);
            } else {
                // Fallback: just take the first plane
                cv::extractChannel(frame.mat, gray, 0);
            }
            grayOut->push(Frame(gray, frame.index, frame.time));
        }

        void onChannelReceived(int ch)
        {
            if (!channelSubs[ch])
                return;
            auto maybeFrame = channelSubs[ch]->peekNext();
            if (!maybeFrame.has_value())
                return;

            channelBuffers[ch] = std::move(*maybeFrame);
            tryCombine();
        }

        /**
         * @brief Attempt to merge buffered channel frames into one output frame.
         *
         * A merge is performed once every channel that is both enabled AND connected
         * has contributed a frame. After the merge the buffers are cleared, so the
         * next round starts fresh.
         */
        void tryCombine()
        {
            // Wait until all enabled+connected channels have a buffered frame
            for (int ch = 0; ch < 4; ch++) {
                if (!channelEnabled[ch] || !channelSubs[ch])
                    continue; // not active — skip
                if (!channelBuffers[ch].has_value())
                    return; // still waiting for this channel
            }

            // Determine if we include an alpha plane
            const bool hasAlpha = channelEnabled[3] && channelSubs[3];
            const int numPlanes = hasAlpha ? 4 : 3;

            // Determine output frame size from the first available buffer
            cv::Size frameSize = expectedSize;
            microseconds_t timestamp{0};
            for (int ch = 0; ch < 4; ch++) {
                if (channelBuffers[ch].has_value()) {
                    const auto &mat = channelBuffers[ch]->mat;
                    frameSize = mat.size();
                    timestamp = channelBuffers[ch]->time;
                    break;
                }
            }

            // Build planes in BGR/BGRA order for cv::merge
            // plane 0 = B (user ch 2), plane 1 = G (user ch 1), plane 2 = R (user ch 0), plane 3 = A (user ch 3)
            std::vector<cv::Mat> planes(numPlanes);
            for (int p = 0; p < numPlanes; p++) {
                const int ch = OCV_PLANE_TO_CHAN[p];
                if (channelEnabled[ch] && channelSubs[ch] && channelBuffers[ch].has_value()) {
                    const auto &mat = channelBuffers[ch]->mat;
                    if (mat.channels() == 1) {
                        planes[p] = mat;
                    } else {
                        // Input has multiple channels - extract just the first one
                        cv::extractChannel(mat, planes[p], 0);
                    }
                } else {
                    // Channel not active or not yet available - fill with zeros
                    planes[p] = cv::Mat::zeros(frameSize, CV_8UC1);
                }
            }

            cv::Mat merged;
            cv::merge(planes, merged);
            combinedOut->push(Frame(merged, outFrameIndex++, timestamp));

            // Clear buffers so the next round of frames can be collected
            for (int ch = 0; ch < 4; ch++)
                channelBuffers[ch] = std::nullopt;
        }
    };

    void stop() override
    {
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings = m_settingsDlg->serializeSettings();
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->loadSettings(settings);
        // Rebuild ports to match the loaded settings
        updatePortConfiguration();
        return true;
    }

private:
    bool prepareSingleInput()
    {
        if (!m_mainIn || !m_mainIn->hasSubscription()) {
            setStateDormant();
            return true;
        }

        const auto mainSub = m_mainIn->subscription();

        const auto framerate = mainSub->metadataValue<double>(QStringLiteral("framerate"), 0.0);
        const auto size = mainSub->metadataValue<MetaSize>(QStringLiteral("size"), {});

        if (m_currentMode == PrismMode::GRAYSCALE) {
            m_grayOut->setMetadataValue("framerate", framerate);
            m_grayOut->setMetadataValue("size", size);
            m_grayOut->start();
        } else {
            // Split mode
            for (int ch = 0; ch < 4; ch++) {
                if (!m_channelStreams[ch])
                    continue;
                m_channelStreams[ch]->setMetadataValue("framerate", framerate);
                m_channelStreams[ch]->setMetadataValue("size", size);
                m_channelStreams[ch]->start();
            }
        }

        setWorker(
            Worker{
                .mode = m_currentMode,
                .channelEnabled = std::to_array(m_channelEnabled),
                .mainSub = mainSub,
                .channelStreams = std::to_array(m_channelStreams),
                .grayOut = m_grayOut,
                .channelSubs = {},
                .combinedOut = nullptr,
                .expectedSize = cv::Size(0, 0),
            });

        setStateReady();
        return true;
    }

    bool prepareCombine()
    {
        bool anyConnected = false;
        std::array<std::shared_ptr<StreamSubscription<Frame>>, 4> channelSubs;
        cv::Size expectedSize(0, 0);

        for (int ch = 0; ch < 4; ch++) {
            if (!m_channelInputs[ch] || !m_channelInputs[ch]->hasSubscription())
                continue;
            channelSubs[ch] = m_channelInputs[ch]->subscription();

            // Use the first connected channel for output metadata
            if (!anyConnected) {
                const auto fr = channelSubs[ch]->metadataValue<double>(QStringLiteral("framerate"), 0.0);
                const auto sz = channelSubs[ch]->metadataValue<MetaSize>(QStringLiteral("size"), {});
                expectedSize = cv::Size(sz.width, sz.height);
                m_combinedOut->setMetadataValue("framerate", fr);
                m_combinedOut->setMetadataValue("size", sz);
            }

            anyConnected = true;
        }

        if (!anyConnected) {
            setStateDormant();
            return true;
        }

        m_combinedOut->start();

        setWorker(
            Worker{
                .mode = m_currentMode,
                .channelEnabled = std::to_array(m_channelEnabled),
                .mainSub = nullptr,
                .channelStreams = {},
                .grayOut = nullptr,
                .channelSubs = channelSubs,
                .combinedOut = m_combinedOut,
                .expectedSize = expectedSize,
            });

        setStateReady();
        return true;
    }
};

QString PrismModuleInfo::id() const
{
    return QStringLiteral("prism");
}

QString PrismModuleInfo::name() const
{
    return QStringLiteral("Prism");
}

QString PrismModuleInfo::description() const
{
    return QStringLiteral(
        "Split a frames into separate channels or combine channels back into one. Can also convert colored frames to "
        "grayscale.");
}

ModuleCategories PrismModuleInfo::categories() const
{
    return ModuleCategory::PROCESSING;
}

AbstractModule *PrismModuleInfo::createModule(QObject *parent)
{
    return new PrismModule(this, parent);
}

#include "prismmodule.moc"
