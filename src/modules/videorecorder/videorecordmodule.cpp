/**
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

#include "videorecordmodule.h"

#include <QMessageBox>
#include <QJsonDocument>
#include <QDebug>

#include "videowriter.h"
#include "recordersettingsdialog.h"

VideoRecorderModule::VideoRecorderModule(QObject *parent)
    : ImageSinkModule(parent),
      m_settingsDialog(nullptr)
{
    m_name = QStringLiteral("Video Recorder");
}

VideoRecorderModule::~VideoRecorderModule()
{
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
}

QString VideoRecorderModule::id() const
{
    return QStringLiteral("videorecorder");
}

QString VideoRecorderModule::description() const
{
    return QStringLiteral("Store a video composed of frames from an image source module to disk.");
}

QPixmap VideoRecorderModule::pixmap() const
{
    return QPixmap(":/module/videorecorder");
}

void VideoRecorderModule::setName(const QString &name)
{
    ImageSinkModule::setName(name);
    if (m_settingsDialog != nullptr)
        m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
}

ModuleFeatures VideoRecorderModule::features() const
{
    return ModuleFeature::SETTINGS;
}

bool VideoRecorderModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    m_settingsDialog = new RecorderSettingsDialog;
    m_settingsDialog->setVideoName("video");
    m_settingsDialog->setSaveTimestamps(true);
    m_settingsWindows.append(m_settingsDialog);

    // find all modules suitable as frame sources
    Q_FOREACH(auto mod, manager->activeModules()) {
        auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
        if (imgSrcMod == nullptr)
            continue;
        m_frameSourceModules.append(imgSrcMod);
    }
    if (m_frameSourceModules.isEmpty())
        m_settingsDialog->setSelectedImageSourceMod(nullptr);
    else
        m_settingsDialog->setSelectedImageSourceMod(m_frameSourceModules.first()); // set first module as default

    connect(manager, &ModuleManager::moduleCreated, this, &VideoRecorderModule::recvModuleCreated);
    connect(manager, &ModuleManager::modulePreRemove, this, &VideoRecorderModule::recvModulePreRemove);

    setState(ModuleState::READY);
    setInitialized();
    setName(name());
    return true;
}

bool VideoRecorderModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);

    m_vidStorageDir = QStringLiteral("%1/videos").arg(storageRootDir);

    if (m_settingsDialog->videoName().isEmpty()) {
        raiseError("Video recording name is not set. Please set it in the settings to continue.");
        return false;
    }
    if (!makeDirectory(m_vidStorageDir))
        return false;

    auto imgSrcMod = m_settingsDialog->selectedImageSourceMod();
    if (imgSrcMod == nullptr) {
        raiseError("No frame source is set for video recording. Please set it in the modules' settings to continue.");
        return false;
    }

    const auto useColor = true;
    const auto frameSize = imgSrcMod->selectedResolution();

    m_videoWriter.reset(new VideoWriter);
    m_videoWriter->setContainer(m_settingsDialog->videoContainer());
    m_videoWriter->setCodec(m_settingsDialog->videoCodec());
    m_videoWriter->setLossless(m_settingsDialog->isLossless());
    m_videoWriter->setFileSliceInterval(m_settingsDialog->sliceInterval());

    try {
        m_videoWriter->initialize(QStringLiteral("%1/%2").arg(m_vidStorageDir).arg(m_settingsDialog->videoName()).toStdString(),
                                  frameSize.width,
                                  frameSize.height,
                                  imgSrcMod->selectedFramerate(),
                                  useColor,
                                  m_settingsDialog->saveTimestamps());
    } catch (const std::runtime_error& e) {
        raiseError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
        return false;
    }

    // attach the video recorder directly to the recording device.
    // this avoids making a function call or emitting a Qt signal,
    // which is more efficient with higher framerates and ensures we
    // always record data properly.
    imgSrcMod->attachVideoWriter(m_videoWriter.get());

    // write info video info file with auxiliary information about the video we encoded
    // (this is useful to gather intel about the video without opening the video file)
    auto infoPath = QStringLiteral("%1/%2_videoinfo.json").arg(m_vidStorageDir).arg(m_settingsDialog->videoName());
    QJsonObject vInfo;
    vInfo.insert("name", m_settingsDialog->videoName());
    vInfo.insert("frameWidth", frameSize.width);
    vInfo.insert("frameHeight", frameSize.height);
    vInfo.insert("framerate", imgSrcMod->selectedFramerate());
    vInfo.insert("colored", useColor);

    QFile vInfoFile(infoPath);
    if (!vInfoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        raiseError("Unable to open video info file for writing.");
        return false;
    }

    QTextStream vInfoFileOut(&vInfoFile);
    vInfoFileOut << QJsonDocument(vInfo).toJson();

    statusMessage(QStringLiteral("Recording from %1").arg(imgSrcMod->name()));
    setState(ModuleState::WAITING);
    return true;
}

void VideoRecorderModule::stop()
{
    if (m_videoWriter.get() != nullptr)
        m_videoWriter->finalize();

    auto imgSrcMod = m_settingsDialog->selectedImageSourceMod();
    disconnect(imgSrcMod, &ImageSourceModule::newFrame, this, &VideoRecorderModule::receiveFrame);

    statusMessage(QStringLiteral("Recording stopped."));
    m_videoWriter.reset(nullptr);
}

bool VideoRecorderModule::canRemove(AbstractModule *mod)
{
    return mod != m_settingsDialog->selectedImageSourceMod();
}

void VideoRecorderModule::showSettingsUi()
{
    assert(initialized());

    m_settingsDialog->setImageSourceModules(m_frameSourceModules);
    m_settingsDialog->show();
}

QByteArray VideoRecorderModule::serializeSettings(const QString &confBaseDir)
{
    Q_UNUSED(confBaseDir);
    QJsonObject jset;
    jset.insert("imageSourceModule", m_settingsDialog->selectedImageSourceMod()->name());
    jset.insert("videoName", m_settingsDialog->videoName());
    jset.insert("saveTimestamps", m_settingsDialog->saveTimestamps());

    jset.insert("videoCodec", static_cast<int>(m_settingsDialog->videoCodec()));
    jset.insert("videoContainer", static_cast<int>(m_settingsDialog->videoContainer()));
    jset.insert("lossless", m_settingsDialog->isLossless());

    jset.insert("sliceInterval", static_cast<int>(m_settingsDialog->sliceInterval()));

    return jsonObjectToBytes(jset);
}

bool VideoRecorderModule::loadSettings(const QString &confBaseDir, const QByteArray &data)
{
    Q_UNUSED(confBaseDir);
    auto jset = jsonObjectFromBytes(data);

    auto modName = jset.value("imageSourceModule").toString();
    Q_FOREACH(auto mod, m_frameSourceModules) {
        if (mod->name() == modName) {
            m_settingsDialog->setSelectedImageSourceMod(mod);
            break;
        }
    }
    m_settingsDialog->setVideoName(jset.value("videoName").toString());
    m_settingsDialog->setSaveTimestamps(jset.value("saveTimestamps").toBool());

    m_settingsDialog->setVideoCodec(static_cast<VideoCodec>(jset.value("videoCodec").toInt()));
    m_settingsDialog->setVideoContainer(static_cast<VideoContainer>(jset.value("videoContainer").toInt()));
    m_settingsDialog->setLossless(jset.value("lossless").toBool());

    m_settingsDialog->setSliceInterval(static_cast<uint>(jset.value("sliceInterval").toInt()));

    return true;
}

void VideoRecorderModule::recvModuleCreated(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod != nullptr)
        m_frameSourceModules.append(imgSrcMod);
}

void VideoRecorderModule::recvModulePreRemove(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod == nullptr)
        return;
    for (int i = 0; i < m_frameSourceModules.size(); i++) {
        auto fsmod = m_frameSourceModules.at(i);
        if (fsmod == imgSrcMod) {
            m_frameSourceModules.removeAt(i);
            break;
        }
    }
}

void VideoRecorderModule::receiveFrame(const FrameData &frameData)
{
    // Video recorder modules are special in that we directly register them with the recorder module's
    // DAQ thread, so their performance will not suffer from additional signal/slots and queueing overhead
    Q_UNUSED(frameData);
}
