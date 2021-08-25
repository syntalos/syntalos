//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.4
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

#include <QSettings>
#include "datafilereader.h"
#include "boardselectdialog.h"

// Check if FrontPanel DLL is loaded, and create an instance of okCFrontPanel.
BoardIdentifier::BoardIdentifier(const QString &bitfileDir, QWidget *parent_) :
    parent(parent_),
    bitfileRootDir(bitfileDir)
{
    qDebug() << "---- Intan Technologies ----\n";
    if (!okFrontPanel_TryLoadLib()) {
#ifdef _WIN32
        QMessageBox::warning(nullptr, "FrontPanel DLL could not be loaded.", "FrontPanel DLL could not be loaded. Make sure 'okFrontPanel.dll' is in the application start directory"
                                                                             " and check that Microsoft Visual C++ 2010 Redistributable x64 is installed");
        qDebug() << "FrontPanel DLL could not be loaded. Make sure this DLL is in the application start directory.";
#elif __APPLE__
        QMessageBox::warning(nullptr, "FrontPanel DyLib could not be loaded.", "FrontPanel DyLib could not be loaded. Make sure 'libokFrontPanel.dylib' is in the Frameworks directory\n"
                                                                             "of the application");
        qDebug() << "FrontPanel DyLib could not be loaded. Make sure DyLib can be found.";
#elif __linux__
        QMessageBox::warning(nullptr, "FrontPanel Shared Object could not be loaded.", "FrontPanel Shared Object could not be loaded. Make sure 'libokFrontPanel.so' is in the\n"
                                                                              "application start directory.");
        qDebug() << "FrontPanel Shared Object could not be loaded. Make sure the .so is in the application start directory";
#endif
        return;
    }
    qDebug() << "FrontPanel DLL loaded. Version: " << okFrontPanel_GetAPIVersionString();

    dev = new okCFrontPanel;
}

// Delete the controllers object.
BoardIdentifier::~BoardIdentifier()
{
    while (controllers.size() > 0) {
        delete controllers.first();
        controllers.remove(0);
    }
}

// Return a QString description of the specified board.
QString BoardIdentifier::getBoardTypeString(BoardMode mode, int numSpiPorts)
{
    switch (mode) {
    case RHDUSBInterfaceBoard:
        return RHDBoardString;
    case RHSController:
        return RHS128chString;
    case RHDController:
        if (numSpiPorts == 4) return RHD512chString;
        else return RHD1024chString;
    case CLAMPController:
        if (numSpiPorts == 2) return CLAMP2chString;
        else return CLAMP8chString;
    case UnknownUSB2Device:
        return UnknownUSB2String;
    case UnknownUSB3Device:
        return UnknownUSB3String;
    default:
        return UnknownString;
    }
}

// Return a QIcon with a picture of the specified board.
QIcon BoardIdentifier::getIcon(const QString& boardType, QStyle *style, int size)
{
    if (boardType == RHDBoardString)
        return QIcon(":/images/usb_interface_board.png");
    else if (boardType == RHS128chString)
        return QIcon(":/images/stim_controller_board.png");
    else if (boardType == RHD512chString)
        return QIcon(":/images/rhd512_controller_board.png");
    else if (boardType == RHD1024chString)
        return QIcon(":/images/rhd1024_controller_board.png");
    else if (boardType == CLAMP2chString)
        return QIcon(":/images/clamp2_controller_board.png");
    else if (boardType == CLAMP8chString)
        return QIcon(":/images/clamp8_controller_board.png");
    else
        return QIcon(style->standardIcon(QStyle::SP_MessageBoxQuestion).pixmap(size));
}

// Return a QVector of ControllerInfo structures containing information about each controller, after uploading bit files
// to each controller and determining its characteristics.
QVector<ControllerInfo*> BoardIdentifier::getConnectedControllersInfo()
{
    int i, nDevices;
    qDebug() << "Scanning USB for Opal Kelly devices...";
    nDevices = dev->GetDeviceCount();
    qDebug() << "Found" << nDevices << "Opal Kelly" << ((nDevices == 1) ? "device" : "devices") << "connected.";

    for (i = 0; i < nDevices; ++i) {
        // Create Controller data structure
        ControllerInfo *controller = new ControllerInfo;

        // Fill Controller data structure with characteristics relating to this controller.
        // This function uploads a bit file to the controller, writes some WireIns, and reads some WireOuts.
        identifyController(controller, i);

        // Add this Controller to the 'controllers' QVector.
        controllers.append(controller);
    }
    delete dev;

    return controllers;
}

// Populate variables in 'controller'. Upload a bit file, writes some WireIns, and reads some WireOuts.
void BoardIdentifier::identifyController(ControllerInfo *controller, int index)
{
    // Populate serialNumber field.
    controller->serialNumber = dev->GetDeviceListSerial(index).c_str();

    // Populate usbVersion field.
    controller->usbVersion = (opalKellyModelName(dev->GetDeviceListModel(index)) == "XEM6010LX45") ? USB2 : USB3;

    // Upload bitfile to determine boardMode, expConnected, and numSPIPorts.
    // Initialize expConnected, numSPIPorts, and boardMode to correspond to an unsuccessful mat.
    controller->expConnected = false;
    controller->numSPIPorts = 0;
    controller->boardMode = UnknownUSB2Device;

    // Open device.
    if (dev->OpenBySerial(dev->GetDeviceListSerial(index).c_str()) != okCFrontPanel::NoError) {
        qDebug() << "Device could not be opened. Is one connected?";
        return;
    }

    // Set up default PLL.
    dev->LoadDefaultPLLConfiguration();

    // Determine proper bitfile to load to FPGA (depending on if USB 2 or 3).
    QString bitfilename = (controller->usbVersion == USB2) ? ConfigFileXEM6010Tester : ConfigFileRHDController;

    // Upload bit file.
    if (!uploadFpgaBitfileQMessageBox(bitfileRootDir + "/" + bitfilename)) {
        QMessageBox::critical(nullptr, QObject::tr("Configuration File Error: Software Aborting"),
                              QObject::tr("Cannot upload configuration file: ") + bitfilename +
                              QObject::tr(".  Make sure file is in the same directory as the executable file."));
        exit(EXIT_FAILURE);
    }
    RHXController::resetBoard(dev);

    // Read mode from board.
    int boardMode = RHXController::getBoardMode(dev);

    if (controller->usbVersion == USB2) {
        // Populate boardMode field for USB2 boards.
        switch (boardMode) {
        case RHDUSBInterfaceBoardMode:
            controller->boardMode = RHDUSBInterfaceBoard;
            controller->numSPIPorts = 4;
            controller->expConnected = false;
            break;
        case RHSControllerBoardMode:
            controller->boardMode = RHSController;
            break;
        case CLAMPControllerBoardMode:
            controller->boardMode = CLAMPController;
            break;
        default:
            controller->boardMode = UnknownUSB2Device;
            return;
        }
    } else {
        // Populate boardMode field for USB3 boards.
        switch (boardMode) {
        case RHDControllerBoardMode:
            controller->boardMode = RHDController;
            break;
        default:
            controller->boardMode = UnknownUSB3Device;
            return;
        }
    }

    // For all boards other than the RHD USB Interface Board, determine the number of SPI ports and whether an expander board
    // is connected.
    if (controller->boardMode != RHDUSBInterfaceBoard) {
        dev->UpdateWireOuts();
        controller->numSPIPorts = RHXController::getNumSPIPorts(dev, (controller->usbVersion == USB3),
                                                                controller->expConnected);
    }
}

// Return name of Opal Kelly board based on model code.
QString BoardIdentifier::opalKellyModelName(int model) const
{
    switch (model) {
    case OK_PRODUCT_XEM3001V1:
        return "XEM3001V1";
    case OK_PRODUCT_XEM3001V2:
        return "XEM3001V2";
    case OK_PRODUCT_XEM3010:
        return "XEM3010";
    case OK_PRODUCT_XEM3005:
        return "XEM3005";
    case OK_PRODUCT_XEM3001CL:
        return "XEM3001CL";
    case OK_PRODUCT_XEM3020:
        return "XEM3020";
    case OK_PRODUCT_XEM3050:
        return "XEM3050";
    case OK_PRODUCT_XEM9002:
        return "XEM9002";
    case OK_PRODUCT_XEM3001RB:
        return "XEM3001RB";
    case OK_PRODUCT_XEM5010:
        return "XEM5010";
    case OK_PRODUCT_XEM6110LX45:
        return "XEM6110LX45";
    case OK_PRODUCT_XEM6001:
        return "XEM6001";
    case OK_PRODUCT_XEM6010LX45:
        return "XEM6010LX45";
    case OK_PRODUCT_XEM6010LX150:
        return "XEM6010LX150";
    case OK_PRODUCT_XEM6110LX150:
        return "XEM6110LX150";
    case OK_PRODUCT_XEM6006LX9:
        return "XEM6006LX9";
    case OK_PRODUCT_XEM6006LX16:
        return "XEM6006LX16";
    case OK_PRODUCT_XEM6006LX25:
        return "XEM6006LX25";
    case OK_PRODUCT_XEM5010LX110:
        return "XEM5010LX110";
    case OK_PRODUCT_ZEM4310:
        return "ZEM4310";
    case OK_PRODUCT_XEM6310LX45:
        return "XEM6310LX45";
    case OK_PRODUCT_XEM6310LX150:
        return "XEM6310LX150";
    case OK_PRODUCT_XEM6110V2LX45:
        return "XEM6110V2LX45";
    case OK_PRODUCT_XEM6110V2LX150:
        return "XEM6110V2LX150";
    case OK_PRODUCT_XEM6002LX9:
        return "XEM6002LX9";
    case OK_PRODUCT_XEM6310MTLX45T:
        return "XEM6310MTLX45T";
    case OK_PRODUCT_XEM6320LX130T:
        return "XEM6320LX130T";
    default:
        return "UNKNOWN";
    }
}

// Upload bitfile specified by 'filename' to the FPGA, reporting any errors that occur as a QMessageBox.
bool BoardIdentifier::uploadFpgaBitfileQMessageBox(const QString& filename)
{
    okCFrontPanel::ErrorCode errorCode = dev->ConfigureFPGA(filename.toStdString());

    switch (errorCode) {
    case okCFrontPanel::NoError:
        break;
    case okCFrontPanel::DeviceNotOpen:
        QMessageBox::critical(parent, "FPGA configuration failed", "Device not open.");
        return false;
    case okCFrontPanel::FileError:
        QMessageBox::critical(parent, "FPGA configuration failed", "Cannot find configuration file.");
        return false;
    case okCFrontPanel::InvalidBitstream:
        QMessageBox::critical(parent, "FPGA configuration failed", "Bitstream is not properly formatted.");
        return false;
    case okCFrontPanel::DoneNotHigh:
        QMessageBox::critical(parent, "FPGA configuration failed", "FPGA DONE signal did not assert after configuration. Make sure switch on Opal Kelly board is set to 'USB' not 'PROM'.");
        return false;
    case okCFrontPanel::TransferError:
        QMessageBox::critical(parent, "FPGA configuration failed", "USB error occurred during download.");
        return false;
    case okCFrontPanel::CommunicationError:
        QMessageBox::critical(parent, "FPGA configuration failed", "Communication error with firmware.");
        return false;
    case okCFrontPanel::UnsupportedFeature:
        QMessageBox::critical(parent, "FPGA configuration failed", "Unsupported feature.");
        return false;
    default:
        QMessageBox::critical(parent, "FPGA configuration failed", "Unknown error.");
        return false;
    }

    // Check for Opal Kelly FrontPanel support in the FPGA configuration.
    if (dev->IsFrontPanelEnabled() == false) {
        QMessageBox::critical(parent, "FPGA configuration failed",
                              "Opal Kelly FrontPanel support is not enabled in this FPGA configuration.");
        return false;
    }

    return true;
}

ScrollableMessageBox::ScrollableMessageBox(QWidget *parent, const QString &title, const QString &text) :
    QDialog(parent),
    message(nullptr),
    okButton(nullptr)
{
    setWindowFlags(windowFlags() ^ Qt::WindowContextHelpButtonHint);

    message = new QLabel(text);
    message->setTextInteractionFlags(Qt::TextSelectableByMouse);

    okButton = new QPushButton(tr("OK"), this);

    connect(okButton, SIGNAL(clicked(bool)), this, SLOT(close()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(message);
    mainLayout->addWidget(okButton);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setWindowTitle(title);
    setLayout(scrollLayout);

    // Set dialog initial size to 25% larger than scrollArea's sizeHint - should avoid scroll bars for default size.
    int initialWidth = std::max((int) round(mainWidget->sizeHint().width() * 1.25), mainWidget->sizeHint().width() + 30);
    int initialHeight = std::max((int) round(mainWidget->sizeHint().height() * 1.25), mainWidget->sizeHint().height() + 30);

    // If initial height is more than 500, cap it at 500.
    initialHeight = std::min(500, initialHeight);

    resize(initialWidth, initialHeight);

    setWindowState(windowState() | Qt::WindowActive);
}

// Create a dialog window for user to select which board's software to initialize.
BoardSelectDialog::BoardSelectDialog(IntanRhxModule *mod, QWidget *parent) :
    QDialog(parent),
    boardTable(nullptr),
    openButton(nullptr),
    defaultSampleRateCheckBox(nullptr),
    defaultSettingsFileCheckBox(nullptr),
    boardIdentifier(nullptr),
    dataFileReader(nullptr),
    rhxController(nullptr),
    state(nullptr),
    controllerInterface(nullptr),
    parser(nullptr),
    controlWindow(nullptr),
    syMod(mod)
{
    // Initialize Board Identifier.
    boardIdentifier = new BoardIdentifier(mod->moduleRootDir(), this);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    // Determine how many and what type of controllers are connected to this machine.
    controllersInfo = boardIdentifier->getConnectedControllersInfo();

    // Create a table containing information about connected controllers.
    boardTable = new QTableWidget(controllersInfo.size(), 3, this);
    populateTable();

    // Allow the user to open the selected board by clicking the open button
    // (can also be done by double-clicking the row in the table).
    openButton = new QPushButton(tr("Open"), this);
    openButton->setEnabled(false);
    connect(openButton, SIGNAL(clicked()), this, SLOT(openSelectedBoard()));

    defaultSampleRateCheckBox = new QCheckBox(this);
    defaultSettingsFileCheckBox = new QCheckBox(this);
    defaultSampleRateCheckBox->setVisible(false);

    QHBoxLayout *firstRowLayout = new QHBoxLayout;
    firstRowLayout->addWidget(defaultSettingsFileCheckBox);
    firstRowLayout->addStretch(1);

    QHBoxLayout *secondRowLayout = new QHBoxLayout;
    secondRowLayout->addWidget(defaultSampleRateCheckBox);
    secondRowLayout->addStretch(1);
    secondRowLayout->addWidget(openButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(boardTable);
    mainLayout->addLayout(firstRowLayout);
    mainLayout->addLayout(secondRowLayout);

    setWindowTitle("Select Intan Controller");

    setLayout(mainLayout);

    resize(minimumSize());

    if (!validControllersPresent(controllersInfo)) {
        showDemoMessageBox();
    } else {
        show();
    }

    // Highlight first enabled row.
    for (int row = 0; row < boardTable->rowCount(); row++) {
        // Get this row's text.
        QString thisText = boardTable->itemAt(row, 0)->text();
        // If this type of board is recognized and enabled, give it focus. Otherwise, move to the next row.
        if (thisText == RHDBoardString || thisText == RHS128chString ||
                thisText == RHD512chString || thisText == RHD1024chString) {
            boardTable->setRangeSelected(QTableWidgetSelectionRange(row, 0, row, 2), true);
            boardTable->setFocus();
            break;
        }
    }
}

BoardSelectDialog::~BoardSelectDialog()
{
    if (boardIdentifier) delete boardIdentifier;
    if (dataFileReader) delete dataFileReader;
    if (controllerInterface) delete controllerInterface;
    if (rhxController) delete rhxController;
    if (parser) delete parser;
//    if (state) delete state;  // This causes "The program has unexpectedly finished" upon quitting for some unknown reason.
}

// Determine whether or not the given QVector of type ControllerInfo contains any valid controllers that can be opened
// by this software (RHD USB interface board, RHD Recording Controller, or RHS Stim/Record Controller).
bool BoardSelectDialog::validControllersPresent(QVector<ControllerInfo*> cInfo)
{
    for (int i = 0; i < cInfo.size(); i++) {
        if (cInfo[i]->boardMode == RHDUSBInterfaceBoard || cInfo[i]->boardMode == RHDController ||
                cInfo[i]->boardMode == RHSController)
            return true;
    }
    return false;
}

ControlWindow *BoardSelectDialog::getControlWindow() const
{
    return controlWindow;
}

ControllerInterface *BoardSelectDialog::getControllerInterface() const
{
    return controllerInterface;
}

SystemState *BoardSelectDialog::systemState() const
{
    return state;
}

void BoardSelectDialog::showDemoMessageBox()
{
    AmplifierSampleRate sampleRate = SampleRate20000Hz;
    StimStepSize stimStepSize = StimStepSize500nA;
    bool rememberSettings = false;

    DemoSelections demoSelection;
    DemoDialog demoDialog(&demoSelection, this);
    demoDialog.exec();

    if (demoSelection == DemoPlayback) {
        playbackDataFile();
    } else {
        ControllerType controllerType;
        if (demoSelection == DemoUSBInterfaceBoard) {
            controllerType = ControllerRecordUSB2;
        } else if (demoSelection == DemoRecordingController) {
            controllerType = ControllerRecordUSB3;
        } else {
            controllerType = ControllerStimRecordUSB2;
        }

        StartupDialog startupDialog(controllerType, &sampleRate, &stimStepSize, &rememberSettings, false, this);
        startupDialog.exec();

        startSoftware(controllerType, sampleRate, stimStepSize, controllerType == ControllerRecordUSB3 ? 8 : 4, true, "N/A",
                      SyntheticMode);

        this->accept();
    }
}

// Fill the table with information corresponding to all connected Opal Kelly devices.
void BoardSelectDialog::populateTable()
{
    // Set up header.
    boardTable->setHorizontalHeaderLabels(QStringList() << "Intan Controller" << "I/O Expander" << "Serial Number");
    boardTable->horizontalHeader()->setSectionsClickable(false);
    boardTable->verticalHeader()->setSectionsClickable(false);
    boardTable->setFocusPolicy(Qt::ClickFocus);

    // Populate each row with information corresponding to a single controller.
    Qt::ItemFlags itemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    for (int row = 0; row < controllersInfo.size(); row++) {

        // Report the type of board.
        QString boardType = BoardIdentifier::getBoardTypeString(controllersInfo[row]->boardMode, controllersInfo[row]->numSPIPorts);
        QTableWidgetItem *intanBoardType = new QTableWidgetItem(BoardIdentifier::getIcon(boardType, style(), 100), boardType);
        // If this type is clamp, add a description to the boardType string.
        if (controllersInfo[row]->boardMode == CLAMPController) {
            intanBoardType->setText(intanBoardType->text().append(tr(" (run Clamp software to use)")));
        }
        intanBoardType->setFlags(itemFlags);
        boardTable->setItem(row, 0, intanBoardType);

        // Report if an io expander is connected.
        QTableWidgetItem *ioExpanderStatus = nullptr;
        if (boardType == RHDBoardString || boardType == UnknownUSB2String ||
                boardType == UnknownUSB3String || boardType == UnknownString) {
            ioExpanderStatus = new QTableWidgetItem(tr("N/A"));
        } else {
            QIcon icon((controllersInfo[row]->expConnected) ?
                           style()->standardIcon(QStyle::SP_DialogYesButton).pixmap(20) :
                           style()->standardIcon(QStyle::SP_DialogNoButton).pixmap(20));
            ioExpanderStatus = new QTableWidgetItem(icon, (controllersInfo[row]->expConnected ?
                                                               tr("I/O Expander Connected") :
                                                               tr("No I/O Expander Connected")));
        }
        ioExpanderStatus->setFlags(itemFlags);
        boardTable->setItem(row, 1, ioExpanderStatus);

        // Report the serial number of this board.
        QTableWidgetItem *serialNumber = new QTableWidgetItem(controllersInfo[row]->serialNumber);
        serialNumber->setFlags(itemFlags);
        boardTable->setItem(row, 2, serialNumber);

        // If the type of board is unrecognized, disable the row (greyed-out and unclickable).
        if (!(boardType == RHDBoardString || boardType == RHS128chString ||
              boardType == RHD512chString || boardType == RHD1024chString)) {
            intanBoardType->setFlags(Qt::NoItemFlags);
            ioExpanderStatus->setFlags(Qt::NoItemFlags);
            serialNumber->setFlags(Qt::NoItemFlags);
        }
    }

    // Make table visible in full (for up to 5 rows... then allow a scroll bar to be used).
    boardTable->setIconSize(QSize(283, 100));
    boardTable->resizeColumnsToContents();
    boardTable->resizeRowsToContents();
    boardTable->setMinimumSize(calculateTableSize());
    boardTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    boardTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(boardTable, SIGNAL(cellDoubleClicked(int, int)),
            this, SLOT(startBoard(int)));  // When the user double clicks a row, trigger that board's software.
    connect(boardTable, SIGNAL(currentCellChanged(int, int, int, int)),
            this, SLOT(newRowSelected(int)));  // When the user selects a valid row, enable 'open' button.
}

// Return a QSize (that should be the minimum size of the table) which allows all columns to be visible, and up to 5 rows
// to be visible before a scroll bar is added.
QSize BoardSelectDialog::calculateTableSize()
{
    int width = boardTable->verticalHeader()->width();
    for (int column = 0; column < boardTable->columnCount(); column++) {
        width += boardTable->columnWidth(column);
    }
    width += 4;

    // Make the minimum height to be 5 rows.
    int numRows = 5;
    if (boardTable->rowCount() <= 5)
        numRows = boardTable->rowCount();
    int height = boardTable->horizontalHeader()->height();
    for (int row = 0; row < numRows; row++) {
        height += boardTable->rowHeight(row);
    }
    height += 4;

    return QSize(width, height);
}

void BoardSelectDialog::startSoftware(ControllerType controllerType, AmplifierSampleRate sampleRate, StimStepSize stimStepSize,
                                      int numSPIPorts, bool expanderConnected, const QString& boardSerialNumber, AcquisitionMode mode)
{
    if (mode == LiveMode) {
        rhxController = new RHXController(controllerType, sampleRate);
    } else if (mode == SyntheticMode) {
        rhxController = new SyntheticRHXController(controllerType, sampleRate);
    } else if (mode == PlaybackMode) {
        rhxController = new PlaybackRHXController(controllerType, sampleRate, dataFileReader);
    } else {
        return;
    }

    state = new SystemState(rhxController, stimStepSize, numSPIPorts, expanderConnected);
    state->setSyntalosModule(syMod);
    state->highDPIScaleFactor = this->devicePixelRatio();  // Use this to adjust graphics for high-DPI monitors.
    state->availableScreenResolution = QGuiApplication::primaryScreen()->geometry();
    controllerInterface = new ControllerInterface(state, rhxController, boardSerialNumber, syMod, dataFileReader, this);
    state->setupGlobalSettingsLoadSave(controllerInterface);
    parser = new CommandParser(state, controllerInterface, this);
    controlWindow = new ControlWindow(state, parser, controllerInterface);

    connect(controlWindow, SIGNAL(sendExecuteCommand(QString)), parser, SLOT(executeCommandSlot(QString)));
    connect(controlWindow, SIGNAL(sendExecuteCommandWithParameter(QString,QString)), parser, SLOT(executeCommandWithParameterSlot(QString, QString)));
    connect(controlWindow, SIGNAL(sendGetCommand(QString)), parser, SLOT(getCommandSlot(QString)));
    connect(controlWindow, SIGNAL(sendSetCommand(QString, QString)), parser, SLOT(setCommandSlot(QString, QString)));

    connect(parser, SIGNAL(stimTriggerOn(QString)), controllerInterface, SLOT(manualStimTriggerOn(QString)));
    connect(parser, SIGNAL(stimTriggerOff(QString)), controllerInterface, SLOT(manualStimTriggerOff(QString)));
    connect(parser, SIGNAL(stimTriggerPulse(QString)), controllerInterface, SLOT(manualStimTriggerPulse(QString)));

    connect(parser, SIGNAL(updateGUIFromState()), controlWindow, SLOT(updateFromState()));
    connect(parser, SIGNAL(sEndOfLineiveNote(QString)), controllerInterface->saveThread(), SLOT(saveLiveNote(QString)));

    connect(controllerInterface, SIGNAL(TCPErrorMessage(QString)), parser, SLOT(TCPErrorSlot(QString)));

    if (dataFileReader) {
        connect(controlWindow, SIGNAL(setDataFileReaderSpeed(double)), dataFileReader, SLOT(setPlaybackSpeed(double)));
        connect(controlWindow, SIGNAL(jumpToStart()), dataFileReader, SLOT(jumpToStart()));
        connect(controlWindow, SIGNAL(jumpToPosition(QString)), dataFileReader, SLOT(jumpToPosition(QString)));
        connect(controlWindow, SIGNAL(jumpRelative(double)), dataFileReader, SLOT(jumpRelative(double)));
        connect(controlWindow, SIGNAL(setStatusBarReadyPlayback()), dataFileReader, SLOT(setStatusBarReady()));
        connect(dataFileReader, SIGNAL(setStatusBar(QString)), controlWindow, SLOT(updateStatusBar(QString)));
        connect(dataFileReader, SIGNAL(setTimeLabel(QString)), controlWindow, SLOT(updateTimeLabel(QString)));
        connect(dataFileReader, SIGNAL(sendSetCommand(QString,QString)), parser, SLOT(setCommandSlot(QString,QString)));
    }

    connect(controllerInterface, SIGNAL(haveStopped()), controlWindow, SLOT(stopAndReportAnyErrors()));
    connect(controllerInterface, SIGNAL(setTimeLabel(QString)), controlWindow, SLOT(updateTimeLabel(QString)));
    connect(controllerInterface, SIGNAL(setTopStatusLabel(QString)), controlWindow, SLOT(updateTopStatusLabel(QString)));
    connect(controllerInterface, SIGNAL(setHardwareFifoStatus(double)), controlWindow, SLOT(updateHardwareFifoStatus(double)));
    connect(controllerInterface, SIGNAL(cpuLoadPercent(double)), controlWindow, SLOT(updateMainCpuLoad(double)));

    connect(controllerInterface->saveThread(), SIGNAL(setStatusBar(QString)), controlWindow, SLOT(updateStatusBar(QString)));
    connect(controllerInterface->saveThread(), SIGNAL(setTimeLabel(QString)), controlWindow, SLOT(updateTimeLabel(QString)));
    connect(controllerInterface->saveThread(), SIGNAL(sendSetCommand(QString, QString)),
            parser, SLOT(setCommandSlot(QString, QString)));
    connect(controllerInterface->saveThread(), SIGNAL(error(QString)), controlWindow, SLOT(queueErrorMessage(QString)));

    controlWindow->show();

#if 0
    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)state->getControllerTypeEnum()]);
    if (defaultSettingsFileCheckBox->isChecked()) {
        settings.setValue("loadDefaultSettingsFile", true);
        QString defaultSettingsFile = QString(settings.value("defaultSettingsFile", "").toString());
        if (controlWindow->loadSettingsFile(defaultSettingsFile)) {
            emit controlWindow->setStatusBar("Loaded default settings file " + defaultSettingsFile);
        } else {
            emit controlWindow->setStatusBar("Error loading default settings file " + defaultSettingsFile);
        }
    } else {
        settings.setValue("loadDefaultSettingsFile", false);
    }
    settings.endGroup();
#endif
}

// Trigger the currently selected board's software.
void BoardSelectDialog::openSelectedBoard()
{
    QTableWidgetItem *selectedItem = boardTable->selectedItems().first();
    startBoard(selectedItem->row());
}

// Enable the 'Open' button when a valid controller's row is selected.
void BoardSelectDialog::newRowSelected(int row)
{
    openButton->setEnabled(true);

    ControllerType controllerType = ControllerRecordUSB3;
    if (boardTable->item(row, 0)->text() == RHDBoardString) controllerType = ControllerRecordUSB2;
    if (boardTable->item(row, 0)->text() == RHS128chString) controllerType = ControllerStimRecordUSB2;

    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)controllerType]);
    bool useDefaultSettings = false; //settings.value("useDefaultSettings", false).toBool();
    bool loadDefaultSettingsFile = false; //settings.value("loadDefaultSettingsFile", false).toBool();

    if (useDefaultSettings) {
        defaultSampleRateCheckBox->setChecked(true);
        defaultSampleRateCheckBox->setVisible(true);
        int defaultSampleRateIndex = settings.value("defaultSampleRate", 14).toInt();
        int defaultStimStepSizeIndex = settings.value("defaultStimStepSize", 6).toInt();
        if (controllerType == ControllerStimRecordUSB2) {
            defaultSampleRateCheckBox->setText(tr("Start software with ") + SampleRateString[defaultSampleRateIndex] +
                                               tr(" sample rate and ") +
                                               StimStepSizeString[defaultStimStepSizeIndex]);
        } else {
            defaultSampleRateCheckBox->setText(tr("Start software with ") + SampleRateString[defaultSampleRateIndex] +
                                               tr(" sample rate"));
        }
    } else {
        defaultSampleRateCheckBox->setChecked(false);
        defaultSampleRateCheckBox->setVisible(false);
    }

    if (loadDefaultSettingsFile) {
        defaultSettingsFileCheckBox->setChecked(true);
        defaultSettingsFileCheckBox->setVisible(true);
        QString defaultSettingsFile = QString(settings.value("defaultSettingsFile", "").toString());
        defaultSettingsFileCheckBox->setText(tr("Load default settings file: ") + defaultSettingsFile);
    } else {
        defaultSettingsFileCheckBox->setChecked(false);
        defaultSettingsFileCheckBox->setVisible(false);
    }

    settings.endGroup();
}

// Trigger the given row's board's software.
void BoardSelectDialog::startBoard(int row)
{
    openButton->setEnabled(false);
    boardTable->setEnabled(false);

    AmplifierSampleRate sampleRate = SampleRate20000Hz;
    StimStepSize stimStepSize = StimStepSize500nA;
    bool rememberSettings = false;

    ControllerType controllerType = ControllerRecordUSB3;
    if (boardTable->item(row, 0)->text() == RHDBoardString) controllerType = ControllerRecordUSB2;
    if (boardTable->item(row, 0)->text() == RHS128chString) controllerType = ControllerStimRecordUSB2;

    QSettings settings;
    settings.beginGroup(ControllerTypeSettingsGroup[(int)controllerType]);
    if (defaultSampleRateCheckBox->isChecked()) {
        sampleRate = (AmplifierSampleRate) settings.value("defaultSampleRate", 14).toInt();
        stimStepSize = (StimStepSize) settings.value("defaultStimStepSize", 6).toInt();
    } else {
        StartupDialog *startupDialog = new StartupDialog(controllerType, &sampleRate, &stimStepSize, &rememberSettings, true, this);
        startupDialog->exec();

        if (rememberSettings) {
            settings.setValue("useDefaultSettings", true);
            settings.setValue("defaultSampleRate", (int) sampleRate);
            settings.setValue("defaultStimStepSize", (int) stimStepSize);
        } else {
            settings.setValue("useDefaultSettings", false);
        }
    }
    settings.endGroup();

    startSoftware(controllerType, sampleRate, stimStepSize, controllersInfo.at(row)->numSPIPorts,
                  controllersInfo.at(row)->expConnected, boardTable->item(row, 2)->text(), LiveMode);

    this->accept();
}

// Allow user to load an Intan data file for playback.
void BoardSelectDialog::playbackDataFile()
{
    QSettings settings;
    QString defaultDirectory = settings.value("playbackDirectory", ".").toString();
    QString playbackFileName;
    playbackFileName = QFileDialog::getOpenFileName(this, tr("Select Intan Data File"), defaultDirectory, tr("Intan Data Files (*.rhd *.rhs)"));

    if (playbackFileName.isEmpty()) {
        exit(EXIT_FAILURE);
    }

    bool canReadFile = false;
    QString report;
    dataFileReader = new DataFileReader(playbackFileName, canReadFile, report);
    if (!canReadFile) {
        ScrollableMessageBox scrollableMessageBox(this, "Unable to Load Data File", report);
        scrollableMessageBox.exec();
        delete dataFileReader;
        dataFileReader = nullptr;
        exit(EXIT_FAILURE);
    } else if (!report.isEmpty()) {
        ScrollableMessageBox scrollableMessageBox(this, "Data File Loaded", report);
        scrollableMessageBox.exec();
        QFileInfo fileInfo(playbackFileName);
        settings.setValue("playbackDirectory", fileInfo.absolutePath());
    }

    startSoftware(dataFileReader->controllerType(), dataFileReader->sampleRate(), dataFileReader->stimStepSize(),
                  dataFileReader->numSPIPorts(), dataFileReader->expanderConnected(), "N/A", PlaybackMode);

    this->accept();
}

