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

#include "config.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QPushButton>
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
#include <QTableWidget>
#include <QMdiSubWindow>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSpinBox>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <memory>
#include <QSettings>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QHeaderView>

#include <KTar>

#include "ma-private.h"

#include "modules/rhd2000/intanui.h"
#include "modules/rhd2000/waveplot.h"

#include "hrclock.h"

#include "modules/traceplot/traceplotproxy.h"
#include "modules/traceplot/traceview.h"

#include "moduleindicator.h"
#include "modulemanager.h"
#include "moduleselectdialog.h"

#include "modules/rhd2000/rhd2000module.h"


#define CONFIG_FILE_FORMAT_VERSION "2"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);

    // Create status bar
    m_statusBarLabel = new QLabel(tr(""));
    statusBar()->addWidget(m_statusBarLabel, 1);
    statusBar()->setSizeGripEnabled(false);  // fixed window size

    // setup general page
    auto openDirBtn = new QToolButton();
    openDirBtn->setIcon(QIcon::fromTheme("folder-open"));

    auto dirInfoLabel = new QLabel("Export &Directory:");
    dirInfoLabel->setBuddy(openDirBtn);

    m_exportDirLabel = new QLabel("???");
    m_exportDirInfoLabel = new QLabel ("Recorded data will be stored in: The directory you select.");

    ui->dataExportDirLayout->addWidget(dirInfoLabel);
    ui->dataExportDirLayout->addWidget(m_exportDirLabel);
    ui->dataExportDirLayout->addWidget(openDirBtn);
    ui->dataExportLayout->addWidget(m_exportDirInfoLabel);

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
    ui->scrollAreaLayout->addStretch();

    // configure actions
    ui->actionRun->setEnabled(false);
    ui->actionStop->setEnabled(false);

    // connect actions
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runActionTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::stopActionTriggered);
    connect(ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsActionTriggered);
    connect(ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsActionTriggered);

    // various
    ui->tabWidget->setCurrentIndex(0);
    m_exportDirValid = false;

    // set date ID string
    auto time = QDateTime::currentDateTime();
    m_currentDate = time.date().toString("yyyy-MM-dd");

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

    // create new module manager
    m_modManager = new ModuleManager(this, this);
    connect(m_modManager, &ModuleManager::moduleCreated, this, &MainWindow::moduleAdded);
    connect(m_modManager, &ModuleManager::moduleError, this, &MainWindow::receivedModuleError);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setRunPossible(bool enabled)
{
    ui->actionRun->setEnabled(enabled);
}

void MainWindow::setStopPossible(bool enabled)
{
    ui->actionStop->setEnabled(enabled);
}

bool MainWindow::makeDirectory(const QString &dir)
{
    if (!QDir().mkpath(dir)) {
        QMessageBox::critical(this, "Error",
                              QString("Unable to create directory '%1'.").arg(dir));
        setStatusText("OS error.");
        return false;
    }

    return true;
}

void MainWindow::runActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);
    m_failed = false;

    if (m_modManager->activeModules().isEmpty()) {
        QMessageBox::warning(this, "Configuration error", "You did not add a single module to be run.\nPlease add a module to the experiment to continue.");
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    // determine and create the directory for ephys data
    qDebug() << "Initializing new recording run";

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

    if (!makeDirectory(m_dataExportDir)) {
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    auto timer = new HRTimer;
    Q_FOREACH(auto mod, m_modManager->activeModules()) {
        setStatusText(QStringLiteral("Preparing %1...").arg(mod->name()));
        if (!mod->prepare(m_dataExportDir, m_currentSubject, timer)) {
            m_failed = true;
            setStatusText(QStringLiteral("Module %1 failed to prepare.").arg(mod->name()));
            break;
        }
    }
    if (m_failed)
        return;

    setStatusText(QStringLiteral("Initializing launch..."));

    timer->start();
    Q_FOREACH(auto mod, m_modManager->activeModules()) {
        mod->start();
    }

    m_running = true;
    setStatusText(QStringLiteral("Running..."));

    while (m_running) {
        Q_FOREACH(auto mod, m_modManager->activeModules()) {
            if (!mod->runCycle()){
                setStatusText(QStringLiteral("Module %1 failed.").arg(mod->name()));
                m_failed = true;
                break;
            }
        }
        QApplication::processEvents();
        if (m_failed)
            break;
    }

    auto finishTimestamp = static_cast<long long>(timer->timeSinceStartMsec().count());

    Q_FOREACH(auto mod, m_modManager->activeModules()) {
        setStatusText(QStringLiteral("Stopping %1...").arg(mod->name()));
        mod->stop();
    }

    delete timer;

    setStatusText(QStringLiteral("Writing manifest..."));

    // write manifest with misc information
    QDateTime curDateTime(QDateTime::currentDateTime());

    QJsonObject manifest;
    manifest.insert("appVersion", QApplication::applicationVersion());
    manifest.insert("subjectId", m_currentSubject.id);
    manifest.insert("subjectGroup", m_currentSubject.group);
    manifest.insert("subjectComment", m_currentSubject.comment);
    manifest.insert("recordingLengthMsec", finishTimestamp);
    manifest.insert("date", curDateTime.toString(Qt::ISODate));
    manifest.insert("success", !m_failed);

    QJsonArray jActiveModules;
    Q_FOREACH(auto mod, m_modManager->activeModules()) {
        QJsonObject info;
        info.insert(mod->id(), mod->name());
        jActiveModules.append(info);
    }
    manifest.insert("activeModules", jActiveModules);

    QFile manifestFile(QStringLiteral("%1/manifest.json").arg(m_dataExportDir));
    if (!manifestFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Unable to finish recording", "Unable to open manifest file for writing.");
        setRunPossible(true);
        setStopPossible(false);
        return;
    }

    QTextStream manifestFileOut(&manifestFile);
    manifestFileOut << QJsonDocument(manifest).toJson();

    setStatusText(QStringLiteral("Ready."));
}

void MainWindow::stopActionTriggered()
{
    setRunPossible(m_exportDirValid);
    setStopPossible(false);

    m_running = false;
}

void MainWindow::setDataExportBaseDir(const QString& dir)
{
    if (dir.isEmpty())
        return;

    m_dataExportBaseDir = dir;
    m_exportDirValid = QDir().exists(m_dataExportBaseDir);
    m_exportDirLabel->setText(m_dataExportBaseDir);

    auto font = m_exportDirLabel->font();
    font.setBold(false);
    auto palette = m_exportDirLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    if (m_dataExportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::TempLocation)) ||
        m_dataExportBaseDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))) {
        font.setBold(true);
        palette.setColor(QPalette::WindowText, Qt::red);
    }
    m_exportDirLabel->setPalette(palette);
    m_exportDirLabel->setFont(font);

    // update the export directory
    updateDataExportDir();

    // we can run as soon as we have a valid base directory
    setRunPossible(m_exportDirValid);
}

void MainWindow::updateDataExportDir()
{
    m_dataExportDir = QDir::cleanPath(QString::fromUtf8("%1/%2/%3/%4")
                                    .arg(m_dataExportBaseDir)
                                    .arg(m_currentSubject.id)
                                    .arg(m_currentDate)
                                    .arg(m_experimentId));

    auto palette = m_exportDirInfoLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    if (m_dataExportDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::TempLocation)) ||
        m_dataExportDir.startsWith(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))) {
        palette.setColor(QPalette::WindowText, Qt::red);
    }
    m_exportDirInfoLabel->setPalette(palette);

    m_exportDirInfoLabel->setText(QString("Recorded data will be stored in: %1").arg(m_dataExportDir));
}

bool MainWindow::saveConfiguration(const QString &fileName)
{
    KTar tar(fileName);
    if (!tar.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to open new configuration file for writing.";
        return false;
    }
    setStatusText("Saving configuration to file...");

    QDir confBaseDir(QStringLiteral("%1/..").arg(fileName));

    // save basic settings
    QJsonObject settings;
    settings.insert("formatVersion", CONFIG_FILE_FORMAT_VERSION);
    settings.insert("appVersion", QCoreApplication::applicationVersion());
    settings.insert("creationDate", QDateTime::currentDateTime().date().toString());

    settings.insert("exportDir", m_dataExportBaseDir);
    settings.insert("experimentId", m_experimentId);

    // basic configuration
    tar.writeFile ("main.json", QJsonDocument(settings).toJson());

    // save list of subjects
    tar.writeFile ("subjects.json", QJsonDocument(m_subjectList->toJson()).toJson());

    // save module settings
    auto modIndex = 0;
    Q_FOREACH(auto mod, m_modManager->activeModules()) {
        if (!tar.writeDir(QString::number(modIndex)))
            return false;
        auto modSettings = mod->serializeSettings(confBaseDir.absolutePath());
        tar.writeFile(QStringLiteral("%1/%2.dat").arg(modIndex).arg(mod->id()), modSettings);

        QJsonObject modInfo;
        modInfo.insert("id", mod->id());
        tar.writeFile(QStringLiteral("%1/info.json").arg(modIndex), QJsonDocument(modInfo).toJson());

        modIndex++;
    }

    tar.close();

    QFileInfo fi(fileName);
    this->updateWindowTitle(fi.fileName());

    setStatusText("Ready.");
    return true;
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

    QApplication::quit();
}

void MainWindow::saveSettingsActionTriggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Configuration Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("MazeAmaze Configuration Files (*.mact)"));

    if (fileName.isEmpty())
        return;

    if (!saveConfiguration(fileName)) {
        QMessageBox::critical(this, tr("Can not save configuration"),
                              tr("Unable to write configuration file to disk."));
    }
}

void MainWindow::updateWindowTitle(const QString& fileName)
{
    if (fileName.isEmpty()) {
        this->setWindowTitle(QStringLiteral("MazeAmaze"));
    } else {
        this->setWindowTitle(QStringLiteral("MazeAmaze - %2").arg(fileName));
    }
}

void MainWindow::loadSettingsActionTriggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 tr("Select Settings Filename"),
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 tr("MazeAmaze Settings Files (*.mact)"));
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

    if (rootObj.value("formatVersion").toString() != CONFIG_FILE_FORMAT_VERSION) {
        auto reply = QMessageBox::question(this,
                                           "Incompatible configuration",
                                           QStringLiteral("The settings file you want to load was created with a different, possibly older version of MazeAmaze and may not work correctly in this version.\n"
                                                          "Should we attempt to load it anyway? (This may result in unexpected behavior)"),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            this->setEnabled(true);
            setStatusText("Aborted configuration loading.");
            return;
        }
    }

    setDataExportBaseDir(rootObj.value("exportDir").toString());
    ui->expIdEdit->setText(rootObj.value("experimentId").toString());

#if 0
    auto videoSettings = rootObj.value("video").toObject();
    m_eresWidthEdit->setValue(videoSettings.value("exportWidth").toInt(800));
    m_eresHeightEdit->setValue(videoSettings.value("exportHeight").toInt(600));
    m_fpsEdit->setValue(videoSettings.value("fps").toInt(20));
    m_gainCB->setChecked(videoSettings.value("gainEnabled").toBool());
    m_exposureEdit->setValue(videoSettings.value("exposureTime").toDouble(6));
    //m_saveTarCB->setChecked(videoSettings.value("makeFrameTarball").toBool(true));
    m_camFlashMode->setChecked(videoSettings.value("gpioFlash").toBool(true));

    auto uEyeConfFile = videoSettings.value("uEyeConfig").toString();
    if (!uEyeConfFile.isEmpty()) {
        uEyeConfFile = confBaseDir.absoluteFilePath(uEyeConfFile);
        //! m_videoTracker->setUEyeConfigFile(uEyeConfFile);
        m_ueyeConfFileLbl->setText(uEyeConfFile);
    }
#endif

    // load list of subjects
    auto subjectsFile = rootDir->file("subjects.json");
    if (subjectsFile != nullptr) {
        // not having a list of subjects is totally fine

        auto subjDoc = QJsonDocument::fromJson(subjectsFile->data());
        m_subjectList->fromJson(subjDoc.array());
    }

#if 0
    // load Intan settings
    auto intanSettingsFile = rootDir->file("intan.isf");
    if (intanSettingsFile != nullptr)
        m_intanUI->loadSettings(intanSettingsFile->data());

    // load Maze IO Python script
    auto mazeScriptFile = rootDir->file("mscript.py");
    if (mazeScriptFile == nullptr)
        m_mscriptView->document()->setText("import maio as io\n\n# Empty\n");
    else
        m_mscriptView->document()->setText(mazeScriptFile->data());
#endif

    QFileInfo fi(fileName);
    this->updateWindowTitle(fi.fileName());

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
    m_statusBarLabel->setText(msg);
    QApplication::processEvents();
}

void MainWindow::on_tbAddModule_clicked()
{
    ModuleSelectDialog modDialog(m_modManager->moduleInfo(), this);
    if (modDialog.exec() == QDialog::Accepted) {
        if (!modDialog.selectedEntryId().isEmpty())
            m_modManager->createModule(modDialog.selectedEntryId());
    }
}

void MainWindow::moduleAdded(AbstractModule *mod)
{
    auto mi = new ModuleIndicator(mod, m_modManager, ui->scrollArea);

    // add widget after the stretcher
    ui->scrollAreaLayout->insertWidget(ui->scrollAreaLayout->count() - 1, mi);
}

void MainWindow::receivedModuleError(AbstractModule *mod, const QString &message)
{
    Q_UNUSED(mod);
    Q_UNUSED(message);
    m_failed = true;
    m_running = false;
    setRunPossible(true);
    setStopPossible(false);
}
