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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "moduleapi.h"
#include "optionalwaitcondition.h"
#include <QObject>

namespace Syntalos
{

Q_DECLARE_LOGGING_CATEGORY(logSExporter)

struct ExportedStreamInfo {
    QString instanceId;
    QString channelId;
};

/**
 * @brief Exporter for streaming data from modules
 */
class StreamExporter : public QObject
{
    Q_OBJECT
public:
    explicit StreamExporter(const QString &threadName = QString(), QObject *parent = nullptr);
    ~StreamExporter();

    bool isRunning() const;
    bool isFailed() const;
    QString threadName() const;

    void setFailed(bool failed);

    std::optional<ExportedStreamInfo> publishStreamByPort(std::shared_ptr<VarStreamInputPort> iport);

    void run(OptionalWaitCondition *waitCondition);
    void stop();

signals:
    void failed();

private:
    class Private;
    Q_DISABLE_COPY(StreamExporter)
    QScopedPointer<Private> d;

    void shutdownThread();
    void streamEventThreadFunc(OptionalWaitCondition *waitCondition);
};

} // namespace Syntalos
