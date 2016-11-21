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
    intanUI = new IntanUI(this);

    auto intanLayout = new QVBoxLayout();
    intanLayout->addWidget(intanUI);
    ui->intanTab->setLayout(intanLayout);

    ui->mdiArea->addSubWindow(intanUI->displayWidget())->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    // add Intan menu actions
    ui->menuIntan->addSeparator();
    ui->menuIntan->addAction(intanUI->renameChannelAction);
    ui->menuIntan->addAction(intanUI->toggleChannelEnableAction);
    ui->menuIntan->addAction(intanUI->enableAllChannelsAction);
    ui->menuIntan->addAction(intanUI->disableAllChannelsAction);
    ui->menuIntan->addSeparator();
    ui->menuIntan->addAction(intanUI->originalOrderAction);
    ui->menuIntan->addAction(intanUI->alphaOrderAction);

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

    auto mouseIdEdit = new QLineEdit();
    auto expIdEdit = new QLineEdit();
    mouseIdEdit->setPlaceholderText("A mouse ID, e.g. \"MM-18\"");
    expIdEdit->setPlaceholderText("Experiment ID, e.g. \"trial1\"");
    mouseIdEdit->setMinimumWidth(180);
    expIdEdit->setMinimumWidth(180);

    ui->expDetailsLayout->addRow(tr("&Mouse ID:"), mouseIdEdit);
    ui->expDetailsLayout->addRow(tr("&Experiment ID:"), expIdEdit);

    connect(mouseIdEdit, &QLineEdit::textChanged, this, &MainWindow::mouseIdChanged);
    connect(expIdEdit, &QLineEdit::textChanged, this, &MainWindow::experimentIdChanged);

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
    connect(m_videoTracker, &VideoTracker::newFrame, [=](time_t time, const cv::Mat& image) {
        m_rawVideoWidget->setWindowTitle(QString("Raw Video (at %1sec)").arg(time / 1000));
        m_rawVideoWidget->showImage(image);
    });

    m_trackVideoWidget = new VideoViewWidget(this);
    ui->mdiArea->addSubWindow(m_trackVideoWidget)->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    m_trackVideoWidget->setWindowTitle("Tracking");
    connect(m_videoTracker, &VideoTracker::newTrackingFrame, [=](time_t time, const cv::Mat& image) {
        m_trackVideoWidget->setWindowTitle(QString("Tracking (at %1sec)").arg(time / 1000));
        m_trackVideoWidget->showImage(image);
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
    m_eresWidthEdit->setMinimum(720);
    m_eresHeightEdit->setMinimum(400);
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
    m_gainCB->setChecked(true);
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

    // various
    ui->tabWidget->setCurrentIndex(0);
    exportDirValid = false;

    // set date ID string
    auto time = QDateTime::currentDateTime();
    currentDate = time.date().toString("yyyy-MM-dd");

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
}

void MainWindow::videoError(const QString &message)
{
    m_failed = true;
    QMessageBox::critical(this, "Video Error", message);
    stopActionTriggered();
    m_statusWidget->setVideoStatus(StatusWidget::Broken);
}

void MainWindow::scriptEvalError(int line, const QString& message)
{
    m_failed = true;
    QMessageBox::critical(this, "Maze Script Error",
                          QString("Uncaught exception at line %1: %2")
                                    .arg(line)
                                    .arg(message));
    stopActionTriggered();
}

bool MainWindow::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        QMessageBox::critical(this, "Error",
                              QString("Unable to create directory '%1'.").arg(dir));
        return false;
    }

    return true;
}

void MainWindow::runActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);
    m_failed = false;

    // safeguard against accidental data removals
    QDir deDir(dataExportDir);
    if (deDir.exists()) {
        auto reply = QMessageBox::question(this,
                                           "Really continue?",
                                           QString("The directory %1 already contains data (likely from a previous run). If you continue, the old data will be deleted. Continue and delete data?")
                                               .arg(dataExportDir),
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
    auto intanDataDir = QString::fromUtf8("%1/intan").arg(dataExportDir);
    if (!makeDirectory(intanDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    auto mazeEventDataDir = QString::fromUtf8("%1/maze").arg(dataExportDir);
    if (!makeDirectory(mazeEventDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    auto videoDataDir = QString::fromUtf8("%1/video").arg(dataExportDir);
    if (!makeDirectory(videoDataDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    // set base locations
    QString intanBaseName;
    if (mouseId.isEmpty()) {
        intanBaseName = QString::fromUtf8("%1/ephys").arg(intanDataDir);
        m_msintf->setEventFile(QString("%1/events.csv").arg(mazeEventDataDir));
        m_videoTracker->setMouseId("frame");
    } else {
        intanBaseName = QString::fromUtf8("%1/%2_ephys").arg(intanDataDir).arg(mouseId);
        m_msintf->setEventFile(QString("%1/%2_events.csv").arg(mazeEventDataDir).arg(mouseId));
        m_videoTracker->setMouseId(mouseId);
    }
    m_videoTracker->setDataLocation(videoDataDir);
    intanUI->setBaseFileName(intanBaseName);

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
    intanUI->recordInterfaceBoard();
}

void MainWindow::stopActionTriggered()
{
    setRunPossible(exportDirValid);
    setStopPossible(false);

    // stop Maze script
    m_msintf->stop();

    // stop video tracker
    m_videoTracker->stop();
    m_videoTracker->closeCamera();

    // stop interface board
    intanUI->stopInterfaceBoard();

    // enable UI elements
    m_mazeJSView->setEnabled(true);
    ui->cameraGroupBox->setEnabled(true);

    m_statusWidget->setIntanStatus(StatusWidget::Ready);
    m_statusWidget->setFirmataStatus(StatusWidget::Ready);
    m_statusWidget->setVideoStatus(StatusWidget::Ready);
    m_running = false;

    // compress frame tarball, if selected
    if (m_saveTarCB->isChecked()) {
        QProgressDialog dialog(this);

        dialog.setCancelButton(nullptr);
        dialog.setLabelText("Packing and compressing frames...");
        dialog.setWindowModality(Qt::WindowModal);
        dialog.show();

        if (!m_videoTracker->makeFrameTarball())
            QMessageBox::critical(this, "Error writing frame tarball", m_videoTracker->lastError());

        dialog.close();
    }
}

/**
 * Intan test run, we will not record anything.
 */
void MainWindow::intanRunActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);

    m_statusWidget->setIntanStatus(StatusWidget::Active);
    intanUI->runInterfaceBoard();
}

void MainWindow::setDataExportBaseDir(const QString& dir)
{
    if (dir.isEmpty())
        return;

    dataExportBaseDir = dir;
    exportDirValid = QDir().exists(dataExportBaseDir);
    exportDirLabel->setText(dataExportBaseDir);

    // update the export directory
    updateDataExportDir();

    // we can run as soon as we have a valid base directory
    setRunPossible(exportDirValid);
    if (exportDirValid)
        m_statusWidget->setSystemStatus(StatusWidget::Configured);
}

void MainWindow::updateDataExportDir()
{
    dataExportDir = QDir::cleanPath(QString::fromUtf8("%1/%2/%3/%4")
                                    .arg(dataExportBaseDir)
                                    .arg(mouseId)
                                    .arg(currentDate)
                                    .arg(experimentId));
    exportDirInfoLabel->setText(QString("Recorded data will be stored in: %1").arg(dataExportDir));
}

void MainWindow::openDataExportDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this,
                                                 "Select Directory",
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 QFileDialog::ShowDirsOnly);
    setDataExportBaseDir(dir);
}

void MainWindow::mouseIdChanged(const QString& text)
{
    mouseId = text;
    updateDataExportDir();
}

void MainWindow::experimentIdChanged(const QString& text)
{
    experimentId = text;
    updateDataExportDir();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_running)
        stopActionTriggered();
    event->accept();
}

void MainWindow::saveSettingsActionTriggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Settings Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("MazeAmaze Settings Files (*.maamc)"));

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

    settings.insert("exportDir", dataExportBaseDir);
    settings.insert("mouseId", mouseId);
    settings.insert("experimentId", experimentId);

    QJsonObject videoSettings;
    videoSettings.insert("exportWidth", m_eresWidthEdit->value());
    videoSettings.insert("exportHeight", m_eresHeightEdit->value());
    videoSettings.insert("fps", m_fpsEdit->value());
    videoSettings.insert("gainEnabled", m_gainCB->isChecked());
    videoSettings.insert("exposureTime", m_exposureEdit->value());
    videoSettings.insert("uEyeConfig", confBaseDir.relativeFilePath(m_videoTracker->uEyeConfigFile()));
    settings.insert("video", videoSettings);

    tar.writeFile ("main.json", QJsonDocument(settings).toJson());

    // save Intan settings data
    QByteArray intanSettings;
    QDataStream intanSettingsStream(&intanSettings,QIODevice::WriteOnly);
    intanUI->exportSettings(intanSettingsStream);

    tar.writeFile ("intan.isf", intanSettings);

    // save Maze JavaScript / QScript
    tar.writeFile ("maze-script.qs", QByteArray(m_mazeJSView->document()->text().toStdString().c_str()));

    tar.close();

    setStatusText("Ready.");
}

void MainWindow::loadSettingsActionTriggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 tr("Select Settings Filename"),
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 tr("MazeAmaze Settings Files (*.maamc)"));
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

    QDir confBaseDir(QString("%1/..").arg(fileName));

    auto mainDoc = QJsonDocument::fromJson(globalSettingsFile->data());
    auto rootObj = mainDoc.object();

    setDataExportBaseDir(rootObj.value("exportDir").toString());
    mouseIdChanged(rootObj.value("mouseId").toString());
    experimentIdChanged(rootObj.value("experimentId").toString());

    auto videoSettings = rootObj.value("video").toObject();
    m_eresWidthEdit->setValue(videoSettings.value("exportWidth").toInt(800));
    m_eresHeightEdit->setValue(videoSettings.value("exportHeight").toInt(600));
    m_fpsEdit->setValue(videoSettings.value("fps").toInt(20));
    m_gainCB->setChecked(videoSettings.value("gainEnabled").toBool());
    m_exposureEdit->setValue(videoSettings.value("exposureTime").toDouble(6));

    auto uEyeConfFile = videoSettings.value("uEyeConfig").toString();
    if (!uEyeConfFile.isEmpty()) {
        uEyeConfFile = confBaseDir.absoluteFilePath(uEyeConfFile);
        m_videoTracker->setUEyeConfigFile(uEyeConfFile);
        m_ueyeConfFileLbl->setText(uEyeConfFile);
    }

    // load Intan settings
    auto intanSettingsFile = rootDir->file("intan.isf");
    if (intanSettingsFile != nullptr)
        intanUI->loadSettings(intanSettingsFile->data());

    // save Maze JavaScript / QScript
    auto mazeScriptFile = rootDir->file("maze-script.qs");
    if (mazeScriptFile != nullptr)
        m_mazeJSView->document()->setText(mazeScriptFile->data());

    setStatusText("Ready.");
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
