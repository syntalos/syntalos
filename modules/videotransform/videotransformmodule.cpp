/*
 * Copyright (C) 2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "streams/frametype.h"
#include "vtransformctldialog.h"

class VideoTransformModule : public AbstractModule
{
    Q_OBJECT

private:
    std::shared_ptr<StreamInputPort<Frame>> m_framesInPort;
    std::shared_ptr<StreamSubscription<Frame>> m_framesIn;
    std::shared_ptr<DataStream<Frame>> m_framesOut;

    VTransformCtlDialog *m_settingsDlg;
    QList<std::shared_ptr<VideoTransform>> m_activeVTFList;

public:
    explicit VideoTransformModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_framesInPort = registerInputPort<Frame>(QStringLiteral("frames-in"), QStringLiteral("Frames"));
        m_framesOut = registerOutputPort<Frame>(QStringLiteral("frames-out"), QStringLiteral("Edited Frames"));

        m_settingsDlg = new VTransformCtlDialog;
        addSettingsWindow(m_settingsDlg);
    }

    ~VideoTransformModule()
    {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    bool prepare(const TestSubject&) override
    {
        m_framesIn = nullptr;
        // check if there even is something to do for us
        if (!m_framesInPort->hasSubscription()) {
            setStateIdle();
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
        const auto origQSize = m_framesIn->metadataValue("size", QSize()).toSize();
        cv::Size tfISize;
        tfISize.width = origQSize.width();
        tfISize.height = origQSize.height();
        for (const auto vtf : m_activeVTFList) {
            vtf->setOriginalSize(tfISize);
            vtf->start();
            tfISize = vtf->resultSize();
        }

        // set new dimensions of output data (we may have changed that)
        m_framesOut->setMetadataValue("size", QSize(tfISize.width, tfISize.height));

        // ensure no UI is visible at the moment, it needs to be updated
        // with the new limits
        m_settingsDlg->unselectAll();

        // start the stream
        m_framesOut->start();

        setStateReady();
        return true;
    }

    void start() override
    {
        // TODO
    }

    void onFrameReceived()
    {
        const auto maybeFrame = m_framesIn->peekNext();
        if (!maybeFrame.has_value())
            return;

        // we have to copy the frame as we must not modify the matrix in-place
        // (it may be currently in use by other threads - unfortunately immutability
        // can't be enforced at all on cv::Mat in streams)
        auto frame = maybeFrame.value();
        frame.mat.copyTo(frame.mat);

        // apply transformations
        for (const auto vtf : m_activeVTFList) {
            vtf->process(frame);
        }
        m_framesOut->push(frame);
    }

    void stop() override
    {
        for (const auto vtf : m_activeVTFList)
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

QPixmap VideoTransformModuleInfo::pixmap() const
{
    return QPixmap(":/module/videotransform");
}

AbstractModule *VideoTransformModuleInfo::createModule(QObject *parent)
{
    return new VideoTransformModule(parent);
}

#include "videotransformmodule.moc"