/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QObject>
#include "moduleapi.h"

SYNTALOS_DECLARE_MODULE

using namespace Syntalos;

class IntanRhxModuleInfo : public ModuleInfo
{
public:
    QString id() const final;
    QString name() const final;
    QString summary() const final;
    QString description() const final;
    QString authors() const final;
    QString license() const final;
    ModuleCategories categories() const final;
    bool singleton() const final;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

class BoardSelectDialog;
class ControlWindow;
class ChanExportDialog;
class ControllerInterface;
class SystemState;
class Channel;

template<typename T>
class StreamDataInfo
{
public:
    explicit StreamDataInfo(int group = -1, int channel = -1)
        : active(false),
          channelGroup(group),
          nativeChannel(channel)
    {
        signalBlock = std::make_shared<T>();
    }

    bool active;
    std::shared_ptr<DataStream<T>> stream;
    std::shared_ptr<T> signalBlock;
    int channelGroup;
    int nativeChannel;
};

class IntanRhxModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit IntanRhxModule(const QString &id,
                            IntanRhxModuleInfo *modInfo,
                            QObject *parent = nullptr);
    ~IntanRhxModule();

    bool initialize() override;

    ModuleFeatures features() const override;
    ModuleDriverKind driver() const override;

    void updateStartWaitCondition(OptionalWaitCondition *waitCondition) override;
    bool prepare(const TestSubject&) override;

    void processUiEvents() override;
    void start() override;

    void stop() override;

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &extraData) override;
    bool loadSettings(const QString&, const QVariantHash &settings, const QByteArray& extraData) override;

    void stopWithError(const QString &message);

    void setPortSignalBlockSampleSize(size_t sampleNum);
    std::vector<std::vector<StreamDataInfo<FloatSignalBlock>>> floatSdiByGroupChannel;
    std::vector<std::vector<StreamDataInfo<IntSignalBlock>>> intSdiByGroupChannel;

    std::unique_ptr<FreqCounterSynchronizer> clockSync;

    // these are used by timesync code
    int blocksPerTimestamp;
    int currentBlockIdx;
    microseconds_t lastBlockTimestamp;

private slots:
    void onExportedChannelsChanged(const QList<Channel*> &channels);

private:
    QIcon m_modIcon;
    BoardSelectDialog *m_boardSelectDlg;
    ControlWindow *m_ctlWindow;
    ChanExportDialog *m_chanExportDlg;
    ControllerInterface *m_controllerIntf;
    SystemState *m_sysState;

};

inline void syntalosModuleSetBlocksPerTimestamp(IntanRhxModule *mod, int count)
{
    mod->blocksPerTimestamp = count;
}

inline void syntalosModuleSetSignalBlocksTimestamps(IntanRhxModule *mod, const microseconds_t &blockRecvTimestamp,
                                                    uint32_t* tsBuf, size_t tsLen)
{
    if (mod == nullptr)
        return;

    VectorXu tvm = Eigen::Map<VectorXu, Eigen::Unaligned>(tsBuf, tsLen);
    for (auto &blocks : mod->intSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            if (!sdi.active)
                continue;
            sdi.signalBlock->timestamps = tvm;
        }
    }

    for (auto &blocks : mod->floatSdiByGroupChannel) {
        for (auto &sdi : blocks) {
            if (!sdi.active)
                continue;
            sdi.signalBlock->timestamps = tvm;
        }
    }

    int currentBlockIdx = mod->currentBlockIdx;
    const auto blocksPerTimestamp = mod->blocksPerTimestamp;
    if (blockRecvTimestamp != mod->lastBlockTimestamp) {
        currentBlockIdx = 0;
        mod->lastBlockTimestamp = blockRecvTimestamp;
    } else {
        currentBlockIdx++;
        if (currentBlockIdx >= blocksPerTimestamp)
            currentBlockIdx = blocksPerTimestamp;
    }

    // calculate time sync guesstimates
    mod->clockSync->processTimestamps(blockRecvTimestamp,
                                      currentBlockIdx, blocksPerTimestamp,
                                      tvm);
    mod->currentBlockIdx = currentBlockIdx;
}

inline void syntalosModuleExportAmplifierChanData(IntanRhxModule *mod, int group, int channel, uint16_t *rawBuf, size_t numSamples,
                                                  int numAmplifierChannels, int rawChanIndex)
{
    if (mod == nullptr)
        return;

    if (group >= (int) mod->floatSdiByGroupChannel.size())
        return;
    auto &blocks = mod->floatSdiByGroupChannel[group];
    if (channel >= (int) blocks.size())
        return;
    auto &sdi = blocks[channel];
    if (!sdi.active)
        return;

    sdi.signalBlock->data.resize(numSamples, 1);
    for (size_t i = 0; i < numSamples; ++i)
        sdi.signalBlock->data(i, 0) = 0.195F * (((double) rawBuf[numAmplifierChannels * i + rawChanIndex]) - 32768.0F);

    // publish new data on this stream
    sdi.stream->push(*sdi.signalBlock.get());
}

inline void syntalosModuleExportDigitalChanData(IntanRhxModule *mod, int group, int channel, float *rawBuf, size_t numSamples)
{
    if (mod == nullptr)
        return;

    if (group >= (int) mod->intSdiByGroupChannel.size())
        return;
    auto &blocks = mod->intSdiByGroupChannel[group];
    if (channel >= (int) blocks.size())
        return;
    auto &sdi = blocks[channel];
    if (!sdi.active)
        return;

    sdi.signalBlock->data.resize(numSamples, 1);
    for (size_t i = 0; i < numSamples; ++i)
        sdi.signalBlock->data(i, 0) = static_cast<int>(rawBuf[i]);

    // publish new data on this stream
    sdi.stream->push(*sdi.signalBlock.get());
}
