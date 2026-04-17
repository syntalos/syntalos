/*
 * Copyright (C) 2020-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "videotransformmodule.h"

#include "datactl/frametype.h"
#include "vtransformctldialog.h"

SYNTALOS_MODULE(VideoTransformModule)

class VideoTransformModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<StreamInputPort<Frame>> m_framesInPort;
    std::shared_ptr<StreamSubscription<Frame>> m_framesIn;
    std::shared_ptr<DataStream<Frame>> m_framesOut;

    VTransformCtlDialog *m_settingsDlg;
    QList<std::shared_ptr<VideoTransform>> m_activeVTFList;
    cv::Size m_expectedFrameSize;

public:
    explicit VideoTransformModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesInPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_framesOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Edited Frames"));

        m_settingsDlg = new VTransformCtlDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~VideoTransformModule() {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject &) override
    {
        m_framesIn = nullptr;
        // check if there even is something to do for us
        if (!m_framesInPort->hasSubscription()) {
            setStateDormant();
            return true;
        }

        // lock UI
        m_settingsDlg->setRunning(true);

        // be notified once we get a new frame
        m_framesIn = m_framesInPort->subscription();
        registerDataReceivedEvent(&VideoTransformModule::onFrameReceived, m_framesIn);

        // get copy of video-transformation list
        m_activeVTFList = m_settingsDlg->transformList();

        // copy all existing metadata over from the source
        m_framesOut->setMetadata(m_framesIn->metadata());

        // notify transformers about original data
        MetaSize tfISize = m_framesIn->metadataValue<MetaSize>("size", {});
        m_expectedFrameSize = cv::Size(tfISize.width, tfISize.height);
        for (const auto &vtf : m_activeVTFList) {
            vtf->setOriginalSize(tfISize);
            vtf->start();
            tfISize = vtf->resultSize();
        }

        // set new dimensions of output data (we may have changed that)
        m_framesOut->setMetadataValue("size", tfISize);

        // update UI with the new limits
        m_settingsDlg->updateUi();

        // start the stream
        m_framesOut->start();

        setStateReady();
        return true;
    }

    void start() override
    {
        // nothing to do here
    }

    void onFrameReceived()
    {
        const auto maybeFrame = m_framesIn->peekNext();
        if (!maybeFrame.has_value())
            return;

        // get the frame
        auto frame = maybeFrame.value();

        // ensure the frame dimensions match what the stream metadata advertises;
        // the transforms rely on this guarantee and must not receive unexpected sizes
        if (frame.mat.cols != m_expectedFrameSize.width || frame.mat.rows != m_expectedFrameSize.height) [[unlikely]] {
            raiseError(QStringLiteral(
                           "Received frame with unexpected dimensions %1x%2 (expected %3x%4). "
                           "The video source is sending invalid frames.")
                           .arg(frame.mat.cols)
                           .arg(frame.mat.rows)
                           .arg(m_expectedFrameSize.width)
                           .arg(m_expectedFrameSize.height));
            return;
        }

        // apply transformations
        cv::Mat &image = frame.mat;
        bool prevTransformCreatedCopy = false;
        for (const auto &vtf : m_activeVTFList) {
            if (vtf->needsIndependentCopy()) {
                if (!prevTransformCreatedCopy) {
                    // make sure we have our own copy of the data and don't modify the original
                    // data pool that is shared between threads
                    image = image.clone();
                    prevTransformCreatedCopy = true;
                }
            } else {
                // the transform will copy the data by itself, so we can assume it was copied after this point
                prevTransformCreatedCopy = true;
            }
            vtf->process(image);
        }

        // forward the updated frame
        frame.mat = image;
        m_framesOut->push(frame);
    }

    void stop() override
    {
        for (const auto &vtf : m_activeVTFList)
            vtf->stop();
        m_activeVTFList.clear();

        // unlock UI
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings = m_settingsDlg->serializeSettings();
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->loadSettings(settings);
        return true;
    }
};

QString VideoTransformModuleInfo::id() const
{
    return QStringLiteral("videotransform");
}

QString VideoTransformModuleInfo::name() const
{
    return QStringLiteral("Video Transformer");
}

QString VideoTransformModuleInfo::description() const
{
    return QStringLiteral("Perform common transformations on frames, such as cropping and scaling.");
}

ModuleCategories VideoTransformModuleInfo::categories() const
{
    return ModuleCategory::PROCESSING;
}

AbstractModule *VideoTransformModuleInfo::createModule(QObject *parent)
{
    return new VideoTransformModule(parent);
}

#include "videotransformmodule.moc"
