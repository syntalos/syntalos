/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include "moduleapi.h"

class IntanUi;
class SignalSources;

class Rhd2000ModuleInfo : public ModuleInfo
{
    Q_OBJECT
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString license() const override;
    QPixmap pixmap() const override;
    bool singleton() const override;
    AbstractModule *createModule(QObject *parent = nullptr) override;
};

template<typename T>
class StreamDataInfo
{
public:
    explicit StreamDataInfo(bool enabled = true)
        : chan(-1),
          sbChan(-1),
          active(enabled)
    { }

    std::shared_ptr<DataStream<T>> stream;
    std::shared_ptr<T> signalBlock;
    int chan;
    int sbChan;
    bool active;
};

class Rhd2000Module : public AbstractModule
{
    Q_OBJECT
public:
    explicit Rhd2000Module(QObject *parent = nullptr);

    bool prepare(const TestSubject &) override;

    void start() override;
    void stop() override;

    QList<QAction *> actions() override;

    void serializeSettings(const QString &, QVariantHash &, QByteArray &data) override;
    bool loadSettings(const QString &, const QVariantHash &, const QByteArray &data) override;

    // Accessorts for the Intan code
    void emitStatusInfo(const QString &text);

    void pushSignalData();

    std::vector<std::vector<StreamDataInfo<FloatSignalBlock>>> ampSdiByStreamCC;

    std::vector<std::pair<std::shared_ptr<DataStream<FloatSignalBlock>>, std::shared_ptr<FloatSignalBlock>>> boardADCStreamBlocks;
    std::vector<std::pair<std::shared_ptr<DataStream<IntSignalBlock>>, std::shared_ptr<IntSignalBlock>>> boardDINStreamBlocks;

    std::unique_ptr<FreqCounterSynchronizer> clockSync;

private slots:
    void on_portsScanned(SignalSources *sources);
    void runBoardDAQ();

private:
    IntanUi *m_intanUi;
    QList<QAction *> m_actions;
    QAction *m_runAction;
    QTimer *m_evTimer;

    std::vector<std::pair<std::shared_ptr<DataStream<FloatSignalBlock>>, std::shared_ptr<FloatSignalBlock>>> m_ampStreamBlocks;


    void noRecordRunActionTriggered();
};

inline void setSyModAmplifierData(Rhd2000Module *mod, int stream, int channel, int t, double val)
{
    if (mod != nullptr) {
        auto fsdi = mod->ampSdiByStreamCC[stream][channel];
        if (!fsdi.active)
            return;
        fsdi.signalBlock->data[fsdi.sbChan][t] = val;
    }
}

inline void syModSyncTimestamps(Rhd2000Module *mod,
                                const double &devLatencyMs, const milliseconds_t &dataRecvTimestamp, VectorXl &timestamps)
{
    if (mod == nullptr)
        return;

    mod->clockSync->processTimestamps(dataRecvTimestamp, devLatencyMs, timestamps);
}

inline void setSyModSigBlockTimestamps(Rhd2000Module *mod, const VectorXl &timestamps)
{
    if (mod != nullptr) {
        // amplifier channels
        for (auto &fsdiByCC : mod->ampSdiByStreamCC) {
            for (auto &fsdi : fsdiByCC) {
                if (!fsdi.active)
                    continue;
                fsdi.signalBlock->timestamps = timestamps;
            }
        }

        // board ADC channels
        for (auto &pair : mod->boardADCStreamBlocks) {
            pair.second->timestamps = timestamps;
        }

        // board DIN channels
        for (auto &pair : mod->boardDINStreamBlocks) {
            pair.second->timestamps = timestamps;
        }
    }
}

inline void setSyModBoardADCData(Rhd2000Module *mod, int channel, int t, double val)
{
    if (mod != nullptr) {
        int stream = std::floor(channel / 16.0);
        mod->boardADCStreamBlocks[stream].second->data[channel][t] = val;
    }
}

inline void setSyModBoardDINData(Rhd2000Module *mod, int channel, int t, int val)
{
    if (mod != nullptr) {
        int stream = std::floor(channel / 16.0);
        mod->boardDINStreamBlocks[stream].second->data[channel][t] = val;
    }
}
