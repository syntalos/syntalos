//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include "tcpdisplay.h"
#include "rhxcontroller.h"

TCPDisplay::TCPDisplay(SystemState* state_, QWidget *parent) :
    QWidget(parent)
{
    state = state_;
    signalSources = state_->signalSources;

    connect(this, SIGNAL(establishWaveformConnection()), state->tcpWaveformDataCommunicator, SLOT(establishConnection()));
    connect(this, SIGNAL(establishSpikeConnection()), state->tcpSpikeDataCommunicator, SLOT(establishConnection()));

    connect(state->tcpCommandCommunicator, SIGNAL(newConnection()), this, SLOT(processNewCommandConnection()));
    connect(state->tcpCommandCommunicator, SIGNAL(statusChanged()), this, SLOT(updateCommandWidgets()));

    connect(state->tcpWaveformDataCommunicator, SIGNAL(newConnection()), this, SLOT(processNewWaveformOutputConnection()));
    connect(state->tcpWaveformDataCommunicator, SIGNAL(statusChanged()), this, SLOT(updateDataOutputWidgets()));

    connect(state->tcpSpikeDataCommunicator, SIGNAL(newConnection()), this, SLOT(processNewSpikeOutputConnection()));
    connect(state->tcpSpikeDataCommunicator, SIGNAL(statusChanged()), this, SLOT(updateDataOutputWidgets()));

    QTabWidget *tabWidget = new QTabWidget(this);

    commandsHostLineEdit = new QLineEdit("127.0.0.1", this);

    waveformOutputHostLineEdit = new QLineEdit(state->tcpWaveformDataCommunicator->address, this);
    spikeOutputHostLineEdit = new QLineEdit(state->tcpSpikeDataCommunicator->address, this);

    connect(waveformOutputHostLineEdit, SIGNAL(textEdited(QString)), this, SLOT(waveformOutputHostEdited()));

    commandsPortSpinBox = new QSpinBox(this);
    commandsPortSpinBox->setRange(0,9999);
    commandsPortSpinBox->setValue(5000);

    waveformOutputPortSpinBox = new QSpinBox(this);
    waveformOutputPortSpinBox->setRange(0,9999);
    waveformOutputPortSpinBox->setValue(state->tcpWaveformDataCommunicator->port);

    spikeOutputPortSpinBox = new QSpinBox(this);
    spikeOutputPortSpinBox->setRange(0,9999);
    spikeOutputPortSpinBox->setValue(state->tcpSpikeDataCommunicator->port);

    connect(waveformOutputPortSpinBox, SIGNAL(valueChanged(int)), this, SLOT(waveformOutputPortChanged()));
    connect(spikeOutputPortSpinBox, SIGNAL(valueChanged(int)), this, SLOT(spikeOutputPortChanged()));

    commandsConnectButton = new QPushButton(tr("Connect"), this);
    connect(commandsConnectButton, SIGNAL(clicked()), this, SLOT(commandsConnect()));

    waveformOutputConnectButton = new QPushButton(tr("Connect"));
    connect(waveformOutputConnectButton, SIGNAL(clicked(bool)), this, SLOT(waveformOutputConnect()));

    spikeOutputConnectButton = new QPushButton(tr("Connect"));
    connect(spikeOutputConnectButton, SIGNAL(clicked(bool)), this, SLOT(spikeOutputConnect()));

    presentChannelsTable = new QTableWidget(1, 1, this);
    presentChannelsTable->horizontalHeader()->setStretchLastSection(true);
    presentChannelsTable->horizontalHeader()->hide();  
    presentChannelsTable->verticalHeader()->hide();
    presentChannelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    presentChannelsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(presentChannelsTable, SIGNAL(itemSelectionChanged()), this, SLOT(selectPresentChannels())); // When the user selects a valid channel (or multiple), enable the 'Add' button.

    addChannelButton = new QPushButton(tr("Add Selected"), this);
    addChannelButton->setEnabled(false);
    connect(addChannelButton, SIGNAL(clicked()), this, SLOT(addChannels()));

    addAllChannelsButton = new QPushButton(tr("Add All"), this);
    connect(addAllChannelsButton, SIGNAL(clicked()), this, SLOT(addAllChannels()));

    filterSelectLabel = new QLabel("Type of data to stream\n(Only applies to\namplifier channels)");

    filterSelectComboBox = new QComboBox(this);
    filterSelectComboBox->addItem("WIDE");
    filterSelectComboBox->addItem("LOW");
    filterSelectComboBox->addItem("HIGH");
    filterSelectComboBox->addItem("SPK");
    if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
        filterSelectComboBox->addItem("DC");
        filterSelectComboBox->addItem("STIM");
    }
    filterSelectComboBox->setCurrentText("WIDE");

    removeChannelButton = new QPushButton(tr("Remove Selected"), this);
    removeChannelButton->setEnabled(false);
    connect(removeChannelButton, SIGNAL(clicked()), this, SLOT(removeChannels()));

    removeAllChannelsButton = new QPushButton(tr("Remove All"), this);
    connect(removeAllChannelsButton, SIGNAL(clicked()), this, SLOT(removeAllChannels()));

    channelsToStreamTable = new QTableWidget(0, 1, this);
    channelsToStreamTable->horizontalHeader()->setStretchLastSection(true);
    channelsToStreamTable->horizontalHeader()->hide();
    channelsToStreamTable->verticalHeader()->hide();
    channelsToStreamTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    channelsToStreamTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(channelsToStreamTable, SIGNAL(itemSelectionChanged()), this, SLOT(selectChannelsToStream())); // When the user selects a valid channel (or multiple), enable the 'Remove' button.

    commandsDisconnectButton = new QPushButton(tr("Disconnect"), this);
    connect(commandsDisconnectButton, SIGNAL(clicked()), this, SLOT(commandsDisconnect()));

    waveformOutputDisconnectButton = new QPushButton(tr("Disconnect"), this);
    connect(waveformOutputDisconnectButton, SIGNAL(clicked()), this, SLOT(waveformOutputDisconnect()));

    spikeOutputDisconnectButton = new QPushButton(tr("Disconnect"), this);
    connect(spikeOutputDisconnectButton, SIGNAL(clicked()), this, SLOT(spikeOutputDisconnect()));

    commandsStatus = new QLabel(tr("Disconnected"), this);

    dataOutputStatus = new QLabel(tr("Disconnected"), this);

    commandTextEdit = new QTextEdit(this);
    commandTextEdit->setReadOnly(true);
    clearCommandsButton = new QPushButton(tr("Clear Commands"), this);
    connect(clearCommandsButton, SIGNAL(clicked()), this, SLOT(clearCommands()));

    errorTextEdit = new QTextEdit(this);
    errorTextEdit->setReadOnly(true);
    clearErrorsButton = new QPushButton(tr("Clear Errors"), this);
    connect(clearErrorsButton, SIGNAL(clicked()), this, SLOT(clearErrors()));

    dataRateStatus = new QLabel(tr("DataRateStatus"), this);

    QHBoxLayout *commandsAddressRow = new QHBoxLayout;
    commandsAddressRow->addWidget(new QLabel(tr("Host"), this));
    commandsAddressRow->addWidget(commandsHostLineEdit);
    commandsAddressRow->addWidget(new QLabel(tr("Port"), this));
    commandsAddressRow->addWidget(commandsPortSpinBox);

    QHBoxLayout *waveformOutputAddressRow = new QHBoxLayout;
    waveformOutputAddressRow->addWidget(new QLabel(tr("Host"), this));
    waveformOutputAddressRow->addWidget(waveformOutputHostLineEdit);
    waveformOutputAddressRow->addWidget(new QLabel(tr("Port"), this));
    waveformOutputAddressRow->addWidget(waveformOutputPortSpinBox);
    waveformOutputAddressRow->addWidget(waveformOutputConnectButton);
    waveformOutputAddressRow->addWidget(waveformOutputDisconnectButton);

    QGroupBox *waveformOutputGroupBox = new QGroupBox(tr("Waveform Output"), this);
    waveformOutputGroupBox->setLayout(waveformOutputAddressRow);

    QHBoxLayout *spikeOutputAddressRow = new QHBoxLayout;
    spikeOutputAddressRow->addWidget(new QLabel(tr("Host"), this));
    spikeOutputAddressRow->addWidget(spikeOutputHostLineEdit);
    spikeOutputAddressRow->addWidget(new QLabel(tr("Port"), this));
    spikeOutputAddressRow->addWidget(spikeOutputPortSpinBox);
    spikeOutputAddressRow->addWidget(spikeOutputConnectButton);
    spikeOutputAddressRow->addWidget(spikeOutputDisconnectButton);

    QGroupBox *spikeOutputGroupBox = new QGroupBox(tr("Spike Output"), this);
    spikeOutputGroupBox->setLayout(spikeOutputAddressRow);

    QHBoxLayout *commandsStatusRow = new QHBoxLayout;
    commandsStatusRow->addWidget(commandsConnectButton);
    commandsStatusRow->addWidget(commandsDisconnectButton);
    commandsStatusRow->addWidget(new QLabel(tr("Status:"), this));
    commandsStatusRow->addWidget(commandsStatus);
    commandsStatusRow->addStretch();

    QHBoxLayout *dataOutputStatusRow1 = new QHBoxLayout;
    dataOutputStatusRow1->addWidget(new QLabel(tr("Status:"), this));
    dataOutputStatusRow1->addWidget(dataOutputStatus);
    dataOutputStatusRow1->addStretch();

    QHBoxLayout *dataOutputStatusRow2 = new QHBoxLayout;
    dataOutputStatusRow2->addWidget(dataRateStatus);
    dataOutputStatusRow2->addStretch();

    QHBoxLayout *clearCommandsRow = new QHBoxLayout;
    clearCommandsRow->addWidget(clearCommandsButton);
    clearCommandsRow->addStretch(1);

    QHBoxLayout *clearErrorsRow = new QHBoxLayout;
    clearErrorsRow->addWidget(clearErrorsButton);
    clearErrorsRow->addStretch(1);

    QVBoxLayout *commandsColumn = new QVBoxLayout;
    commandsColumn->addWidget(new QLabel(tr("Received Commands:"), this));
    commandsColumn->addWidget(commandTextEdit);
    commandsColumn->addLayout(clearCommandsRow);

    QVBoxLayout *errorsColumn = new QVBoxLayout;
    errorsColumn->addWidget(new QLabel(tr("Errors:"), this));
    errorsColumn->addWidget(errorTextEdit);
    errorsColumn->addLayout(clearErrorsRow);

    QHBoxLayout *logRow = new QHBoxLayout;
    logRow->addLayout(commandsColumn);
    logRow->addLayout(errorsColumn);

    QVBoxLayout *column = new QVBoxLayout;
    column->addLayout(commandsAddressRow);
    column->addLayout(commandsStatusRow);
    column->addLayout(logRow);

    QFrame *commandsFrame = new QFrame(this);
    commandsFrame->setLayout(column);

    QVBoxLayout *presentChannelsColumn = new QVBoxLayout;
    presentChannelsColumn->addWidget(new QLabel(tr("Present Channels:"), this));
    presentChannelsColumn->addWidget(presentChannelsTable);

    QVBoxLayout *addRemoveColumn = new QVBoxLayout;
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(addChannelButton);
    addRemoveColumn->addWidget(addAllChannelsButton);
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(filterSelectLabel);
    addRemoveColumn->addWidget(filterSelectComboBox);
    addRemoveColumn->addStretch(2);
    addRemoveColumn->addWidget(removeChannelButton);
    addRemoveColumn->addWidget(removeAllChannelsButton);
    addRemoveColumn->addStretch(1);

    QVBoxLayout *channelsToStreamColumn = new QVBoxLayout;
    channelsToStreamColumn->addWidget(new QLabel(tr("Channels To Stream:"), this));
    channelsToStreamColumn->addWidget(channelsToStreamTable);

    QHBoxLayout *channelsRow = new QHBoxLayout;
    channelsRow->addLayout(presentChannelsColumn);
    channelsRow->addLayout(addRemoveColumn);
    channelsRow->addLayout(channelsToStreamColumn);

    QVBoxLayout *dataOutputColumn = new QVBoxLayout;
    dataOutputColumn->addWidget(waveformOutputGroupBox);
    dataOutputColumn->addWidget(spikeOutputGroupBox);
    dataOutputColumn->addLayout(dataOutputStatusRow1);
    dataOutputColumn->addLayout(dataOutputStatusRow2);
    dataOutputColumn->addLayout(channelsRow);

    QFrame *dataOutputFrame = new QFrame(this);
    dataOutputFrame->setLayout(dataOutputColumn);

    tabWidget->addTab(commandsFrame, tr("Commands"));
    tabWidget->addTab(dataOutputFrame, tr("Data Output"));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(tabWidget);

    updateCommandWidgets();
    updateDataOutputWidgets();

    updateTables();

    QFontMetrics metrics(presentChannelsTable->itemAt(0, 0)->font());
    presentChannelsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    presentChannelsTable->verticalHeader()->setDefaultSectionSize(metrics.height() * 1.5);
    channelsToStreamTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    channelsToStreamTable->verticalHeader()->setDefaultSectionSize(metrics.height() * 1.5);

    setLayout(mainLayout);
}

void TCPDisplay::updateFromState()
{
    waveformOutputHostLineEdit->setText(state->tcpWaveformDataCommunicator->address);
    waveformOutputPortSpinBox->setValue(state->tcpWaveformDataCommunicator->port);
    spikeOutputHostLineEdit->setText(state->tcpSpikeDataCommunicator->address);
    spikeOutputPortSpinBox->setValue(state->tcpSpikeDataCommunicator->port);
    updateDataOutputWidgets();
}

void TCPDisplay::processNewCommandConnection()
{
    if (state->tcpCommandCommunicator->connectionAvailable()) {
        state->tcpCommandCommunicator->establishConnection();
        connect(state->tcpCommandCommunicator, SIGNAL(readyRead()), this, SLOT(readClientCommand()), Qt::QueuedConnection);
    }
}

void TCPDisplay::processNewWaveformOutputConnection()
{
    // Since tcpWaveformDataCommmunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot.
    emit establishWaveformConnection();
}

void TCPDisplay::processNewSpikeOutputConnection()
{
    // Since tcpSpikeDataCommunicator might be in TCPDataOutputThread, instead of calling the slot directly, emit a signal that calls the slot.
    emit establishSpikeConnection();
}

void TCPDisplay::commandsConnect()
{
    if (!state->tcpCommandCommunicator->serverListening()) {
        state->tcpCommandCommunicator->listen(commandsHostLineEdit->text(), commandsPortSpinBox->value());
    }
}

void TCPDisplay::waveformOutputConnect()
{
    emit sendExecuteCommand("connecttcpwaveformdataoutput");
}

void TCPDisplay::spikeOutputConnect()
{
    emit sendExecuteCommand("connecttcpspikedataoutput");
}

void TCPDisplay::commandsDisconnect()
{
    state->tcpCommandCommunicator->returnToDisconnected();
}

void TCPDisplay::waveformOutputDisconnect()
{
    emit sendExecuteCommand("disconnecttcpwaveformdataoutput");
}

void TCPDisplay::spikeOutputDisconnect()
{
    emit sendExecuteCommand("disconnecttcpspikedataoutput");
}

void TCPDisplay::readClientCommand()
{
    QString receivedCommand;
    receivedCommand = state->tcpCommandCommunicator->read();
    commandTextEdit->append(receivedCommand);
    parseCommands(receivedCommand);
}

void TCPDisplay::updateCommandWidgets()
{
    if (state->tcpCommandCommunicator->status == TCPCommunicator::Connected) {
        commandsStatus->setText(tr("Connected"));
        commandsStatus->setStyleSheet("QLabel { color : green; }");
        commandsConnectButton->setEnabled(false);
        commandsDisconnectButton->setEnabled(true);
    } else if (state->tcpCommandCommunicator->status == TCPCommunicator::Pending) {
        commandsStatus->setText(tr("Pending"));
        commandsStatus->setStyleSheet("QLabel {color : orange; }");
        commandsConnectButton->setEnabled(false);
        commandsDisconnectButton->setEnabled(true);
    } else {
        commandsStatus->setText(tr("Disconnected"));
        commandsStatus->setStyleSheet("QLabel {color : red; }");
        commandsConnectButton->setEnabled(true);
        commandsDisconnectButton->setEnabled(false);
    }
}

void TCPDisplay::updateDataOutputWidgets()
{
    if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Connected &&
            state->tcpSpikeDataCommunicator->status == TCPCommunicator::Connected) {
        // If both ports are connected, report it, green

        dataOutputStatus->setText(tr("Both Ports Connected"));
        dataOutputStatus->setStyleSheet("QLabel { color : green; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(true);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Disconnected &&
               state->tcpSpikeDataCommunicator->status == TCPCommunicator::Disconnected) {
        // If both ports are disconnected, report it, red

        dataOutputStatus->setText(tr("Both Ports Disconnected"));
        dataOutputStatus->setStyleSheet("QLabel { color : red; }");
        waveformOutputConnectButton->setEnabled(true);
        spikeOutputConnectButton->setEnabled(true);
        waveformOutputDisconnectButton->setEnabled(false);
        spikeOutputDisconnectButton->setEnabled(false);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Pending &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Pending) {
        // If both ports are pending, report it, orange

        dataOutputStatus->setText(tr("Both Ports Pending"));
        dataOutputStatus->setStyleSheet("QLabel { color : orange; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(true);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Connected &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Disconnected) {
        // If waveform port is connected but spike port is disconnected, report it, red

        dataOutputStatus->setText(tr("Waveform Port Connected, Spike Port Disconnected"));
        dataOutputStatus->setStyleSheet("QLabel { color : red; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(true);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(false);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Disconnected &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Connected) {
        // If waveform port is disconnected but spike port is connected, report it, red

        dataOutputStatus->setText(tr("Waveform Port Disconnected, Spike Port Connected"));
        dataOutputStatus->setStyleSheet("QLabel { color : red; }");
        waveformOutputConnectButton->setEnabled(true);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(false);
        spikeOutputDisconnectButton->setEnabled(true);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Connected &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Pending) {
        // If waveform port is connected but spike port is pending, report it, orange

        dataOutputStatus->setText(tr("Waveform Port Connected, Spike Port Pending"));
        dataOutputStatus->setStyleSheet("QLabel { color : orange; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(true);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Pending &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Connected) {
        // If waveform port is pending but spike port is connected, report it, orange

        dataOutputStatus->setText(tr("Waveform Port Pending, Spike Port Connected"));
        dataOutputStatus->setStyleSheet("QLabel { color : orange; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(true);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Pending &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Disconnected) {
        // If waveform port is pending but spike port is disconnected, report it, red

        dataOutputStatus->setText(tr("Waveform Port Pending, Spike Port Disconnected"));
        dataOutputStatus->setStyleSheet("QLabel { color : red; }");
        waveformOutputConnectButton->setEnabled(false);
        spikeOutputConnectButton->setEnabled(true);
        waveformOutputDisconnectButton->setEnabled(true);
        spikeOutputDisconnectButton->setEnabled(false);
    } else if (state->tcpWaveformDataCommunicator->status == TCPCommunicator::Disconnected &&
             state->tcpSpikeDataCommunicator->status == TCPCommunicator::Pending) {
        // If waveform port is disconnected but spike port is pending, report it, red

        dataOutputStatus->setText(tr("Waveform Port Pending, Spike Port Disconnected"));
        dataOutputStatus->setStyleSheet("QLabel { color : red; }");
        waveformOutputConnectButton->setEnabled(true);
        spikeOutputConnectButton->setEnabled(false);
        waveformOutputDisconnectButton->setEnabled(false);
        spikeOutputDisconnectButton->setEnabled(true);
    } else {
        qDebug() << "We should never get here... unforeseen combination of TCP port connections";
        qDebug() << "Waveform Data status: " << (int) state->tcpWaveformDataCommunicator->status;
        qDebug() << "Spike Data status: " << (int) state->tcpSpikeDataCommunicator->status;
        return;
    }

    if (state->tcpWaveformDataCommunicator->status != TCPCommunicator::Connected ||
            state->tcpSpikeDataCommunicator->status != TCPCommunicator::Connected) {
        dataRateStatus->setText(tr("No TCP connection to stream data over"));
    } else if (!state->running) {
        dataRateStatus->setText(tr("Ready to stream data when board runs"));
    } else {
        int numChannelsToStream = channelsToStreamTable->rowCount();
        QString pluralizedString(tr(" channels "));
        if (numChannelsToStream == 1)
            pluralizedString = tr(" channel ");
        dataRateStatus->setText(tr("Streaming ") + QString::number(numChannelsToStream) + pluralizedString + tr(" at ") + QString::number(state->sampleRate->getNumericValue() / 1000.0) + tr(" kS/s"));
    }

    updateTables();
}

void TCPDisplay::waveformOutputHostEdited()
{
    state->tcpWaveformDataCommunicator->address = waveformOutputHostLineEdit->text();
}

void TCPDisplay::spikeOutputHostEdited()
{
    state->tcpSpikeDataCommunicator->address = spikeOutputHostLineEdit->text();
}

void TCPDisplay::waveformOutputPortChanged()
{
    state->tcpWaveformDataCommunicator->port = waveformOutputPortSpinBox->value();
}

void TCPDisplay::spikeOutputPortChanged()
{
    state->tcpSpikeDataCommunicator->port = spikeOutputPortSpinBox->value();
}

void TCPDisplay::clearCommands()
{
    commandTextEdit->clear();
}

void TCPDisplay::clearErrors()
{
    errorTextEdit->clear();
}

void TCPDisplay::selectPresentChannels()
{
    //bool changeChannelsAllowed = !(state->running && state->globalTcpDataOutputEnabled->getValue());
    bool changeChannelsAllowed = !state->running;
    if (presentChannelsTable->selectedItems().size() > 0)
        addChannelButton->setEnabled(changeChannelsAllowed);
    else
        addChannelButton->setEnabled(false);

    // Uncomment this to enable/disable filter select combo box based on if an amplifier channel is selected.
    // Note: we actually probably don't want this behavior, since Add All will always be an option the combo box will be relevant for.

//    bool ampSignalsSelected = false;
//    // Go through all selected items to see if they contain any AmplifierSignals.
//    for (int i = 0; i < presentChannelsTable->selectedItems().size(); ++i) {
//        Channel *presentChannel = signalSources->findChannelFromName(presentChannelsTable->selectedItems().at(i)->text());
//        if (presentChannel->getSignalType() == AmplifierSignal) {
//            ampSignalsSelected = true;
//            break;
//        }
//    }

//    // If at least one AmplifierSignal is selected, then enable the filterSelectComboBox.
//    // Otherwise, disable it.
//    filterSelectComboBox->setEnabled(ampSignalsSelected);
}

void TCPDisplay::selectChannelsToStream()
{
    //bool changeChannelsAllowed = !(state->running && state->globalTcpDataOutputEnabled->getValue());
    bool changeChannelsAllowed = !state->running;
    if (channelsToStreamTable->selectedItems().size() > 0)
        removeChannelButton->setEnabled(changeChannelsAllowed);
    else
        removeChannelButton->setEnabled(false);
}

void TCPDisplay::addChannels()
{
    for (int channel = 0; channel < presentChannelsTable->selectedItems().size(); channel++) {
        addChannel(presentChannelsTable->selectedItems()[channel]->text());
    }
}

// If this channel is an amplifier signal, then consult filterSelectComboBox to see which filter should be added.
void TCPDisplay::addChannel(const QString& channelName)
{
    Channel *thisChannel = signalSources->channelByName(channelName);
    if (!thisChannel)
        return;
    if (thisChannel->getSignalType() == AmplifierSignal) {
        if (filterSelectComboBox->currentText() == "WIDE") {
            thisChannel->setOutputToTcp(true);
        } else if (filterSelectComboBox->currentText() == "LOW") {
            thisChannel->setOutputToTcpLow(true);
        } else if (filterSelectComboBox->currentText() == "HIGH") {
            thisChannel->setOutputToTcpHigh(true);
        } else if (filterSelectComboBox->currentText() == "SPK") {
            thisChannel->setOutputToTcpSpike(true);
        } else if (filterSelectComboBox->currentText() == "DC") {
            thisChannel->setOutputToTcpDc(true);
        } else if (filterSelectComboBox->currentText() == "STIM") {
            thisChannel->setOutputToTcpStim(true);
        }
    } else {
        signalSources->channelByName(channelName)->setOutputToTcp(true);
    }
}

void TCPDisplay::addAllChannels()
{
    for (int channel = 0; channel < presentChannelsTable->rowCount(); channel++) {
        if (!presentChannelsTable->item(channel, 0)) break;
        addChannel(presentChannelsTable->item(channel, 0)->text());
    }
}

void TCPDisplay::removeChannels()
{
    for (int channel = 0; channel < channelsToStreamTable->selectedItems().size(); channel++) {
        removeChannel(channelsToStreamTable->selectedItems()[channel]->text());
    }
}

void TCPDisplay::removeChannel(const QString& channelName)
{
    // If the '|' character is present in the channelName, then we know it's a filter for AmplifierSignal.
    // In that case, parse to get the filter and nativeChannelName.
    if (channelName.contains('|')) {
        QStringList separatedChannelName = channelName.split('|');
        QString nativeChannelName = separatedChannelName.at(0);
        QString filterName = separatedChannelName.at(1);
        if (filterName == "WIDE")
            signalSources->channelByName(nativeChannelName)->setOutputToTcp(false);
        else if (filterName == "LOW")
            signalSources->channelByName(nativeChannelName)->setOutputToTcpLow(false);
        else if (filterName == "HIGH")
            signalSources->channelByName(nativeChannelName)->setOutputToTcpHigh(false);
        else if (filterName == "SPK")
            signalSources->channelByName(nativeChannelName)->setOutputToTcpSpike(false);
        else if (filterName == "DC")
            signalSources->channelByName(nativeChannelName)->setOutputToTcpDc(false);
        else if (filterName == "STIM")
            signalSources->channelByName(nativeChannelName)->setOutputToTcpStim(false);
        else
            return;
    } else { // Otherwise, just toggle the channel's outputToTcp
        signalSources->channelByName(channelName)->setOutputToTcp(false);
    }
}

void TCPDisplay::removeAllChannels()
{
    for (int channel = 0; channel < channelsToStreamTable->rowCount(); channel++) {
        removeChannel(channelsToStreamTable->item(channel, 0)->text());
    }
}

void TCPDisplay::updateTables()
{
    updatePresentChannelsTable();

    updateChannelsToStreamTable();

    // If channelsToStreamTable is empty, then disable 'Remove All'.
    //bool changeChannelsAllowed = !(state->running && state->globalTcpDataOutputEnabled->getValue());
    bool changeChannelsAllowed = !state->running;
    removeAllChannelsButton->setEnabled(channelsToStreamTable->rowCount() != 0 && changeChannelsAllowed);

    // If changeChannelsAllowed and all bands of all channels are in channelsToStreamTable, then disable 'Add All'.
    int maxTcpChannels = 0;
    for (int group = 0; group < signalSources->numGroups(); ++group) {
        SignalGroup *thisGroup = signalSources->groupByIndex(group);
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel *thisChannel = thisGroup->channelByIndex(channel);
            if (thisChannel->getSignalType() == AmplifierSignal) {
                if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    maxTcpChannels += 6;
                } else {
                    maxTcpChannels += 4;
                }
            } else {
                maxTcpChannels++;
            }
        }
    }
    if (changeChannelsAllowed)
        addAllChannelsButton->setEnabled(channelsToStreamTable->rowCount() != maxTcpChannels);
}

void TCPDisplay::updatePresentChannelsTable()
{
    // Scan through all channels.
    vector<string> presentChannelsVector;
    for (int group = 0; group < state->signalSources->numGroups(); ++group) {
        SignalGroup* thisGroup = state->signalSources->groupByIndex(group);
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel* thisChannel = thisGroup->channelByIndex(channel);
            // Add all channels to present channels.
            presentChannelsVector.insert(presentChannelsVector.end(), thisChannel->getNativeNameString());
        }
    }

    if (presentChannelsTable->rowCount() != (int) presentChannelsVector.size()) {
        presentChannelsTable->clear();
        presentChannelsTable->setRowCount((int) presentChannelsVector.size());
    }
    presentChannelsTable->setFocusPolicy(Qt::ClickFocus);

    Qt::ItemFlags presentChannelsFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    for (int channel = 0; channel < (int) presentChannelsVector.size(); ++channel) {
        QString thisChannelString = QString::fromStdString(presentChannelsVector[channel]);

        // If this channel exists, add it to presentChannelsTable.
        QTableWidgetItem *presentChannelItem = new QTableWidgetItem(thisChannelString);
        presentChannelItem->setFlags(presentChannelsFlags);
        presentChannelsTable->setItem(channel, 0, presentChannelItem);

        Channel* thisChannel = signalSources->channelByName(thisChannelString);
        if (!thisChannel) continue;

        // If the channel is tcp enabled (for Amplifier Signals if all filters are),
        // disable the channel in presentChannelsTable.
        bool fullyOutput = false;
        if (thisChannel->getSignalType() == AmplifierSignal) {
            if (thisChannel->getOutputToTcp() && thisChannel->getOutputToTcpLow() &&
                    thisChannel->getOutputToTcpHigh() && thisChannel->getOutputToTcpSpike()) {
                if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    if (thisChannel->getOutputToTcpDc() && thisChannel->getOutputToTcpStim()) {
                        fullyOutput = true;
                    }
                } else {
                    fullyOutput = true;
                }
            }
        } else {
            if (thisChannel->getOutputToTcp()) {
                fullyOutput = true;
            }
        }

        if (fullyOutput)
            presentChannelsTable->item(channel, 0)->setFlags(Qt::NoItemFlags);
        else
            presentChannelsTable->item(channel, 0)->setFlags(presentChannelsFlags);
    }
}

void TCPDisplay::updateChannelsToStreamTable()
{
    // Scan through all channels.
    vector<string> channelsToStreamVector;
    for (int group = 0; group < state->signalSources->numGroups(); ++group) {
        SignalGroup* thisGroup = state->signalSources->groupByIndex(group);
        for (int channel = 0; channel < thisGroup->numChannels(); ++channel) {
            Channel* thisChannel = thisGroup->channelByIndex(channel);

            // If this is an AmplifierSignal, add all TCP-enabled filters to present channels.
            // Otherwise, if its channel output is TCP-enabled, add it to present channels.
            if (thisChannel->getSignalType() == AmplifierSignal) {
                if (thisChannel->getOutputToTcp())
                    channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|WIDE");
                if (thisChannel->getOutputToTcpLow())
                    channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|LOW");
                if (thisChannel->getOutputToTcpHigh())
                    channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|HIGH");
                if (thisChannel->getOutputToTcpSpike())
                    channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|SPK");
                if (state->getControllerTypeEnum() == ControllerStimRecordUSB2) {
                    if (thisChannel->getOutputToTcpDc())
                        channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|DC");
                    if (thisChannel->getOutputToTcpStim())
                        channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString() + "|STIM");
                }
            } else {
                if (thisChannel->getOutputToTcp())
                    channelsToStreamVector.insert(channelsToStreamVector.end(), thisChannel->getNativeNameString());
            }
        }
    }

    channelsToStreamTable->clear();
    channelsToStreamTable->setRowCount(0);

    Qt::ItemFlags channelsToStreamFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    for (int channel = 0; channel < (int) channelsToStreamVector.size(); ++channel) {
        QString thisChannelString = QString::fromStdString(channelsToStreamVector[channel]);
        QTableWidgetItem *channelToStreamItem = new QTableWidgetItem(thisChannelString);
        channelToStreamItem->setFlags(channelsToStreamFlags);
        channelsToStreamTable->setRowCount(channelsToStreamTable->rowCount() + 1);
        channelsToStreamTable->setItem(channelsToStreamTable->rowCount() - 1, 0, channelToStreamItem);
    }
}

void TCPDisplay::parseCommands(const QString& commands)
{
    // For case-insensitivity, read all commands as just lower-case.

    // Separate each command by a semicolon.
    QStringList commandsList = commands.split(';');

    // Accept a semicolon at the end of the last command.
    if (commandsList.last().isEmpty())
        commandsList.removeLast();

    // For each command, determine its syntax validity. Good syntax should result in a signal emission, bad syntax should result in a TCP error message being sent.
    for (int i = 0; i < commandsList.size(); i++) {

        QStringList words = commandsList.at(i).split(' ');

        // Ignore any empty space at the beginning or end of a command.
        for (int j = 0; j < words.size(); j++) {
            words.replace(j, words.at(j).trimmed());
        }
        words.removeAll("");

        if (words.at(0).toLower() == "set") {
            // "Set" syntax: "set" + parameter + value

            // Exception for "note1", "note2", "note3" "filename", and "impedancefilename" - allow value to have spaces
            if (words.at(1).toLower() == "note1" ||
                    words.at(1).toLower() == "note2" ||
                    words.at(1).toLower() == "note3" ||
                    words.at(1).toLower().startsWith(state->filename->getParameterName().toLower()) ||
                    words.at(1).toLower().startsWith(state->impedanceFilename->getParameterName().toLower())) {
                QString noteValue;
                for (int k = 2; k < words.size(); k++) {
                    if (k < words.size() - 1) {
                        noteValue = noteValue + words.at(k) + " ";
                    } else {
                        noteValue = noteValue + words.at(k);
                    }
                }
                emit sendSetCommand(words.at(1), noteValue);
            } else if (words.size() == 3) {
                emit sendSetCommand(words.at(1), words.at(2));
            } else {
                QString errorMessage = "Error - Command " + QString::number(i + 1) + ": Set commands require a parameter and a value";
                state->tcpCommandCommunicator->writeQString(errorMessage);
            }
        } else if (words.at(0).toLower() == "get") {
            // "Get" syntax: "get" + parameter
            if (words.size() == 2) {
                emit sendGetCommand(words.at(1));
            } else {
                QString errorMessage = "Error - Command " + QString::number(i + 1) + ": Get commands require a parameter";
                state->tcpCommandCommunicator->writeQString(errorMessage);
            }
        } else if (words.at(0).toLower() == "execute") {
            // "Execute" syntax: "execute" + action
            if (words.size() == 2) {
                emit sendExecuteCommand(words.at(1));
            } else if (words.size() == 3) {
                emit sendExecuteCommandWithParameter(words.at(1), words.at(2));
            } else {
                QString errorMessage = "Error - Command " + QString::number(i + 1) + ": Execute commands require an action";
                state->tcpCommandCommunicator->writeQString(errorMessage);
            }
        } else if (words.at(0).toLower() == "livenotes") {
            // "LiveNotes" syntax: "livenotes" + action
            emit sendNoteCommand(commandsList.at(i).mid(10));
        } else {
            // Unrecognized command
            QString errorMessage = "Error - Command " + QString::number(i + 1) + ": Unrecognized command";
            state->tcpCommandCommunicator->writeQString(errorMessage);
        }
    }
}

void TCPDisplay::TCPReturn(QString result)
{
    state->tcpCommandCommunicator->writeQString(result);
}

void TCPDisplay::TCPError(QString errorString)
{
    errorTextEdit->append(errorString);
    state->tcpCommandCommunicator->writeQString(errorString);
}
