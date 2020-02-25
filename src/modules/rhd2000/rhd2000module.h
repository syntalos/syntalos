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

class FloatStreamDataInfo
{
public:
    explicit FloatStreamDataInfo(bool enabled = true)
    {
        this->active = enabled;
    }

    std::shared_ptr<DataStream<FloatSignalBlock>> stream;
    std::shared_ptr<FloatSignalBlock> signalBlock;
    int chan;
    int sbChan;
    bool active;
};

class Rhd2000Module : public AbstractModule
{
    Q_OBJECT
public:
    explicit Rhd2000Module(QObject *parent = nullptr);

    bool prepare(const TestSubject &testSubject) override;

    void start() override;
    void stop() override;

    QList<QAction *> actions() override;

    QByteArray serializeSettings(const QString &) override;
    bool loadSettings(const QString &, const QByteArray &data) override;

    // Accessorts for the Intan code
    void emitStatusInfo(const QString &text);

    void pushAmplifierData();

    std::vector<std::vector<FloatStreamDataInfo>> fsdiByStreamCC;

private slots:
    void on_portsScanned(SignalSources *sources);
    void runBoardDAQ();

private:
    IntanUi *m_intanUi;
    QList<QAction *> m_actions;
    QAction *m_runAction;
    QTimer *m_evTimer;

    std::vector<std::pair<std::shared_ptr<DataStream<FloatSignalBlock>>, std::shared_ptr<FloatSignalBlock>>> m_streamSigBlocks;

    void noRecordRunActionTriggered();
};

inline void setSyModAmplifierData(Rhd2000Module *mod, int stream, int channel, int t, double val)
{
    if (mod != nullptr) {
        auto fsdi = mod->fsdiByStreamCC[stream][channel];
        fsdi.signalBlock->data[fsdi.sbChan][t] = val;
    }
}
