/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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

#pragma once

#include <QEventLoop>
#include <QObject>

#include "moduleapi.h"
#include "streamexporter.h"

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logMLinkMod)

struct ErrorEvent;
} // namespace Syntalos

namespace iox
{
namespace mepoo
{
struct NoUserHeader;
}
namespace popo
{
using namespace iox;
class UntypedSubscriber;
class UntypedClient;
template<typename T, typename H>
class Subscriber;
} // namespace popo
} // namespace iox

/**
 * @brief Master link for out-of-process modules
 */
class MLinkModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit MLinkModule(QObject *parent = nullptr);
    virtual ~MLinkModule() override;

    ModuleDriverKind driver() const override;
    virtual ModuleFeatures features() const override;

    QString moduleBinary() const;
    void setModuleBinary(const QString &binaryPath);

    bool outputCaptured() const;
    void setOutputCaptured(bool capture);

    void setPythonVirtualEnv(const QString &venvDir);
    void setScript(const QString &script, const QString &wdir = QString());
    bool setScriptFromFile(const QString &fname, const QString &wdir = QString());

    QByteArray settingsData() const;
    void setSettingsData(const QByteArray &data);

    void terminateProcess();
    bool runProcess();

    bool isProcessRunning() const;

    QString readProcessOutput();

    void markIncomingForExport(StreamExporter *exporter);
    virtual bool prepare(const TestSubject &) override;
    virtual void start() override;

signals:
    void processOutputReceived(const QString &text);

private:
    template<typename T>
    std::unique_ptr<T> makeSubscriber(const QString &eventName);
    std::unique_ptr<iox::popo::UntypedSubscriber> makeUntypedSubscriber(const QString &eventName);
    template<typename T>
    std::unique_ptr<T> makeClient(const QString &callName);
    std::unique_ptr<iox::popo::UntypedClient> makeUntypedClient(const QString &callName);
    template<typename ReqData>
    bool callUntypedClientSimple(
        const std::unique_ptr<iox::popo::UntypedClient> &client,
        const ReqData &reqEntity,
        int timeoutSec = 8);
    template<typename Client, typename Func>
    bool callClientSimple(const Client &client, Func func, int timeoutSec = 8);

    void resetConnection();
    static void onErrorReceivedCb(
        iox::popo::Subscriber<ErrorEvent, iox::mepoo::NoUserHeader> *subscriber,
        MLinkModule *self);
    static void onPortChangedCb(iox::popo::UntypedSubscriber *subscriber, MLinkModule *self);

private:
    class Private;
    Q_DISABLE_COPY(MLinkModule)
    QScopedPointer<Private> d;
    friend class Private;
};
