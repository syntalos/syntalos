/*
 * Copyright (C) 2022-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "upywbenchmodule.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <KActionCollection>
#pragma GCC diagnostic pop
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDebug>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QShortcut>
#include <QSplitter>
#include <QDir>
#include <QSerialPort>
#include <QMessageBox>
#include <QMenu>
#include <QToolBar>
#include <QTimer>
#include <QComboBox>
#include <QSerialPortInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "moduleapi.h"
#include "datactl/datatypes.h"
#include "porteditordialog.h"
#include "utils/style.h"
#include "utils/misc.h"

#include "upyconsole.h"

SYNTALOS_MODULE(UPyWBenchModule);

namespace Syntalos
{
Q_LOGGING_CATEGORY(logUPyWB, "mod.upy-workbench")
}

static constexpr std::chrono::seconds UPY_SERIAL_WRITE_TIMEOUT = std::chrono::seconds{6};

class UPyWBenchModule : public AbstractModule
{
    Q_OBJECT

signals:
    void receivedUserData(const QByteArray &data);

public:
    explicit UPyWBenchModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_codeWindow(nullptr),
          m_timer(new QTimer(this)),
          m_userSerial(new QSerialPort(this))
    {
        // set up code editor
        auto editor = KTextEditor::Editor::instance();

        // create a new document
        auto upyDoc = editor->createDocument(this);

        // load templates
        QFile upyTmplCodeRc(QStringLiteral(":/code/micropy-template.py"));
        if (upyTmplCodeRc.open(QIODevice::ReadOnly))
            upyDoc->setText(upyTmplCodeRc.readAll());
        upyTmplCodeRc.close();

        QFile upyCommsRc(QStringLiteral(":/code/upy-comms.py"));
        if (upyCommsRc.open(QIODevice::ReadOnly))
            m_commCode = upyCommsRc.readAll();
        else
            qCCritical(logUPyWB, "Failed to load autobuild helper");
        upyCommsRc.close();

        // configure UI
        m_codeWindow = new QWidget;
        addDisplayWindow(m_codeWindow);

        m_codeWindow->setWindowIcon(QIcon(":/icons/generic-config"));
        m_codeWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));

        m_codeView = upyDoc->createView(m_codeWindow);
        upyDoc->setHighlightingMode("Python");

        m_consoleWidget = new UPyConsole(m_codeWindow);

        // create main toolbar
        auto toolbar = new QToolBar(m_codeWindow);
        toolbar->setMovable(false);
        toolbar->layout()->setMargin(2);
        m_codeWindow->resize(800, 920);
        m_testRunAction = toolbar->addAction("Test Run");
        setWidgetIconFromResource(m_testRunAction, "upy-testrun");
        m_devResetAction = toolbar->addAction("Reset Device");
        m_devResetAction->setIcon(QIcon::fromTheme("view-refresh"));
        m_devResetAction->setToolTip("Reset the device and abort all running code");
        toolbar->addSeparator();
        m_portEditAction = toolbar->addAction("Edit Ports");
        setWidgetIconFromResource(m_portEditAction, "edit-ports");

        // add port selector
        toolbar->addSeparator();
        m_serialSelector = new QComboBox(toolbar);
        m_serialSelector->setMinimumWidth(140);
        m_serialSelector->setToolTip("The serial port to connect to the device");
        toolbar->addWidget(m_serialSelector);
        updateSerialPortsList();
        m_devConnectAction = toolbar->addAction("Connect Device");
        m_devConnectAction->setCheckable(true);
        setWidgetIconFromResource(m_devConnectAction, "chip-connect");

        auto spacer = new QWidget(toolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);

        m_portsDialog = new PortEditorDialog(this, m_codeWindow);

        // We can only realistically transmit tabular data, as we send/receive via stdin/stdout
        // and perform text parsing for simplicity
        m_portsDialog->setAllowedInputTypes({BaseDataType::TableRow});
        m_portsDialog->setAllowedOutputTypes(
            {BaseDataType::TableRow, BaseDataType::FloatSignalBlock, BaseDataType::IntSignalBlock});

        // combine the UI elements into the main layout
        auto splitter = new QSplitter(Qt::Vertical, m_codeWindow);
        splitter->addWidget(m_codeView);
        splitter->addWidget(m_consoleWidget);
        splitter->setStretchFactor(0, 8);
        splitter->setStretchFactor(1, 1);
        auto codeLayout = new QVBoxLayout(m_codeWindow);
        m_codeWindow->setLayout(codeLayout);
        codeLayout->setMargin(0);
        codeLayout->addWidget(toolbar);
        codeLayout->addWidget(splitter);

        // connect UI events
        connect(m_portEditAction, &QAction::triggered, this, [this](bool) {
            m_portsDialog->updatePortLists();
            m_portsDialog->exec();
        });

        connect(m_devConnectAction, &QAction::toggled, this, [this](bool checked) {
            m_testRunAction->setEnabled(false);
            m_devResetAction->setEnabled(false);
            if (checked) {
                if (connectUserInteractiveDevice()) {
                    m_devConnectAction->setText("Disconnect Device");
                    m_consoleWidget->clear();
                    m_consoleWidget->setVisible(true);
                    m_testRunAction->setEnabled(true);
                    m_devResetAction->setEnabled(true);
                } else {
                    m_devConnectAction->setChecked(false);
                }
            } else {
                m_userSerial->close();
                m_devConnectAction->setText("Connect Device");
            }
        });

        connect(m_testRunAction, &QAction::triggered, this, [this](bool) {
            upySoftReset(m_userSerial);
            upyRawReplExecuteQuick(m_userSerial, m_commCode);
            upyRawReplExecuteQuick(m_userSerial, m_codeView->document()->text());
            m_devResetAction->setEnabled(true);
        });

        connect(m_devResetAction, &QAction::triggered, this, [this](bool) {
            m_consoleWidget->clear();
            upySoftReset(m_userSerial);
        });

        // add menu
        auto menuButton = new QToolButton(toolbar);
        menuButton->setIcon(QIcon::fromTheme("application-menu"));
        menuButton->setPopupMode(QToolButton::InstantPopup);
        auto actionsMenu = new QMenu(m_codeWindow);

        auto docHelpAction = actionsMenu->addAction("Open Module Documentation");
        connect(docHelpAction, &QAction::triggered, this, [](bool) {
            QDesktopServices::openUrl(
                QUrl("https://syntalos.readthedocs.io/latest/modules/upy-workbench.html", QUrl::TolerantMode));
        });
        auto docUPyHelpAction = actionsMenu->addAction("Open MicroPython Documentation");
        connect(docUPyHelpAction, &QAction::triggered, this, [](bool) {
            QDesktopServices::openUrl(QUrl("https://docs.micropython.org/en/latest/", QUrl::TolerantMode));
        });

        menuButton->setMenu(actionsMenu);
        toolbar->addWidget(menuButton);

        // Don't trigger the text editor document save dialog
        // TODO: Maybe we should save the Syntalos board here instead?
        auto actionCollection = m_codeView->actionCollection();
        if (actionCollection) {
            auto saveAction = actionCollection->action("file_save");
            if (saveAction) {
                // Remove default connections to disable default save behavior
                disconnect(saveAction, nullptr, nullptr, nullptr);
            }
        }

        // connect user serial connection events
        connect(m_userSerial, &QSerialPort::readyRead, this, [this]() {
            const auto data = m_userSerial->readAll();
            m_consoleWidget->putData(data);
        });

        connect(m_userSerial, &QSerialPort::bytesWritten, this, [this](qint64 bytes) {
            m_bytesToWrite -= bytes;
            if (m_bytesToWrite <= 0) {
                m_bytesToWrite = 0;
                m_timer->stop();
            }
        });

        connect(m_timer, &QTimer::timeout, this, [this]() {
            const QString error = tr("Write operation timed out for port %1.\n"
                                     "Error: %2")
                                      .arg(m_userSerial->portName(), m_userSerial->errorString());
            QMessageBox::warning(m_codeWindow, QStringLiteral("Write Timeout"), error);
        });
        m_timer->setSingleShot(true);

        connect(m_consoleWidget, &UPyConsole::newInput, this, [this](const QByteArray &data) {
            const auto written = m_userSerial->write(data);
            if (written >= data.size()) {
                m_bytesToWrite += written;
                m_timer->start(UPY_SERIAL_WRITE_TIMEOUT);
            } else {
                const QString error = tr("Failed to write all data to port %1.\n"
                                         "Error: %2")
                                          .arg(m_userSerial->portName(), m_userSerial->errorString());
                QMessageBox::warning(m_codeWindow, QStringLiteral("Write Error"), error);
            }
        });
    }

    ~UPyWBenchModule() override {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::THREAD_DEDICATED;
    }

    bool initialize() override
    {
        setInitialized();
        m_consoleWidget->setVisible(false);
        m_testRunAction->setEnabled(false);

        // receive data from our thread while we are running
        connect(
            this,
            &UPyWBenchModule::receivedUserData,
            this,
            [this](const QByteArray &data) {
                m_consoleWidget->putData(data);
            },
            Qt::QueuedConnection);

        return true;
    }

    void usbHotplugEvent(UsbHotplugEventKind) override
    {
        if (m_running)
            return;
        updateSerialPortsList();
    }

    bool connectUserInteractiveDevice()
    {
        const auto serialPortName = m_serialSelector->currentData().toString();
        if (serialPortName.isEmpty()) {
            QMessageBox::warning(
                m_codeWindow,
                QStringLiteral("Serial Port Error"),
                QStringLiteral("No serial port selected. Please select a serial port to connect to."));
            return false;
        }

        setSerialPortParameters(m_userSerial, serialPortName);
        if (m_userSerial->open(QIODevice::ReadWrite)) {
            QTimer::singleShot(0, this, [this]() {
                upySoftReset(m_userSerial);
                if (!upyRawReplExecuteQuick(m_userSerial, m_commCode)) {
                    QMessageBox::warning(
                        m_codeWindow,
                        QStringLiteral("Serial Port Error"),
                        QStringLiteral("Failed to send code to the device. Check the log output. Is MicroPython "
                                       "flashed to the device?"));
                    return;
                }
            });
        } else {
            QMessageBox::warning(
                m_codeWindow,
                QStringLiteral("Serial Port Error"),
                QStringLiteral("Failed to open serial port %1.\n"
                               "Error: %2")
                    .arg(m_userSerial->portName(), m_userSerial->errorString()));
            return false;
        }

        return true;
    }

    void selectSerialPort(const QString &port)
    {
        for (int i = 0; i < m_serialSelector->count(); i++) {
            if (m_serialSelector->itemData(i).toString() == port) {
                m_serialSelector->setCurrentIndex(i);
                break;
            }
        }
    }

    void updateSerialPortsList()
    {
        const auto selectedPort = m_serialSelector->currentData().toString();
        m_serialSelector->clear();

        // List all serial ports
        auto allPorts = QSerialPortInfo::availablePorts();
        for (auto &port : allPorts) {
            m_serialSelector->addItem(
                QString("%1 (%2)").arg(port.portName(), port.description()), port.systemLocation());
        }

        // select the right port again
        selectSerialPort(selectedPort);
    }

    static void setSerialPortParameters(QSerialPort *port, const QString &portName)
    {
        port->setPortName(portName);
        port->setBaudRate(QSerialPort::Baud115200);
        port->setDataBits(QSerialPort::Data8);
    }

    static void serialClearIncoming(QSerialPort *port)
    {
        // swallow all incoming data with a 10sec timeout
        for (uint i = 0; i < 100; i++) {
            port->readAll();
            if (!port->waitForReadyRead(100))
                break;
        }

        // clear incoming data
        port->clear(QSerialPort::Input);
    }

    static void upyInterrupt(QSerialPort *port)
    {
        // send a keyboard interrupt to the device
        port->write("\x03");
        port->flush();

        // exit raw REPL mode, in case we are in it
        port->write("\r");
        port->write("\x02");
        port->flush();
    }

    static void upySoftReset(QSerialPort *port)
    {
        // interrupt any running code
        upyInterrupt(port);

        // exit raw repl, just in case we are in one
        port->write("\r");
        port->write("\x02");
        port->flush();

        // perform a soft-reset on the device
        port->write("\r");
        port->write("\x04");
        port->flush();
    }

    static bool upyRawReplExecuteQuick(QSerialPort *port, const QString &code)
    {
        // switch to raw REPL & send code
        upyRawReplSendCode(port, code);

        return upyRawReplExecute(port);
    }

    static void upyRawReplSendCode(QSerialPort *port, const QString &code)
    {
        // switch to raw REPL mode
        port->write("\r");
        port->write("\x01");
        port->flush();

        // send code
        port->write(code.toUtf8());
    }

    static bool upyRawReplExecute(QSerialPort *port)
    {
        // don't forward any incoming data and clear the internal buffer
        port->blockSignals(true);
        serialClearIncoming(port);

        // execute code
        port->write("\r");
        port->write("\x04");
        port->flush();

        port->waitForReadyRead(20000);
        if (port->read(2) != "OK") {
            port->blockSignals(false);
            return false;
        }

        // enable incoming data forwarding (& all other signals)
        port->blockSignals(false);

        return true;
    }

    void setName(const QString &value) final
    {
        AbstractModule::setName(value);
        m_codeWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));
    }

    bool prepare(const TestSubject &testSubject) override
    {
        m_portEditAction->setEnabled(false);
        m_serialSelector->setEnabled(false);
        m_devConnectAction->setEnabled(false);
        m_testRunAction->setEnabled(false);
        m_devResetAction->setEnabled(false);

        // close the serial connection that the user is using interactively
        m_devConnectAction->setChecked(false);
        if (m_userSerial->isOpen())
            m_userSerial->close();

        // start all streams
        for (auto &p : outPorts()) {
            if (p->dataTypeId() == BaseDataType::IntSignalBlock || p->dataTypeId() == BaseDataType::FloatSignalBlock) {
                auto stream = p->streamVar();
                stream->setMetadataValue("signal_names", QStringList() << "Data");
                stream->setMetadataValue("time_unit", "milliseconds");
            }

            p->startStream();
        }

        for (auto &p : inPorts()) {
            if (p->dataTypeId() != BaseDataType::TableRow)
                continue;
            auto trp = std::static_pointer_cast<StreamInputPort<TableRow>>(p);
            if (trp->hasSubscription())
                m_activeInPorts.push_back(trp);
        }

        // set up clock synchronizer
        m_clockSync = initClockSynchronizer();
        m_clockSync->setCalibrationPointsCount(30);
        m_clockSync->setTolerance(milliseconds_t(2));
        m_clockSync->setStrategies(TimeSyncStrategy::SHIFT_TIMESTAMPS_FWD | TimeSyncStrategy::SHIFT_TIMESTAMPS_BWD);
        m_baseTimeOffset = milliseconds_t(0);

        // start the synchronizer
        if (!m_clockSync->start()) {
            raiseError(QStringLiteral("Unable to set up clock synchronizer!"));
            return false;
        }

        m_consoleWidget->setVisible(true);
        m_consoleWidget->clear();
        return true;
    }

    void processIncomingPortData(
        const QJsonObject &obj,
        const QHash<int, std::shared_ptr<VariantDataStream>> &streamMap,
        microseconds_t &recvMasterTime)
    {
        // ignore empty requests
        if (obj.isEmpty())
            return;

        // ignore any host commands that were echoed back
        if (obj.contains("hc"))
            return;

        std::shared_ptr<VariantDataStream> stream;
        auto portId = obj["p"].toInt(-1);
        stream = streamMap.value(portId);

        if (!stream) {
            raiseError(QStringLiteral("Unable to find port with ID %1, as requested by the device. Was the port "
                                      "properly registered with the host?")
                           .arg(portId));
            return;
        }

        if (stream->dataTypeId() == BaseDataType::TableRow) {
            TableRow row;
            for (const auto &e : obj["d"].toArray()) {
                if (e.isDouble())
                    row.append(QString::number(e.toDouble()));
                else
                    row.append(e.toString());
            }

            std::static_pointer_cast<DataStream<TableRow>>(stream)->push(row);
            return;
        }

        bool isIntBlock = stream->dataTypeId() == BaseDataType::IntSignalBlock;
        bool isFloatBlock = !isIntBlock && stream->dataTypeId() == BaseDataType::FloatSignalBlock;

        if (isIntBlock || isFloatBlock) {
            const auto array = obj["d"].toArray();
            const auto arrayLen = array.size();
            const auto deviceTimestamp = microseconds_t((obj["t"].toInt() - m_baseTimeOffset.count()) * 1000);

            // synchronize
            m_clockSync->processTimestamp(recvMasterTime, deviceTimestamp);
            const auto syncTimestampMsec = static_cast<quint32>(recvMasterTime.count() / 1000);

            if (isIntBlock) {
                IntSignalBlock block(arrayLen);
                for (int i = 0; i < arrayLen; i++) {
                    block.data(i, 0) = array[i].toInt();
                    block.timestamps(i, 0) = syncTimestampMsec;
                }

                std::static_pointer_cast<DataStream<IntSignalBlock>>(stream)->push(block);
            } else {
                FloatSignalBlock block(arrayLen);
                for (int i = 0; i < arrayLen; i++) {
                    block.data(i, 0) = array[i].toDouble();
                    block.timestamps(i, 0) = syncTimestampMsec;
                }

                std::static_pointer_cast<DataStream<FloatSignalBlock>>(stream)->push(block);
            }
        }
    }

    static void forwardInPortData(
        QSerialPort *port,
        const std::vector<std::shared_ptr<StreamSubscription<TableRow>>> &activeSubs)
    {
        for (size_t i = 0; i < activeSubs.size(); i++) {
            if (!activeSubs[i]->hasPending())
                continue;
            auto maybeRow = activeSubs[i]->peekNext();
            if (!maybeRow.has_value())
                continue;
            auto row = maybeRow.value();
            QJsonObject obj;
            obj.insert("p", (qint64)i);
            QJsonArray data;
            for (const auto &e : row.data)
                data.append(e);
            obj.insert("d", data);
            port->write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            port->write("\n");
            port->flush();
        }
    }

    void runThread(OptionalWaitCondition *waitCondition) final
    {
        // thread-only serial connection
        QSerialPort serialPort;

        const auto serialDevice = m_serialSelector->currentData().toString();
        if (serialDevice.isEmpty()) {
            raiseError("No serial port selected. Can not connect to the device!");
            return;
        }

        setSerialPortParameters(&serialPort, serialDevice);
        if (!serialPort.open(QIODevice::ReadWrite)) {
            raiseError(QStringLiteral("Failed to open serial port %1.\n"
                                      "Error: %2")
                           .arg(serialPort.portName(), serialPort.errorString()));
            return;
        }

        // reset the device to ensure we have a clean slate
        upySoftReset(&serialPort);

        // ignore any data the reset operation may have generated
        serialClearIncoming(&serialPort);

        // inject Syntalos communication code
        if (!upyRawReplExecuteQuick(&serialPort, m_commCode)) {
            raiseError(
                "Failed to send code to the device. Check the log output. Is MicroPython flashed to the device?");
            return;
        }

        // prepare subscription list
        std::vector<std::shared_ptr<StreamSubscription<TableRow>>> activeSubs;
        for (auto &p : m_activeInPorts)
            activeSubs.push_back(p->subscription());

        // send the user's code to the device
        upyRawReplSendCode(&serialPort, m_codeView->document()->text());

        // we are ready!
        m_stopped = false;
        waitCondition->wait(this);

        // execute the previously transmitted code
        upyRawReplExecute(&serialPort);

        bool isConfigDone = false;
        bool isPortInfoSent = false;
        QHash<int, std::shared_ptr<VariantDataStream>> streamMap;

        while (m_running) {
            if (!serialPort.waitForReadyRead(25)) {
                forwardInPortData(&serialPort, activeSubs);
                continue;
            }
            forwardInPortData(&serialPort, activeSubs);
            if (!serialPort.canReadLine())
                continue;

            auto recvMasterTime = m_syTimer->timeSinceStartUsec();
            const auto data = serialPort.readLine();

            // check for error
            if (data.contains("Traceback (most recent call last)")) {
                // an error occurred, print everything to the console
                emit receivedUserData(data);

                for (uint i = 0; i < 20; i++) {
                    if (!serialPort.waitForReadyRead(100))
                        continue;
                    emit receivedUserData(serialPort.readAll());
                }

                raiseError("The device script failed with an error. Check the device console for details.");
                break;
            }

            // check for regular output
            if (!data.startsWith('{')) {
                // output anything this isn't a JSON request to the console
                emit receivedUserData(data);
                continue;
            }

            // we received a JSON object
            const auto jo = QJsonDocument::fromJson(data);

            if (jo.isNull() || !jo.isObject()) {
                emit receivedUserData(data);
            } else {
                if (isConfigDone) {
                    processIncomingPortData(jo.object(), streamMap, recvMasterTime);
                } else {
                    if (!isPortInfoSent) {
                        // ensure the input line-reading pipeline is clear
                        serialPort.write("\n");
                        serialPort.flush();

                        // notify the device about input ports
                        for (size_t i = 0; i < m_activeInPorts.size(); i++) {
                            QJsonObject obj;
                            obj.insert("hc", "in-port");
                            obj.insert("i", (qint64)i);
                            obj.insert("p", m_activeInPorts[i]->id());
                            serialPort.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
                            serialPort.write("\n");
                            serialPort.flush();
                        }
                        isPortInfoSent = true;
                    }

                    // receive information about output ports
                    auto jd = jo.object();
                    if (!jd.contains("dc") && jd.contains("d")) {
                        // we are receiving data now, exist config mode
                        isConfigDone = true;
                        processIncomingPortData(jd, streamMap, recvMasterTime);
                        continue;
                    }

                    const auto command = jd["dc"].toString();
                    if (command == "new-out-port") {
                        const auto portId = jd["n"].toString();
                        auto oport = outPortById(portId);
                        if (!oport) {
                            raiseError(QStringLiteral("Device requested output port of ID '%1', but no such port has "
                                                      "been registered on the host!")
                                           .arg(portId));
                            break;
                        }
                        streamMap[jd["i"].toInt()] = oport->streamVar();
                    } else if (command == "start-time") {
                        m_baseTimeOffset = milliseconds_t(jd["t_ms"].toInt());
                    }
                }
            }
        }

        // stop device program and clean up
        upyInterrupt(&serialPort);
        m_stopped = true;
    }

    void stop() override
    {
        AbstractModule::stop();

        m_running = false;
        while (!m_stopped) {
            appProcessEvents();
        }
        safeStopSynchronizer(m_clockSync);

        m_portEditAction->setEnabled(true);
        m_serialSelector->setEnabled(true);
        m_devConnectAction->setEnabled(true);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &extraData) override
    {
        extraData = m_codeView->document()->text().toUtf8();

        QVariantList varInPorts;
        for (const auto &port : inPorts()) {
            QVariantHash po;
            po.insert("id", port->id());
            po.insert("title", port->title());
            po.insert("data_type", port->dataTypeName());
            varInPorts.append(po);
        }

        QVariantList varOutPorts;
        for (const auto &port : outPorts()) {
            QVariantHash po;
            po.insert("id", port->id());
            po.insert("title", port->title());
            po.insert("data_type", port->dataTypeName());
            varOutPorts.append(po);
        }

        settings.insert("ports_in", varInPorts);
        settings.insert("ports_out", varOutPorts);

        settings.insert("serial_port", m_serialSelector->currentData().toString());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &extraData) override
    {
        m_codeView->document()->setText(QString::fromUtf8(extraData));

        const auto varInPorts = settings.value("ports_in").toList();
        const auto varOutPorts = settings.value("ports_out").toList();

        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            registerInputPortByTypeId(
                BaseDataType::typeIdFromString(qPrintable(po.value("data_type").toString())),
                po.value("id").toString(),
                po.value("title").toString());
        }

        for (const auto &pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(
                BaseDataType::typeIdFromString(qPrintable(po.value("data_type").toString())),
                po.value("id").toString(),
                po.value("title").toString());
        }

        // update port listing in UI
        m_portsDialog->updatePortLists();

        // re-select the right serial device
        selectSerialPort(settings.value("serial_port").toString());

        return true;
    }

private:
    UPyConsole *m_consoleWidget;
    KTextEditor::View *m_codeView;
    PortEditorDialog *m_portsDialog;
    QWidget *m_codeWindow;
    QAction *m_portEditAction;
    QAction *m_testRunAction;
    QComboBox *m_serialSelector;
    QAction *m_devConnectAction;
    QAction *m_devResetAction;

    QTimer *m_timer = nullptr;
    QSerialPort *m_userSerial;
    qint64 m_bytesToWrite = 0;
    QString m_commCode;

    std::atomic_bool m_stopped;
    std::vector<std::shared_ptr<StreamInputPort<TableRow>>> m_activeInPorts;
    milliseconds_t m_baseTimeOffset = milliseconds_t(0);
    std::unique_ptr<SecondaryClockSynchronizer> m_clockSync;
};

QString UPyWBenchModuleInfo::id() const
{
    return QStringLiteral("upy-workbench");
}

QString UPyWBenchModuleInfo::name() const
{
    return QStringLiteral("MicroPython Workbench");
}

QString UPyWBenchModuleInfo::description() const
{
    return QStringLiteral("Program microcontrollers live in Python.");
}

ModuleCategories UPyWBenchModuleInfo::categories() const
{
    return ModuleCategory::SCRIPTING | ModuleCategory::DEVICES;
}

AbstractModule *UPyWBenchModuleInfo::createModule(QObject *parent)
{
    return new UPyWBenchModule(parent);
}

#include "upywbenchmodule.moc"
