/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QGroupBox>
#include <QComboBox>
#include <QToolButton>
#include <QLineEdit>
#include <QDateTime>
#include <QErrorMessage>
#include <QRadioButton>
#include <QDebug>
#include <QThread>
#include <QSerialPortInfo>
#include <QScriptEngine>
#include <QTableWidget>
#include <QMdiSubWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSpinBox>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QDoubleSpinBox>
#include <memory>
#include <QSettings>

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <KTar>

#include "ma-private.h"

#include "intanrec/intanui.h"
#include "intanrec/waveplot.h"

#include "video/videotracker.h"
#include "video/videoviewwidget.h"

#include "mazescript.h"
#include "statuswidget.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Create status bar
    statusBarLabel = new QLabel(tr(""));
    statusBar()->addWidget(statusBarLabel, 1);
    statusBar()->setSizeGripEnabled(false);  // fixed window size

    // status widget
    m_statusWidget = new StatusWidget(this);
    ui->mdiArea->addSubWindow(m_statusWidget)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    // set up Intan GUI and board
    m_intanUI = new IntanUI(this);

    auto intanLayout = new QVBoxLayout();
    intanLayout->addWidget(m_intanUI);
    ui->tabIntan->setLayout(intanLayout);

    ui->mdiArea->addSubWindow(m_intanUI->displayWidget())->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    // add Intan menu actions
    ui->menuIntan->addSeparator();
    ui->menuIntan->addAction(m_intanUI->renameChannelAction);
    ui->menuIntan->addAction(m_intanUI->toggleChannelEnableAction);
    ui->menuIntan->addAction(m_intanUI->enableAllChannelsAction);
    ui->menuIntan->addAction(m_intanUI->disableAllChannelsAction);
    ui->menuIntan->addSeparator();
    ui->menuIntan->addAction(m_intanUI->originalOrderAction);
    ui->menuIntan->addAction(m_intanUI->alphaOrderAction);

    // setup general page
    auto openDirBtn = new QToolButton();
    openDirBtn->setIcon(QIcon::fromTheme("folder-open"));

    auto dirInfoLabel = new QLabel("Export &Directory:");
    dirInfoLabel->setBuddy(openDirBtn);

    exportDirLabel = new QLabel("???");
    exportDirInfoLabel = new QLabel ("Recorded data will be stored in: The directory you select.");

    ui->dataExportDirLayout->addWidget(dirInfoLabel);
    ui->dataExportDirLayout->addWidget(exportDirLabel);
    ui->dataExportDirLayout->addWidget(openDirBtn);
    ui->dataExportLayout->addWidget(exportDirInfoLabel);

    connect(openDirBtn, &QToolButton::clicked, this, &MainWindow::openDataExportDirectory);

    connect(ui->subjectIdEdit, &QLineEdit::textChanged, [=](const QString& mouseId) {
        if (mouseId.isEmpty()) {
            ui->subjectSelectComboBox->setEnabled(true);
            return;
        }
        TestSubject sub;
        sub.id = mouseId;
        changeTestSubject(sub);

        // we shouldn't use both the subject selector and manual data entry
        ui->subjectSelectComboBox->setEnabled(false);
    });
    connect(ui->expIdEdit, &QLineEdit::textChanged, this, &MainWindow::changeExperimentId);

    connect(ui->subjectSelectComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int index) {
        // empty manual edit to not interfere with the subject selector
        ui->subjectIdEdit->setText(QString());

        auto sub = m_subjectList->subject(index);
        changeTestSubject(sub);
    });

    // set up test subjects page
    m_subjectList = new TestSubjectListModel(this);
    ui->subjectListView->setModel(m_subjectList);

    connect(ui->btnSubjectRemove, &QToolButton::clicked, [&]() {
        m_subjectList->removeRow(ui->subjectListView->currentIndex().row());
    });

    connect(ui->btnSubjectAdd, &QToolButton::clicked, [&]() {
        TestSubject sub;
        sub.id = ui->idLineEdit->text();
        if (sub.id.isEmpty()) {
            QMessageBox::warning(this, "Could not add test subject", "Can not add test subject with an empty ID!");
            return;
        }

        sub.group = ui->groupLineEdit->text();
        sub.adaptorHeight = ui->adaptorHeightSpinBox->value();
        sub.active = ui->subjectActiveCheckBox->isChecked();
        sub.comment = ui->remarksTextEdit->toPlainText();
        m_subjectList->addSubject(sub);
    });

    connect(ui->subjectListView, &QListView::activated, [&](const QModelIndex &index) {
        auto sub = m_subjectList->subject(index.row());

        ui->idLineEdit->setText(sub.id);
        ui->groupLineEdit->setText(sub.group);
        ui->subjectActiveCheckBox->setChecked(sub.active);
        ui->remarksTextEdit->setPlainText(sub.comment);

        ui->btnSubjectRemove->setEnabled(true);
        ui->btnSubjectApplyEdit->setEnabled(true);
    });

    connect(ui->btnSubjectApplyEdit, &QToolButton::clicked, [&]() {
        auto index = ui->subjectListView->currentIndex();
        if (!index.isValid()) {
            QMessageBox::warning(this, "Could not change test subject", "No subject selected to apply changes to.");
            return;
        }

        auto row = index.row();
        auto sub = m_subjectList->subject(row);
        auto id = ui->idLineEdit->text();
        if (id.isEmpty()) {
            QMessageBox::warning(this, "Could not change test subject", "Can not change test subject with an empty ID!");
            return;
        }
        sub.id = id;

        sub.group = ui->groupLineEdit->text();
        sub.adaptorHeight = ui->adaptorHeightSpinBox->value();
        sub.active = ui->subjectActiveCheckBox->isChecked();
        sub.comment = ui->remarksTextEdit->toPlainText();

        m_subjectList->removeRow(row);
        m_subjectList->insertSubject(row, sub);
        ui->subjectListView->setCurrentIndex(m_subjectList->index(row));
    });

    ui->subjectSelectComboBox->setModel(m_subjectList);

    // Arduino / Firmata I/O
    auto allPorts = QSerialPortInfo::availablePorts();
    foreach(auto port, allPorts) {
        ui->portsComboBox->addItem(QString("%1 (%2)").arg(port.portName()).arg(port.description()), QVariant::fromValue(port.systemLocation()));
    }
    if (allPorts.count() <= 0)
        m_statusWidget->setFirmataStatus(StatusWidget::Missing);

    m_msintf = new MazeScript;
    connect(m_msintf, &MazeScript::firmataError, this, &MainWindow::firmataError);
    connect(m_msintf, &MazeScript::evalError, this, &MainWindow::scriptEvalError);
    connect(m_msintf, &MazeScript::headersSet, this, &MainWindow::onEventHeadersSet);

    m_mazeEventTable = new QTableWidget(this);
    m_mazeEventTable->setWindowTitle("Maze Events");
    m_mazeEventTable->setWindowFlags(m_mazeEventTable->windowFlags() & ~Qt::WindowCloseButtonHint);
    m_mazeEventTable->horizontalHeader()->hide();
    connect(m_msintf, &MazeScript::mazeEvent, this, &MainWindow::onMazeEvent);
    ui->mdiArea->addSubWindow(m_mazeEventTable)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    // set up code editor
    auto editor = KTextEditor::Editor::instance();
    // create a new document
    auto jsDoc = editor->createDocument(this);
    jsDoc->setText(m_msintf->script());
    m_mazeJSView = jsDoc->createView(this);
    ui->mazeJSLayout->addWidget(m_mazeJSView);
    jsDoc->setHighlightingMode("javascript");

    // set up video and tracking
    m_videoTracker = new VideoTracker;
    connect(m_videoTracker, &VideoTracker::error, this, &MainWindow::videoError);
    m_rawVideoWidget = new VideoViewWidget(this);
    ui->mdiArea->addSubWindow(m_rawVideoWidget)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    m_rawVideoWidget->setWindowTitle("Raw Video");
    connect(m_videoTracker, &VideoTracker::newFrame, [&](time_t time, const cv::Mat& image) {
        m_rawVideoWidget->setWindowTitle(QString("Raw Video (at %1sec)").arg(time / 1000));
        m_rawVideoWidget->showImage(image);
    });

    m_trackVideoWidget = new VideoViewWidget(this);
    ui->mdiArea->addSubWindow(m_trackVideoWidget)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    m_trackVideoWidget->setWindowTitle("Tracking");
    connect(m_videoTracker, &VideoTracker::newTrackingFrame, [&](time_t time, const cv::Mat& image) {
        m_trackVideoWidget->setWindowTitle(QString("Tracking (at %1sec)").arg(time / 1000));
        m_trackVideoWidget->showImage(image);
    });

    m_trackInfoWidget = new VideoViewWidget(this);
    ui->mdiArea->addSubWindow(m_trackInfoWidget)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    m_trackInfoWidget->setWindowTitle("Subject Tracking");
    connect(m_videoTracker, &VideoTracker::newInfoGraphic, [&](const cv::Mat& image) {
        m_trackInfoWidget->showImage(image);
    });

    // video settings panel
    auto cameraBox = new QComboBox(this);
    auto resolutionsBox = new QComboBox(this);
    ui->cameraLayout->addRow(new QLabel("Camera", this), cameraBox);
    ui->cameraLayout->addRow(new QLabel("Resolution", this), resolutionsBox);

    connect(cameraBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [=](int index) {
        auto cameraId = cameraBox->itemData(index).toInt();
        m_videoTracker->setCameraId(cameraId);

        auto resList = m_videoTracker->resolutionList(cameraId);
        foreach (auto size, resList)
            resolutionsBox->addItem(QString("%1x%2").arg(size.width()).arg(size.height()), size);

        // set the first element (usually the highest resolution) as our default resolution
        m_videoTracker->setResolution(resList.front());

        // FIXME: the camera can only work with its highest resolution at time, since binning does not work for some reason
        resolutionsBox->setEnabled(false);
    });

    foreach (auto pair, m_videoTracker->getCameraList())
        cameraBox->addItem(pair.first, pair.second);

    m_fpsEdit = new QSpinBox(this);
    m_fpsEdit->setMinimum(10);
    m_fpsEdit->setMaximum(200);
    m_fpsEdit->setValue(m_videoTracker->framerate());
    connect(m_fpsEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        m_videoTracker->setFramerate(value);
    });
    ui->cameraLayout->addRow(new QLabel("Framerate (FPS)", this), m_fpsEdit);

    auto exportResWidget = new QWidget(this);
    auto exportResLayout = new QHBoxLayout(this);
    exportResWidget->setLayout(exportResLayout);
    ui->cameraLayout->addRow(new QLabel("Resolution of exported images", this), exportResWidget);

    m_eresWidthEdit = new QSpinBox(this);
    m_eresHeightEdit = new QSpinBox(this);
    m_eresWidthEdit->setMinimum(640);
    m_eresHeightEdit->setMinimum(480);
    m_eresWidthEdit->setMaximum(1920);
    m_eresHeightEdit->setMaximum(1080);

    auto imgExportSize = m_videoTracker->exportResolution();
    m_eresWidthEdit->setValue(imgExportSize.width());
    m_eresHeightEdit->setValue(imgExportSize.height());

    exportResLayout->setMargin(0);
    exportResLayout->addWidget(m_eresWidthEdit);
    exportResLayout->addWidget(new QLabel("x", this));
    exportResLayout->addWidget(m_eresHeightEdit);

    connect(m_eresWidthEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        m_videoTracker->setExportResolution(QSize(value, m_eresHeightEdit->value()));
    });
    connect(m_eresHeightEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [=](int value) {
        m_videoTracker->setExportResolution(QSize(m_eresWidthEdit->value(), value));
    });

    m_gainCB = new QCheckBox(this);
    m_gainCB->setChecked(false);
    connect(m_gainCB, &QCheckBox::toggled, [=](bool value) {
        m_videoTracker->setAutoGain(value);
    });
    ui->cameraLayout->addRow(new QLabel("Automatic gain", this), m_gainCB);

    m_exposureEdit = new QDoubleSpinBox(this);
    m_exposureEdit->setValue(6);
    connect(m_exposureEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [=](double value) {
        m_videoTracker->setExposureTime(value);
    });
    ui->cameraLayout->addRow(new QLabel("Exposure time (msec)", this), m_exposureEdit);

    if (m_videoTracker->cameraId() < 0)
        m_statusWidget->setVideoStatus(StatusWidget::Missing);
    else
        m_statusWidget->setVideoStatus(StatusWidget::Ready);

    auto ueyeConfFileWidget = new QWidget(this);
    auto ueyeConfFileLayout = new QHBoxLayout;
    ueyeConfFileWidget->setLayout(ueyeConfFileLayout);
    ueyeConfFileLayout->setMargin(0);
    ui->cameraLayout->addRow(new QLabel("uEye Configuration File", this), ueyeConfFileWidget);

    m_ueyeConfFileLbl = new QLabel(this);
    ueyeConfFileLayout->addWidget(m_ueyeConfFileLbl);
    auto ueyeConfFileBtn = new QToolButton(this);
    ueyeConfFileLayout->addWidget(ueyeConfFileBtn);
    ueyeConfFileBtn->setIcon(QIcon::fromTheme("folder-open"));
    m_ueyeConfFileLbl->setText("No file selected.");

    connect(ueyeConfFileBtn, &QToolButton::clicked, [=]() {
        auto fileName = QFileDialog::getOpenFileName(this,
                                                     tr("Select uEye Settings"), ".",
                                                     tr("uEye Settings (*.ini)"));
        if (fileName.isEmpty())
            return;
        m_ueyeConfFileLbl->setText(fileName);
        m_videoTracker->setUEyeConfigFile(fileName);
    });

    m_saveTarCB = new QCheckBox(this);
    m_saveTarCB->setChecked(true);
    ui->cameraLayout->addRow(new QLabel("Store frames in compressed tarball", this), m_saveTarCB);

    // configure actions
    ui->actionRun->setEnabled(false);
    ui->actionStop->setEnabled(false);

    // connect actions
    connect(ui->actionIntanRun, &QAction::triggered, this, &MainWindow::intanRunActionTriggered);
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runActionTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::stopActionTriggered);
    connect(ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsActionTriggered);
    connect(ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsActionTriggered);

    // connect triggers
    connect(m_intanUI, &IntanUI::recordingReady, [&]() {
        qDebug() << "Intan is ready";
        if (m_lastReady == VideoReady) {
            m_intanUI->unblockRecording();
            m_videoTracker->triggerRecording();
            qDebug() << "Intan triggered start.";
        }
        m_lastReady = IntanReady;
    });

    connect(m_videoTracker, &VideoTracker::recordingReady, [&]() {
        qDebug() << "Video and tracking is ready";
        if (m_lastReady == IntanReady) {
            m_intanUI->unblockRecording();
            m_videoTracker->triggerRecording();
            qDebug() << "Video triggered start.";
        }

        m_lastReady = VideoReady;
    });

    // various
    ui->tabWidget->setCurrentIndex(0);
    m_exportDirValid = false;

    // set date ID string
    auto time = QDateTime::currentDateTime();
    m_currentDate = time.date().toString("yyyy-MM-dd");

    // assume intan is ready (with real or fake data)
    m_statusWidget->setIntanStatus(StatusWidget::Ready);

    // configure about dialog
    m_aboutDialog = new QDialog(this);
    auto aboutLayout = new QVBoxLayout;
    m_aboutDialog->setLayout(aboutLayout);

    auto imgLabel = new QLabel(this);
    imgLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    aboutLayout->addWidget(imgLabel);
    imgLabel->setText(aboutDlgAsciiArt);

    auto aboutLabel = new QLabel(this);
    aboutLabel->setAlignment(Qt::AlignCenter);
    aboutLabel->setText(aboutDlgCopyInfo);
    aboutLayout->addWidget(aboutLabel);

    auto versionLabel = new QLabel(this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setText(versionInfoText.arg(QCoreApplication::applicationVersion()));
    aboutLayout->addWidget(versionLabel);

    auto aboutQuitButton = new QPushButton(this);
    aboutQuitButton->setText("OK");
    aboutLayout->addWidget(aboutQuitButton);

    connect(aboutQuitButton, &QPushButton::clicked, m_aboutDialog, &QDialog::accept);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::aboutActionTriggered);

    // lastly, restore our geometry and widget state
    QSettings settings("DraguhnLab", "MazeAmaze");
    restoreGeometry(settings.value("main/geometry").toByteArray());
}

MainWindow::~MainWindow()
{
    delete ui;
    delete m_msintf;
    delete m_videoTracker;
}

void MainWindow::onMazeEvent(const QStringList &data)
{
    auto columnCount = m_mazeEventTable->columnCount();
    if (columnCount < data.count()) {
        // create necessary amount of columns
        if (columnCount == 0) {
            m_mazeEventTable->setColumnCount(data.count());
        } else {
            for (auto i = columnCount; i < data.count(); i++)
                m_mazeEventTable->insertColumn(i);
        }
    }

    auto lastRowId = m_mazeEventTable->rowCount();
    m_mazeEventTable->setRowCount(lastRowId + 1);

    qDebug() << "Received event:" << data;
    for (auto i = 0; i < data.count(); i++) {
        auto item = new QTableWidgetItem(data.at(i));
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        m_mazeEventTable->setItem(lastRowId, i, item);
    }

    // scroll to the last item
    m_mazeEventTable->scrollToBottom();
}

void MainWindow::onEventHeadersSet(const QStringList &headers)
{
    m_mazeEventTable->horizontalHeader()->show();
    m_mazeEventTable->setColumnCount(headers.count());
    m_mazeEventTable->setHorizontalHeaderLabels(headers);
}

void MainWindow::setRunPossible(bool enabled)
{
    ui->actionRun->setEnabled(enabled);
    ui->actionIntanRun->setEnabled(enabled);
}

void MainWindow::setStopPossible(bool enabled)
{
    ui->actionStop->setEnabled(enabled);
}

void MainWindow::firmataError(const QString &message)
{
    m_failed = true;
    QMessageBox::critical(this, "Serial Interface Error", message);
    stopActionTriggered();
    ui->portsComboBox->setEnabled(true);
    m_statusWidget->setFirmataStatus(StatusWidget::Broken);
    setStatusText("Firmata error.");
}

void MainWindow::videoError(const QString &message)
{
    m_failed = true;
    QMessageBox::critical(this, "Video Error", message);
    stopActionTriggered();
    m_statusWidget->setVideoStatus(StatusWidget::Broken);
    setStatusText("Video error.");
}

void MainWindow::scriptEvalError(int line, const QString& message)
{
    m_failed = true;
    QMessageBox::critical(this, "Maze Script Error",
                          QString("Uncaught exception at line %1: %2")
                                    .arg(line)
                                    .arg(message));
    stopActionTriggered();
    setStatusText("Script error.");
}

bool MainWindow::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        QMessageBox::critical(this, "Error",
                              QString("Unable to create directory '%1'.").arg(dir));
        return false;
        setStatusText("OS error.");
    }

    return true;
}

void MainWindow::runActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);
    m_failed = false;
    m_lastReady = UnknownReady;

    // safeguard against accidental data removals
    QDir deDir(m_dataExportDir);
    if (deDir.exists()) {
        auto reply = QMessageBox::question(this,
                                           "Really continue?",
                                           QString("The directory %1 already contains data (likely from a previous run). If you continue, the old data will be deleted. Continue and delete data?")
                                               .arg(m_dataExportDir),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            setRunPossible(true);
            setStopPossible(false);
            return;
        }
        setStatusText("Removing data from an old run...");
        deDir.removeRecursively();
    }

    // determine and create the directory for ephys data
    qDebug() << "Initializing";
    auto intanDataDir = QString::fromUtf8("%1/intan").arg(m_dataExportDir);
    if (!makeDirectory(intanDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    auto mazeEventDataDir = QString::fromUtf8("%1/maze").arg(m_dataExportDir);
    if (!makeDirectory(mazeEventDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    auto videoDataDir = QString::fromUtf8("%1/video").arg(m_dataExportDir);
    if (!makeDirectory(videoDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    // write manifest with misc information
    QDateTime curDateTime(QDateTime::currentDateTime());

    QJsonObject manifest;
    manifest.insert("maVersion", QApplication::applicationVersion());
    manifest.insert("subjectId", m_currentSubject.id);
    manifest.insert("subjectGroup", m_currentSubject.group);
    manifest.insert("subjectComment", m_currentSubject.comment);
    manifest.insert("frameTarball", m_saveTarCB->isChecked());
    manifest.insert("timestamp", curDateTime.toString(Qt::ISODate));

    QFile manifestFile(QStringLiteral("%1/manifest.json").arg(m_dataExportDir));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Unable to start recording", "Unable to open manifest file for writing.");
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    QTextStream manifestFileOut(&manifestFile);
    manifestFileOut << QJsonDocument(manifest).toJson();

    // set base locations
    QString intanBaseName;
    if (m_currentSubject.id.isEmpty()) {
        intanBaseName = QString::fromUtf8("%1/ephys").arg(intanDataDir);
        m_msintf->setEventFile(QString("%1/events.csv").arg(mazeEventDataDir));
        m_videoTracker->setMouseId("frame");
    } else {
        intanBaseName = QString::fromUtf8("%1/%2_ephys").arg(intanDataDir).arg(m_currentSubject.id);
        m_msintf->setEventFile(QString("%1/%2_events.csv").arg(mazeEventDataDir).arg(m_currentSubject.id));
        m_videoTracker->setMouseId(m_currentSubject.id);
    }
    m_videoTracker->setDataLocation(videoDataDir);
    m_intanUI->setBaseFileName(intanBaseName);

    // open camera (might take a while, so we do this early)
    setStatusText("Opening connection to camera...");
    if (!m_videoTracker->openCamera())
        return;

    // open Firmata connection via the selected serial interface.
    // after we opened the device, we can't change it anymore (or rather, were too lazy to implement this...)
    // so we disable the selection box.
    auto serialDevice = ui->portsComboBox->currentData().toString();
    if (serialDevice.isEmpty()) {
        auto reply = QMessageBox::question(this,
                                           "Really continue?",
                                           "No Firmata device was found for programmable data I/O. Do you really want to continue without this functionality?",
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            setRunPossible(true);
            setStopPossible(false);
            return;
        }
        m_statusWidget->setFirmataStatus(StatusWidget::Broken);
    } else {
        m_msintf->initFirmata(serialDevice);
        ui->portsComboBox->setEnabled(false);
        if (m_failed)
            return;

        // clear previous events
        m_mazeEventTable->clear();
        m_mazeEventTable->setRowCount(0);

        // configure & launch maze script
        setStatusText("Evaluating maze script...");
        m_msintf->setScript(m_mazeJSView->document()->text());
        m_msintf->run();
        if (m_failed)
            return;

        m_statusWidget->setFirmataStatus(StatusWidget::Active);
    }

    // launch video
    auto vThread = new QThread;
    m_videoTracker->moveToThread(vThread);
    connect(vThread, &QThread::started, m_videoTracker, &VideoTracker::run);
    connect(m_videoTracker, &VideoTracker::finished, vThread, &QThread::quit);
    connect(vThread, &QThread::finished, vThread, &QObject::deleteLater);

    setStatusText("Starting video...");
    vThread->start();
    if (m_failed)
        return;
    m_statusWidget->setVideoStatus(StatusWidget::Active);

    // disable UI elements
    m_mazeJSView->setEnabled(false);
    ui->cameraGroupBox->setEnabled(false);

    // launch intan recordings
    qDebug() << "Starting Intan recording";
    setStatusText("Running.");
    m_running = true;
    m_statusWidget->setIntanStatus(StatusWidget::Active);
    m_intanUI->recordInterfaceBoard();
}

void MainWindow::stopActionTriggered()
{
    setRunPossible(m_exportDirValid);
    setStopPossible(false);

    // stop Maze script
    m_msintf->stop();
    m_statusWidget->setFirmataStatus(StatusWidget::Ready);

    // stop video tracker
    m_videoTracker->stop();
    m_videoTracker->closeCamera();

    // stop interface board
    m_intanUI->stopInterfaceBoard();
    m_statusWidget->setIntanStatus(StatusWidget::Ready);
    
    // compress frame tarball, if selected
    if (m_saveTarCB->isChecked()) {
        QProgressDialog dialog(this);

        dialog.setCancelButton(nullptr);
        dialog.setLabelText("Packing and compressing frames...");
        dialog.setWindowModality(Qt::WindowModal);
        dialog.show();

        std::unique_ptr<QMetaObject::Connection> pconn {new QMetaObject::Connection};
        QMetaObject::Connection &conn = *pconn;
        conn = QObject::connect(m_videoTracker, &VideoTracker::progress, [&](int max, int value) {
            dialog.setMaximum(max);
            dialog.setValue(value);
            QApplication::processEvents();
        });

        if (!m_videoTracker->makeFrameTarball())
            QMessageBox::critical(this, "Error writing frame tarball", m_videoTracker->lastError());

        dialog.close();
        QObject::disconnect(conn);
    }
    
    m_statusWidget->setVideoStatus(StatusWidget::Ready);

    // enable UI elements
    m_mazeJSView->setEnabled(true);
    ui->cameraGroupBox->setEnabled(true);
    m_running = false;
}

/**
 * Intan test run, we will not record anything.
 */
void MainWindow::intanRunActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);

    m_statusWidget->setIntanStatus(StatusWidget::Active);
    m_intanUI->runInterfaceBoard();
}

void MainWindow::setDataExportBaseDir(const QString& dir)
{
    if (dir.isEmpty())
        return;

    m_dataExportBaseDir = dir;
    m_exportDirValid = QDir().exists(m_dataExportBaseDir);
    exportDirLabel->setText(m_dataExportBaseDir);

    // update the export directory
    updateDataExportDir();

    // we can run as soon as we have a valid base directory
    setRunPossible(m_exportDirValid);
    if (m_exportDirValid)
        m_statusWidget->setSystemStatus(StatusWidget::Configured);
}

void MainWindow::updateDataExportDir()
{
    m_dataExportDir = QDir::cleanPath(QString::fromUtf8("%1/%2/%3/%4")
                                    .arg(m_dataExportBaseDir)
                                    .arg(m_currentSubject.id)
                                    .arg(m_currentDate)
                                    .arg(m_experimentId));
    exportDirInfoLabel->setText(QString("Recorded data will be stored in: %1").arg(m_dataExportDir));
}

void MainWindow::openDataExportDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this,
                                                 "Select Directory",
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 QFileDialog::ShowDirsOnly);
    setDataExportBaseDir(dir);
}

void MainWindow::changeTestSubject(const TestSubject &subject)
{
    m_currentSubject = subject;
    updateDataExportDir();
}

void MainWindow::changeExperimentId(const QString& text)
{
    m_experimentId = text;
    updateDataExportDir();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_running)
        stopActionTriggered();

    QSettings settings("DraguhnLab", "MazeAmaze");
    settings.setValue("main/geometry", saveGeometry());

    event->accept();
}

void MainWindow::saveSettingsActionTriggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Settings Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("MazeAmaze Settings Files (*.mamc)"));

    if (fileName.isEmpty())
        return;

    KTar tar(fileName);
    if (!tar.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Can not save settings"),
                              tr("Unable to open new settings file for writing."));
        return;
    }
    setStatusText("Saving settings to file...");

    QDir confBaseDir(QString("%1/..").arg(fileName));

    // save basic settings
    QJsonObject settings;
    settings.insert("programVersion", QCoreApplication::applicationVersion());
    settings.insert("creationDate", QDateTime::currentDateTime().date().toString());

    settings.insert("exportDir", m_dataExportBaseDir);
    settings.insert("experimentId", m_experimentId);

    QJsonObject videoSettings;
    videoSettings.insert("exportWidth", m_eresWidthEdit->value());
    videoSettings.insert("exportHeight", m_eresHeightEdit->value());
    videoSettings.insert("fps", m_fpsEdit->value());
    videoSettings.insert("gainEnabled", m_gainCB->isChecked());
    videoSettings.insert("exposureTime", m_exposureEdit->value());
    videoSettings.insert("uEyeConfig", confBaseDir.relativeFilePath(m_videoTracker->uEyeConfigFile()));
    videoSettings.insert("makeFrameTarball", m_saveTarCB->isChecked());
    settings.insert("video", videoSettings);

    tar.writeFile ("main.json", QJsonDocument(settings).toJson());

    // save list of subjects
    tar.writeFile ("subjects.json", QJsonDocument(m_subjectList->toJson()).toJson());

    // save Intan settings data
    QByteArray intanSettings;
    QDataStream intanSettingsStream(&intanSettings,QIODevice::WriteOnly);
    m_intanUI->exportSettings(intanSettingsStream);

    tar.writeFile ("intan.isf", intanSettings);

    // save Maze JavaScript / QScript
    tar.writeFile ("maze-script.qs", QByteArray(m_mazeJSView->document()->text().toStdString().c_str()));

    tar.close();

    QFileInfo fi(fileName);
    this->setWindowTitle(QStringLiteral("MazeAmaze - %1").arg(fi.fileName()));

    setStatusText("Ready.");
}

void MainWindow::loadSettingsActionTriggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 tr("Select Settings Filename"),
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 tr("MazeAmaze Settings Files (*.mamc)"));
    if (fileName.isEmpty())
        return;

    KTar tar(fileName);
    if (!tar.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Can not load settings"),
                              tr("Unable to open settings file for reading."));
        return;
    }

    setStatusText("Loading settings...");
    auto rootDir = tar.directory();

    // load main settings
    auto globalSettingsFile = rootDir->file("main.json");
    if (globalSettingsFile == nullptr) {
        QMessageBox::critical(this, tr("Can not load settings"),
                              tr("The settings file is damaged or is no valid MazeAmaze configuration bundle."));
        setStatusText("");
        return;
    }

    // disable all UI elements while we are loading stuff
    this->setEnabled(false);

    QDir confBaseDir(QString("%1/..").arg(fileName));

    auto mainDoc = QJsonDocument::fromJson(globalSettingsFile->data());
    auto rootObj = mainDoc.object();

    setDataExportBaseDir(rootObj.value("exportDir").toString());
    ui->expIdEdit->setText(rootObj.value("experimentId").toString());

    auto videoSettings = rootObj.value("video").toObject();
    m_eresWidthEdit->setValue(videoSettings.value("exportWidth").toInt(800));
    m_eresHeightEdit->setValue(videoSettings.value("exportHeight").toInt(600));
    m_fpsEdit->setValue(videoSettings.value("fps").toInt(20));
    m_gainCB->setChecked(videoSettings.value("gainEnabled").toBool());
    m_exposureEdit->setValue(videoSettings.value("exposureTime").toDouble(6));
    m_saveTarCB->setChecked(videoSettings.value("makeFrameTarball").toBool(true));

    auto uEyeConfFile = videoSettings.value("uEyeConfig").toString();
    if (!uEyeConfFile.isEmpty()) {
        uEyeConfFile = confBaseDir.absoluteFilePath(uEyeConfFile);
        m_videoTracker->setUEyeConfigFile(uEyeConfFile);
        m_ueyeConfFileLbl->setText(uEyeConfFile);
    }

    // load list of subjects
    auto subjectsFile = rootDir->file("subjects.json");
    if (subjectsFile != nullptr) {
        // not having a list of subjects is totally fine

        auto subjDoc = QJsonDocument::fromJson(subjectsFile->data());
        m_subjectList->fromJson(subjDoc.array());
    }

    // load Intan settings
    auto intanSettingsFile = rootDir->file("intan.isf");
    if (intanSettingsFile != nullptr)
        m_intanUI->loadSettings(intanSettingsFile->data());

    // save Maze JavaScript / QScript
    auto mazeScriptFile = rootDir->file("maze-script.qs");
    if (mazeScriptFile != nullptr)
        m_mazeJSView->document()->setText(mazeScriptFile->data());

    QFileInfo fi(fileName);
    this->setWindowTitle(QStringLiteral("MazeAmaze - %1").arg(fi.fileName()));

    // we are ready, enable all UI elements again
    setStatusText("Ready.");
    this->setEnabled(true);
}

void MainWindow::aboutActionTriggered()
{
    m_aboutDialog->exec();
}

void MainWindow::setStatusText(const QString& msg)
{
    statusBarLabel->setText(msg);
    QApplication::processEvents();
}
