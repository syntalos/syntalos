/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "encodetask.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <filesystem>

#include "../videowriter.h"
#include "videoreader.h"
#include "queuemodel.h"
#include "datactl/edlstorage.h"
#include "datactl/tsyncfile.h"
#include "utils/misc.h"
#include "utils/tomlutils.h"

Q_LOGGING_CATEGORY(logEncodeTask, "encoder.task")

EncodeTask::EncodeTask(QueueItem *item, bool updateAttrs, int codecThreadN)
    : QRunnable(),
      m_item(item),
      m_updateAttrsData(updateAttrs),
      m_codecThreadCount(codecThreadN),
      m_writeTsync(false)
{
}

bool EncodeTask::prepareSourceFiles()
{
    QFileInfo fi(m_item->fname());
    if (!fi.isAbsolute()) {
        m_item->setError(QStringLiteral("Received invalid video file path: %1").arg(m_item->fname()));
        return false;
    }

    m_destFname = m_item->fname();
    m_srcFname = fi.dir().filePath(QStringLiteral("srcraw_") + fi.fileName());
    m_datasetRoot = fi.dir().path();

    if (!QFile::rename(m_destFname, m_srcFname)) {
        m_item->setError(QStringLiteral("Unable to rename source video file."));
        return false;
    }

    const auto tmpTsyncFname = fi.dir().filePath(fi.baseName() + QStringLiteral("_timestamps.tsync"));
    QFileInfo tsyncFi(tmpTsyncFname);
    m_writeTsync = tsyncFi.exists();
    if (!m_writeTsync)
        return true;

    m_tsyncDestFname = tmpTsyncFname;
    m_tsyncSrcFname = tsyncFi.dir().filePath(QStringLiteral("srcraw_") + tsyncFi.fileName());

    if (!QFile::rename(m_tsyncDestFname, m_tsyncSrcFname)) {
        m_item->setError(QStringLiteral("Unable to rename source video timesync file."));
        return false;
    }

    return true;
}

void EncodeTask::run()
{
    m_item->setStatus(QueueItem::RUNNING);
    if (!prepareSourceFiles())
        return;

    // sanity check
    const auto md = m_item->mdata();
    if (m_writeTsync != md["save-timestamps"].toBool()) {
        m_item->setError(
            "No tsync file was found, but we were requested to write a timestamp file. Unable to proceed safely.");
        return;
    }

    // open source video file
    VideoReader vsrc;
    if (!vsrc.open(m_srcFname)) {
        m_item->setError(
            QStringLiteral("Unable to open recorded raw video. Encoding failed. %1").arg(vsrc.lastError()));
        return;
    }

    // open video writer to re-encode the video
    VideoWriter vwriter;
    vwriter.setFileSliceInterval(0); // no slicing allowed
    vwriter.setContainer(static_cast<VideoContainer>(md["video-container"].toInt()));

    // set codec properties
    if (m_codecThreadCount <= 0)
        m_codecThreadCount = 1;

    auto cprops = m_item->codecProps();
    cprops.setThreadCount(m_codecThreadCount);
    vwriter.setCodecProps(cprops);

    // open source tsync file and load it
    TSyncFileTimeUnit tsyncTimeUnit = TSyncFileTimeUnit::MICROSECONDS;
    std::vector<std::pair<long long, long long>> tsyncTimes;
    if (m_writeTsync) {
        TimeSyncFileReader tfr;
        if (!tfr.open(m_tsyncSrcFname)) {
            m_item->setError(
                QStringLiteral("Unable to open tsync file of this video for reading: %1").arg(tfr.lastError()));
            return;
        }

        tsyncTimes = tfr.times();
        tsyncTimeUnit = tfr.timeUnits().second;
        vwriter.setTsyncFileCreationTimeOverride(QDateTime::fromTime_t(tfr.creationTime()));
    }

    // start encoding
    bool firstFrame = true;
    bool success = true;
    int frameWidth;
    int frameHeight;
    bool useColor = true;
    int progress = 0;
    const auto frameCount = vsrc.totalFrames();
    if (frameCount <= 0) {
        m_item->setError(QStringLiteral("No frames found in video file."));
        return;
    }
    double onePerc = 100.0 / frameCount;

    while (true) {
        auto maybeFrame = vsrc.readFrame();
        if (!maybeFrame.has_value())
            break;
        auto frame = maybeFrame.value().first;
        auto frameNo = maybeFrame.value().second;

        if (firstFrame) {
            firstFrame = false;
            frameWidth = frame.cols;
            frameHeight = frame.rows;

            useColor = frame.channels() > 1;
            try {
                vwriter.initialize(
                    m_destFname,
                    md["mod-name"].toString(),
                    md["src-mod-name"].toString(),
                    QUuid::fromString(md["collection-id"].toString()),
                    md["subject-name"].toString(),
                    frameWidth,
                    frameHeight,
                    vsrc.framerate(),
                    frame.depth(),
                    useColor,
                    m_writeTsync);
            } catch (const std::runtime_error &e) {
                m_item->setError(QStringLiteral("Unable to initialize recording: %1").arg(e.what()));
                success = false;
                break;
            }
        }

        // write timestamp info
        auto timestamp = milliseconds_t(0);
        size_t frameIdx = frameNo - 1;
        if (m_writeTsync) {
            if (frameIdx < tsyncTimes.size()) {
                if (tsyncTimeUnit == TSyncFileTimeUnit::MILLISECONDS)
                    timestamp = milliseconds_t(tsyncTimes[frameIdx].second);
                else if (tsyncTimeUnit == TSyncFileTimeUnit::MICROSECONDS)
                    timestamp = std::chrono::duration_cast<milliseconds_t>(microseconds_t(tsyncTimes[frameIdx].second));
                else if (tsyncTimeUnit == TSyncFileTimeUnit::NANOSECONDS)
                    timestamp = std::chrono::duration_cast<milliseconds_t>(nanoseconds_t(tsyncTimes[frameIdx].second));
            }
        }

        if (!vwriter.encodeFrame(frame, timestamp)) {
            m_item->setError(
                QStringLiteral("Unable to reencode video: %1").arg(QString::fromStdString(vwriter.lastError())));
            success = false;
            break;
        }

        int newProgress = frameNo * onePerc;
        if (newProgress != progress) {
            m_item->setProgress(newProgress);
            progress = newProgress;
        }
    }

    vwriter.finalize();

    // update dataset attributes metadata
    if (m_updateAttrsData) {
        static std::mutex attrMutex;
        std::lock_guard<std::mutex> lock(attrMutex);

        QString errorMsg;
        const auto attrFname = m_datasetRoot + QStringLiteral("/attributes.toml");
        const auto attrFnameTmp = m_datasetRoot + QStringLiteral("/attributes.tmp%1").arg(createRandomString(6));
        auto attrs = parseTomlFile(attrFname, errorMsg);
        if (errorMsg.isEmpty()) {
            if (attrs.value("encoder").toHash().value("name").toString() != vwriter.selectedEncoderName()) {
                QVariantHash vInfo;
                vInfo.insert("frame_width", frameWidth);
                vInfo.insert("frame_height", frameHeight);
                vInfo.insert("framerate", vsrc.framerate());
                vInfo.insert("colored", useColor);

                QVariantHash encInfo;
                encInfo.insert("name", vwriter.selectedEncoderName());
                encInfo.insert("lossless", vwriter.codecProps().isLossless());
                encInfo.insert("thread_count", vwriter.codecProps().threadCount());
                if (vwriter.codecProps().useVaapi())
                    encInfo.insert("vaapi_enabled", true);
                if (vwriter.codecProps().mode() == CodecProperties::ConstantBitrate)
                    encInfo.insert("target_bitrate_kbps", vwriter.codecProps().bitrateKbps());
                else
                    encInfo.insert("target_quality", vwriter.codecProps().quality());
                attrs["video"] = vInfo;
                attrs["encoder"] = encInfo;

                QFile f(attrFnameTmp);
                if (f.open(QFile::ReadWrite)) {
                    f.write(qVariantHashToTomlData(attrs));
                    f.write("\n");
                    f.close();

                    // atomically replace old attributes file with our new one
                    std::error_code error;
                    std::filesystem::rename(attrFnameTmp.toStdString(), attrFname.toStdString(), error);
                    if (error) {
                        qCWarning(logEncodeTask).noquote()
                            << "Unable to replace old attributes file: " << QString::fromStdString(error.message());
                        QFile::remove(attrFnameTmp);
                    }
                } else {
                    qCWarning(logEncodeTask).noquote()
                        << "Unable to open temporary attributes file for writing: " << errorMsg;
                    QFile::remove(attrFnameTmp);
                }
            }
        } else {
            qCWarning(logEncodeTask).noquote() << "Unable to read dataset attributes: " << errorMsg;
        }
    }

    if (vsrc.lastFrameIndex() != frameCount) {
        m_item->setError(QStringLiteral("Expected to encode %1 frames, but only encoded %2.")
                             .arg(frameCount)
                             .arg(vsrc.lastFrameIndex()));
        success = false;
    }
    if (success) {
        m_item->setStatus(QueueItem::FINISHED);
        m_item->setProgress(100);

        // remove source files, encoding was a success!
        QFile::remove(m_srcFname);
        if (m_writeTsync)
            QFile::remove(m_tsyncSrcFname);
    }
}
