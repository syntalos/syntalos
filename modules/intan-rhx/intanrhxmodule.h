/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

class IntanRhxModuleInfo : public ModuleInfo
{
public:
    QString id() const override;
    QString name() const override;
    QString description() const override;
    QString license() const override;
    QIcon icon() const override;
    bool singleton() const override;
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
        : channelGroup(group),
          nativeChannel(channel)
    { }

    std::shared_ptr<DataStream<T>> stream;
    std::shared_ptr<T> signalBlock;
    int channelGroup;
    int nativeChannel;
};

class IntanRhxModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit IntanRhxModule(QObject *parent = nullptr);
    ~IntanRhxModule();

    bool initialize() override;

    ModuleFeatures features() const override;
    ModuleDriverKind driver() const override;

    void updateStartWaitCondition(OptionalWaitCondition *waitCondition) override;
    bool prepare(const TestSubject&) override;

    void processUiEvents() override;
    void start() override;

    void stop() override;

    std::vector<std::vector<StreamDataInfo<FloatSignalBlock>>> floatSdiByGroupChannel;
    std::vector<std::vector<StreamDataInfo<IntSignalBlock>>> intSdiByGroupChannel;

private slots:
    void onExportedChannelsChanged(const QList<Channel*> &channels);

private:
    BoardSelectDialog *m_boardSelectDlg;
    ControlWindow *m_ctlWindow;
    ChanExportDialog *m_chanExportDlg;
    ControllerInterface *m_controllerIntf;
    SystemState *m_sysState;

};
