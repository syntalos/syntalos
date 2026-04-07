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
#include <QProcessEnvironment>

#include "moduleapi.h"
#include "streamexporter.h"

namespace Syntalos
{
Q_DECLARE_LOGGING_CATEGORY(logMLinkMod)

struct ErrorEvent;
struct StateChangeEvent;
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

namespace Syntalos
{

/**
 * @brief Master link for out-of-process modules
 */
class MLinkModule : public AbstractModule
{
    Q_OBJECT
public:
    explicit MLinkModule(QObject *parent = nullptr);
    ~MLinkModule() override;
    bool initialize() override;

    ModuleDriverKind driver() const override;
    virtual ModuleFeatures features() const override;

    QString moduleBinary() const;
    void setModuleBinary(const QString &binaryPath);
    void setModuleBinaryArgs(const QStringList &args);

    QProcessEnvironment moduleBinaryEnv() const;
    void setModuleBinaryEnv(const QProcessEnvironment &env);

    bool outputCaptured() const;
    void setOutputCaptured(bool capture);

    void setPythonVirtualEnv(const QString &venvDir);
    void setScript(const QString &script, const QString &wdir = QString());
    bool setScriptFromFile(const QString &fname, const QString &wdir = QString());
    bool isScriptModified() const;

    QByteArray settingsData() const;
    void setSettingsData(const QByteArray &data);

    virtual void showDisplayUi() override;
    virtual void showSettingsUi() override;

    void terminateProcess();
    bool runProcess();
    bool isProcessRunning() const;

    bool loadCurrentScript();
    bool sendPortInformation();

    QString readProcessOutput();

    void markIncomingForExport(StreamExporter *exporter);
    bool prepare(const TestSubject &subject) override;
    void start() override;
    void stop() override;
    void runThread(OptionalWaitCondition *startWaitCondition) override;

signals:
    void processOutputReceived(const QString &text);

protected:
    bool testIpcApiVersion(bool emitErrors = true);

private:
    void handleIncomingControl();

    void resetConnection();
    static void onOutputDataReceivedCb(iox::popo::UntypedSubscriber *subscriber, VariantDataStream *stream);

    bool registerOutPortForwarders();
    void disconnectOutPortForwarders();

private:
    class Private;
    Q_DISABLE_COPY(MLinkModule)
    std::unique_ptr<Private> d;
    friend class Private;
};

} // namespace Syntalos
