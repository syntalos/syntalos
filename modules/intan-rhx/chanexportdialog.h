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

#include <QtWidgets>
#include "systemstate.h"
#include "channel.h"

class QPushButton;
class QLabel;
class QTableWidget;

class ChanExportDialog : public QWidget
{
    Q_OBJECT
public:
    explicit ChanExportDialog(SystemState *state, QWidget *parent = nullptr);
    void updateFromState();

public slots:
    void addChannel(const QString &channelName, bool notify = true);
    void removeAllChannels();
    void updateExportChannelsTable();
    QStringList exportedChannelNames() const;

private slots:
    void availableChannelSelected();

    void addChannels();

    void addAllChannels();
    void removeChannels();
    void removeChannel(const QString &channelName, bool notify = true);

signals:
    void exportedChannelsChanged(const QList<Channel*> &channels);

private:
    QTableWidget *m_availableChannelsTable;

    QPushButton *m_addChannelButton;
    QPushButton *m_removeChannelButton;

    QPushButton *m_addAllChannelsButton;
    QPushButton *m_removeAllChannelsButton;

    QLabel *m_filterSelectLabel;
    QComboBox *m_filterSelectComboBox;

    QTableWidget *m_exportChannelsTable;

    SystemState *m_state;
    SignalSources *m_signalSources;

    QMap<QString, Channel*> m_exportedChannels;

    void updateAvailableChannelsTable();
};
