/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "chanexportdialog.h"

#include "signalsources.h"

ChanExportDialog::ChanExportDialog(SystemState *state, QWidget *parent) :
    QWidget(parent)
{
    m_state = state;
    m_signalSources = m_state->signalSources;

    m_availableChannelsTable = new QTableWidget(1, 1, this);
    m_availableChannelsTable->horizontalHeader()->setStretchLastSection(true);
    m_availableChannelsTable->horizontalHeader()->hide();
    m_availableChannelsTable->verticalHeader()->hide();
    m_availableChannelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_availableChannelsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_availableChannelsTable, &QTableWidget::itemSelectionChanged,
            this, &ChanExportDialog::availableChannelSelected);

    m_addChannelButton = new QPushButton("Add Selected", this);
    m_addChannelButton->setEnabled(false);
    connect(m_addChannelButton, &QPushButton::clicked, this, &ChanExportDialog::addChannels);

    m_addAllChannelsButton = new QPushButton("Add All", this);
    connect(m_addAllChannelsButton, &QPushButton::clicked, this, &ChanExportDialog::addAllChannels);

    m_filterSelectLabel = new QLabel("Type of data to stream\n(Only applies to\namplifier channels)");

    m_filterSelectComboBox = new QComboBox(this);
    m_filterSelectComboBox->addItem("WIDE");
    m_filterSelectComboBox->addItem("LOW");
    m_filterSelectComboBox->addItem("HIGH");
    m_filterSelectComboBox->addItem("SPK");
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        m_filterSelectComboBox->addItem("DC");
        m_filterSelectComboBox->addItem("STIM");
    }
    m_filterSelectComboBox->setCurrentText("WIDE");
    m_filterSelectLabel->setVisible(false);
    m_filterSelectComboBox->setVisible(false);

    m_removeChannelButton = new QPushButton("Remove Selected", this);
    m_removeChannelButton->setEnabled(false);
    connect(m_removeChannelButton, &QPushButton::clicked, this, &ChanExportDialog::removeChannels);

    m_removeAllChannelsButton = new QPushButton("Remove All", this);
    connect(m_removeAllChannelsButton, &QPushButton::clicked, this, &ChanExportDialog::removeAllChannels);

    m_exportChannelsTable = new QTableWidget(0, 1, this);
    m_exportChannelsTable->horizontalHeader()->setStretchLastSection(true);
    m_exportChannelsTable->horizontalHeader()->hide();
    m_exportChannelsTable->verticalHeader()->hide();
    m_exportChannelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_exportChannelsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QVBoxLayout *presentChannelsColumn = new QVBoxLayout;
    presentChannelsColumn->addWidget(new QLabel("Available Channels:", this));
    presentChannelsColumn->addWidget(m_availableChannelsTable);

    QVBoxLayout *addRemoveColumn = new QVBoxLayout;
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(m_addChannelButton);
    addRemoveColumn->addWidget(m_addAllChannelsButton);
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(m_filterSelectLabel);
    addRemoveColumn->addWidget(m_filterSelectComboBox);
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(m_removeChannelButton);
    addRemoveColumn->addWidget(m_removeAllChannelsButton);
    addRemoveColumn->addStretch(1);

    QVBoxLayout *channelsToStreamColumn = new QVBoxLayout;
    channelsToStreamColumn->addWidget(new QLabel("Channels To Export:", this));
    channelsToStreamColumn->addWidget(m_exportChannelsTable);

    QHBoxLayout *channelsRow = new QHBoxLayout;
    channelsRow->addLayout(presentChannelsColumn);
    channelsRow->addLayout(addRemoveColumn);
    channelsRow->addLayout(channelsToStreamColumn);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    //dataOutputColumn->addWidget(waveformOutputGroupBox);
    mainLayout->addLayout(channelsRow);

    updateAvailableChannelsTable();

    QFontMetrics metrics(m_availableChannelsTable->itemAt(0, 0)->font());
    m_availableChannelsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_availableChannelsTable->verticalHeader()->setDefaultSectionSize(metrics.height() * 1.5);
    m_exportChannelsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_exportChannelsTable->verticalHeader()->setDefaultSectionSize(metrics.height() * 1.5);

    setLayout(mainLayout);

    setWindowIcon(QIcon(":/module/intan-rhx"));
    setWindowTitle("Select Exported Channels");

    setGeometry(QStyle::alignedRect(
                    Qt::LeftToRight,
                    Qt::AlignCenter,
                    size(),
                    qApp->screens()[0]->availableGeometry()));
}

void ChanExportDialog::updateAvailableChannelsTable()
{
    // Scan through all channels.
    std::vector<string> presentChannelsVector;
    for (int group = 0; group < m_state->signalSources->numGroups(); ++group) {
        SignalGroup* thisGroup = m_state->signalSources->groupByIndex(group);
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel* thisChannel = thisGroup->channelByIndex(channel);
            // Add all channels to present channels.
            presentChannelsVector.insert(presentChannelsVector.end(), thisChannel->getNativeNameString());
        }
    }

    if (m_availableChannelsTable->rowCount() != (int) presentChannelsVector.size()) {
        m_availableChannelsTable->clear();
        m_availableChannelsTable->setRowCount((int) presentChannelsVector.size());
    }
    m_availableChannelsTable->setFocusPolicy(Qt::ClickFocus);

    const auto selectableItemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    for (int channel = 0; channel < (int) presentChannelsVector.size(); ++channel) {
        QString thisChannelString = QString::fromStdString(presentChannelsVector[channel]);

        // If this channel exists, add it to m_availableChannelsTable.
        QTableWidgetItem *presentChannelItem = new QTableWidgetItem(thisChannelString);
        presentChannelItem->setFlags(selectableItemFlags);
        m_availableChannelsTable->setItem(channel, 0, presentChannelItem);

        auto thisChannel = m_signalSources->channelByName(thisChannelString);
        if (!thisChannel) continue;

        m_availableChannelsTable->item(channel, 0)->setFlags(selectableItemFlags);
    }
}

void ChanExportDialog::updateExportChannelsTable()
{
    m_exportChannelsTable->clear();
    m_exportChannelsTable->setRowCount(0);

    const auto channelsToStreamFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    for (const auto &chanName : m_exportedChannels.keys()) {
        QTableWidgetItem *channelToStreamItem = new QTableWidgetItem(chanName);
        channelToStreamItem->setFlags(channelsToStreamFlags);
        m_exportChannelsTable->setRowCount(m_exportChannelsTable->rowCount() + 1);
        m_exportChannelsTable->setItem(m_exportChannelsTable->rowCount() - 1, 0, channelToStreamItem);

    }
    m_removeChannelButton->setEnabled(!m_exportedChannels.isEmpty());

    emit exportedChannelsChanged(m_exportedChannels.values());
}

QStringList ChanExportDialog::exportedChannelNames() const
{
    return m_exportedChannels.keys();
}

void ChanExportDialog::availableChannelSelected()
{
    bool changeChannelsAllowed = !m_state->running && (m_availableChannelsTable->selectedItems().size() > 0);
    m_addChannelButton->setEnabled(changeChannelsAllowed);
}


void ChanExportDialog::addChannels()
{
    for (int channel = 0; channel < m_availableChannelsTable->selectedItems().size(); channel++) {
        addChannel(m_availableChannelsTable->selectedItems()[channel]->text(), false);
    }

    updateExportChannelsTable();
}

void ChanExportDialog::addChannel(const QString &channelName, bool notify)
{
    Channel *thisChannel = m_signalSources->channelByName(channelName);
    if (!thisChannel)
        return;
    m_exportedChannels[channelName] = thisChannel;
    if (notify)
        updateExportChannelsTable();
}

void ChanExportDialog::addAllChannels()
{
    for (int channel = 0; channel < m_availableChannelsTable->rowCount(); channel++) {
        if (!m_availableChannelsTable->item(channel, 0)) break;
        addChannel(m_availableChannelsTable->item(channel, 0)->text(), false);
    }

    updateExportChannelsTable();
}

void ChanExportDialog::removeChannels()
{
    for (int channel = 0; channel < m_exportChannelsTable->selectedItems().size(); channel++) {
        removeChannel(m_exportChannelsTable->selectedItems()[channel]->text(), false);
    }

    updateExportChannelsTable();
}

void ChanExportDialog::removeChannel(const QString &channelName, bool notify)
{
    m_exportedChannels.remove(channelName);
    if (notify)
        updateExportChannelsTable();
}

void ChanExportDialog::removeAllChannels()
{
    for (int channel = 0; channel < m_exportChannelsTable->rowCount(); channel++) {
        removeChannel(m_exportChannelsTable->item(channel, 0)->text(), false);
    }
    updateExportChannelsTable();
}
