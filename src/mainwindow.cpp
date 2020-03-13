/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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
#include <QSvgWidget>
#include <QFontMetricsF>
#include <KTar>

#include "aboutdialog.h"
#include "engine.h"
#include "moduleapi.h"
#include "timingsdialog.h"


// config format API level
static const QString CONFIG_FILE_FORMAT_VERSION = QStringLiteral("2");

static bool switchIconTheme(const QString& themeName)
{
    if (themeName.isEmpty())
        return false;
    if (QIcon::themeName() == themeName)
        return true;
    auto found = false;
    for (auto &path : QIcon::themeSearchPaths()) {
        QFileInfo fi(QStringLiteral("%1/%2").arg(path).arg(themeName));
        if (fi.isDir()) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;
    QIcon::setThemeName(themeName);
    qDebug() << "Switched icon theme to" << themeName;

    return true;
}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // Load settings and set icon theme explicitly
    // (otherwise the application may look ugly or incomplete on GNOME)
    QSettings settings("DraguhnLab", "Syntalos");
    auto themeName = settings.value("main/iconTheme").toString();

    // try to enforce breeze first, then the user-defined theme name, then the system default
    switchIconTheme(QStringLiteral("breeze"));
    switchIconTheme(themeName);

    // create main window UI
    ui->setupUi(this);
    ui->tabWidget->setCurrentIndex(0);

    // Create status bar
    m_statusBarLabel = new QLabel(QString(), statusBar());
    m_statusBarLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusBar()->addWidget(m_statusBarLabel, 1);
    statusBar()->setSizeGripEnabled(true);

    // Show a welcome status message (a poor replacement for the previous joke messages,
    // but we want to be more serious now ^^)
    QFile greetingsRc(QStringLiteral(":/texts/greetings.json"));
    if(greetingsRc.open(QIODevice::ReadOnly)) {
        const auto greetDoc = QJsonDocument::fromJson(greetingsRc.readAll());
        const auto grObj = greetDoc.object();
        if (!grObj.isEmpty()) {
            const auto greetings = grObj.keys();
            setStatusText(greetings[rand() % greetings.length()].trimmed());
        }
    }
    greetingsRc.close();

    // setup general page
    connect(ui->tbOpenDir, &QToolButton::clicked, this, &MainWindow::openDataExportDirectory);

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
        sub.active = ui->subjectActiveCheckBox->isChecked();
        sub.comment = ui->remarksTextEdit->toPlainText();

        m_subjectList->removeRow(row);
        m_subjectList->insertSubject(row, sub);
        ui->subjectListView->setCurrentIndex(m_subjectList->index(row));
    });

    ui->subjectSelectComboBox->setModel(m_subjectList);

    // diagnostics panels
    m_timingsDialog = new TimingsDialog(this);

    // configure actions
    ui->actionRun->setEnabled(false);
    ui->actionStop->setEnabled(false);

    // connect actions
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runActionTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::stopActionTriggered);
    connect(ui->actionSaveSettings, &QAction::triggered, this, &MainWindow::saveSettingsActionTriggered);
    connect(ui->actionLoadSettings, &QAction::triggered, this, &MainWindow::loadSettingsActionTriggered);

    // various
    ui->exportDirLabel->setText(QStringLiteral("???"));
    ui->exportBaseDirLabel->setText(QStringLiteral("The directory you select."));
    ui->tabWidget->setCurrentIndex(0);

    // connect about dialog trigger
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::aboutActionTriggered);

    // restore main window geometry
    restoreGeometry(settings.value("main/geometry").toByteArray());

    // get a reference to the current engine
    m_engine = ui->graphForm->engine();
    connect(m_engine, &Engine::runFailed, this, &MainWindow::moduleErrorReceived);
    connect(m_engine, &Engine::statusMessage, this, &MainWindow::statusMessageChanged);
    connect(m_engine, &Engine::moduleCreated, this, &MainWindow::onModuleCreated);
    connect(m_engine, &Engine::preRunStart, this, &MainWindow::onEnginePreRunStart);

    // create loading indicator for long loading/running tasks
    m_runIndicatorWidget = new QSvgWidget(this);
    m_runIndicatorWidget->load(QStringLiteral(":/animations/running.svg"));

    const auto indicatorWidgetDim = ui->mainToolBar->height() - 2;
    m_runIndicatorWidget->setMaximumSize(QSize(indicatorWidgetDim, indicatorWidgetDim));
    m_runIndicatorWidget->setMinimumSize(QSize(indicatorWidgetDim, indicatorWidgetDim));
    m_runIndicatorWidget->raise();
    m_runIndicatorWidget->hide();
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
    ui->graphForm->setModifyPossible(!enabled);
    if (enabled)
        m_runIndicatorWidget->show();
    else
        m_runIndicatorWidget->hide();
}

void MainWindow::runActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);

    m_engine->run();

    setRunPossible(true);
    setStopPossible(false);
}

void MainWindow::stopActionTriggered()
{
    setRunPossible(m_engine->exportDirIsValid());
    setStopPossible(false);

    m_engine->stop();
}

void MainWindow::setDataExportBaseDir(const QString& dir)
{
    if (dir.isEmpty())
        return;

    m_engine->setExportBaseDir(dir);
    updateExportDirDisplay();
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

    settings.insert("exportBaseDir", m_engine->exportBaseDir());
    settings.insert("experimentId", m_engine->experimentId());

    // basic configuration
    tar.writeFile ("main.json", QJsonDocument(settings).toJson());

    // save list of subjects
    tar.writeFile ("subjects.json", QJsonDocument(m_subjectList->toJson()).toJson());

    // save graph settings
    ui->graphForm->graphView()->saveState();
    tar.writeFile ("graph.json", QJsonDocument(ui->graphForm->graphView()->settings()).toJson());

    // save module settings
    auto modIndex = 0;
    for (auto &mod : m_engine->activeModules()) {
        if (!tar.writeDir(QString::number(modIndex)))
            return false;
        auto modSettings = mod->serializeSettings(confBaseDir.absolutePath());
        tar.writeFile(QStringLiteral("%1/%2.dat").arg(modIndex).arg(mod->id()), modSettings);

        QJsonObject modInfo;
        modInfo.insert("id", mod->id());
        modInfo.insert("name", mod->name());
        modInfo.insert("uiDisplayGeometry", mod->serializeDisplayUiGeometry());

        // save info about port subscriptions in
        // the form inPortId -> sourceModuleName
        QJsonObject modSubs;
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;
            QJsonArray srcVal = {iport->outPort()->owner()->name(), iport->outPort()->id()};
            modSubs.insert(iport->id(), srcVal);
        }

        modInfo.insert("subscriptions", modSubs);
        tar.writeFile(QStringLiteral("%1/info.json").arg(modIndex), QJsonDocument(modInfo).toJson());

        modIndex++;
    }

    tar.close();

    QFileInfo fi(fileName);
    this->updateWindowTitle(fi.fileName());

    setStatusText("Ready.");
    return true;
}

bool MainWindow::loadConfiguration(const QString &fileName)
{
    KTar tar(fileName);
    if (!tar.open(QIODevice::ReadOnly)) {
        qCritical() << "Unable to open settings file for reading.";
        return false;
    }

    auto rootDir = tar.directory();

    // load main settings
    auto globalSettingsFile = rootDir->file("main.json");
    if (globalSettingsFile == nullptr) {
        QMessageBox::critical(this, tr("Can not load settings"),
                              tr("The settings file is damaged or is no valid Syntalos configuration bundle."));
        setStatusText("");
        return false;
    }

    // disable all UI elements while we are loading stuff
    this->setEnabled(false);

    auto mainDoc = QJsonDocument::fromJson(globalSettingsFile->data());
    auto rootObj = mainDoc.object();

    if (rootObj.value("formatVersion").toString() != CONFIG_FILE_FORMAT_VERSION) {
        auto reply = QMessageBox::question(this,
                                           "Incompatible configuration",
                                           QStringLiteral("The settings file you want to load was created with a different, possibly older version of Syntalos and may not work correctly in this version.\n"
                                                          "Should we attempt to load it anyway? (This may result in unexpected behavior)"),
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            this->setEnabled(true);
            setStatusText("Aborted configuration loading.");
            return true;
        }
    }

    setDataExportBaseDir(rootObj.value("exportBaseDir").toString());
    ui->expIdEdit->setText(rootObj.value("experimentId").toString());

    // load list of subjects
    auto subjectsFile = rootDir->file("subjects.json");
    if (subjectsFile != nullptr) {
        // not having a list of subjects is totally fine

        auto subjDoc = QJsonDocument::fromJson(subjectsFile->data());
        m_subjectList->fromJson(subjDoc.array());
    }

    // load graph settings
    auto graphDataFile = rootDir->file("graph.json");
    if (graphDataFile != nullptr) {
        QJsonObject graphSettings;
        auto graphDoc = QJsonDocument::fromJson(graphDataFile->data());
        graphSettings = graphDoc.object();

        // the graph view will apply stored settings to new nodes automatically
        // from here on.
        ui->graphForm->graphView()->setSettings(graphSettings);
    }

    m_engine->removeAllModules();
    auto rootEntries = rootDir->entries();
    rootEntries.sort();

    QDir confBaseDir(QString("%1/..").arg(fileName));

    // we load the modules in two passes, to ensure they can all register
    // their interdependencies correctly.
    QList<QPair<AbstractModule*, QByteArray>> modSettingsList;

    // add modules
    QList<QPair<AbstractModule*, QJsonObject>> jSubInfo;
    for (auto &ename : rootEntries) {
        auto e = rootDir->entry(ename);
        if (!e->isDirectory())
            continue;
        auto ifile = rootDir->file(QStringLiteral("%1/info.json").arg(ename));
        if (ifile == nullptr)
            return false;

        auto jidoc = QJsonDocument::fromJson(ifile->data());
        auto iobj = jidoc.object();
        const auto modId = iobj.value("id").toString();
        const auto modName = iobj.value("name").toString();
        const auto uiDisplayGeometry = iobj.value("uiDisplayGeometry").toObject();
        const auto jSubs = iobj.value("subscriptions").toObject();

        auto mod = m_engine->createModule(modId, modName);
        if (mod == nullptr) {
            QMessageBox::critical(this, QStringLiteral("Can not load settings"),
                                  QStringLiteral("Unable to find module '%1' - please install the module first, then attempt to load this configuration again.").arg(modId));
            return false;
        }
        auto sfile = rootDir->file(QStringLiteral("%1/%2.dat").arg(ename).arg(modId));
        QByteArray sdata;
        if (sfile == nullptr)
            qWarning() << "Settings data for" << modId << "was not found.";
        else
            sdata = sfile->data();

        if (!uiDisplayGeometry.isEmpty())
            mod->restoreDisplayUiGeometry(uiDisplayGeometry);

        // store subscription info to connect modules later
        jSubInfo.append(qMakePair(mod, jSubs));

        // store module-owned configuration for later
        modSettingsList.append(qMakePair(mod, sdata));
    }

    // create module connections
    for (auto &pair : jSubInfo) {
        auto mod = pair.first;
        const auto jSubs = pair.second;
        for (const QString &iPortId : jSubs.keys()) {
            const auto modPortPair = jSubs.value(iPortId).toArray();
            if (modPortPair.size() != 2) {
                qWarning().noquote() << "Malformed project data: Invalid project port pair in" << mod->name() << "settings.";
                continue;
            }
            const auto srcModName = modPortPair[0].toString();
            const auto srcModOutPortId = modPortPair[1].toString();
            const auto srcMod = m_engine->moduleByName(srcModName);
            if (srcMod == nullptr) {
                qWarning().noquote() << "Error when loading project: Source module" << srcModName << "plugged into" << iPortId << "of" << mod->name() << "was not found. Skipped connection.";
                continue;
            }
            auto inPort = mod->inPortById(iPortId);
            if (inPort.get() == nullptr) {
                qWarning().noquote() << "Error when loading project: Module" << mod->name() << "has no input port with ID" << iPortId;
                continue;
            }
            auto outPort = srcMod->outPortById(srcModOutPortId);
            if (outPort.get() == nullptr) {
                qWarning().noquote() << "Error when loading project: Module" << srcMod->name() << "has no output port with ID" << srcModOutPortId;
                continue;
            }
            inPort->setSubscription(outPort.get(), outPort->subscribe());
        }
    }

    // load module-owned configurations
    for (auto &pair : modSettingsList) {
        auto mod = pair.first;
        if (!mod->loadSettings(confBaseDir.absolutePath(), pair.second)) {
            QMessageBox::critical(this, QStringLiteral("Can not load settings"),
                                  QStringLiteral("Unable to load module settings for '%1'.").arg(mod->name()));
            return false;
        }
    }

    QFileInfo fi(fileName);
    this->updateWindowTitle(fi.fileName());

    // we are ready, enable all UI elements again
    setStatusText("Ready.");
    this->setEnabled(true);

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
    m_engine->setTestSubject(subject);
}

void MainWindow::changeExperimentId(const QString& text)
{
    m_engine->setExperimentId(text);
    updateExportDirDisplay();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_engine->isRunning())
        stopActionTriggered();

    QSettings settings("DraguhnLab", "Syntalos");
    settings.setValue("main/geometry", saveGeometry());

    event->accept();

    QApplication::quit();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_runIndicatorWidget->move(ui->tabWidget->width() - m_runIndicatorWidget->width() - 4, ui->menuBar->height() + 4);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_runIndicatorWidget->move(ui->tabWidget->width() - m_runIndicatorWidget->width() - 4, ui->menuBar->height() + 4);
}

void MainWindow::saveSettingsActionTriggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Configuration Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("Syntalos Configuration Files (*.mact)"));

    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".mact"))
        fileName = QStringLiteral("%1.mact").arg(fileName);

    m_runIndicatorWidget->show();
    if (!saveConfiguration(fileName)) {
        QMessageBox::critical(this, tr("Can not save configuration"),
                              tr("Unable to write configuration file to disk."));
    }
    m_runIndicatorWidget->hide();
}

void MainWindow::updateWindowTitle(const QString& fileName)
{
    if (fileName.isEmpty()) {
        this->setWindowTitle(QStringLiteral("Syntalos"));
    } else {
        this->setWindowTitle(QStringLiteral("Syntalos - %2").arg(fileName));
    }
}

void MainWindow::updateExportDirDisplay()
{
    ui->exportBaseDirLabel->setText(m_engine->exportBaseDir());

    auto font = ui->exportBaseDirLabel->font();
    font.setBold(false);
    auto palette = ui->exportBaseDirLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    if (m_engine->exportDirIsTempDir()) {
        font.setBold(true);
        palette.setColor(QPalette::WindowText, Qt::red);
    }
    ui->exportBaseDirLabel->setPalette(palette);
    ui->exportBaseDirLabel->setFont(font);

    // we can run as soon as we have a valid base directory
    setRunPossible(m_engine->exportDirIsValid());

    palette = ui->exportDirLabel->palette();
    palette.setColor(QPalette::WindowText, Qt::black);
    if (m_engine->exportDirIsTempDir())
        palette.setColor(QPalette::WindowText, Qt::red);
    ui->exportDirLabel->setPalette(palette);
    ui->exportDirLabel->setText(m_engine->exportDir());
}

void MainWindow::loadSettingsActionTriggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 tr("Select Settings Filename"),
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 tr("Syntalos Settings Files (*.mact)"));
    if (fileName.isEmpty())
        return;



    setStatusText("Loading settings...");

    m_runIndicatorWidget->show();
    if (!loadConfiguration(fileName)) {
        QMessageBox::critical(this, tr("Can not load configuration"),
                              tr("Failed to load configuration."));
        m_engine->removeAllModules();
    }
    m_runIndicatorWidget->hide();
}

void MainWindow::aboutActionTriggered()
{
    AboutDialog about(this);

    for (auto &info : m_engine->library()->moduleInfo())
        about.addModuleLicense(info->name(), info->license());

    about.exec();
}

void MainWindow::onModuleCreated(ModuleInfo *, AbstractModule *mod)
{
    connect(mod, &AbstractModule::synchronizerDetailsChanged, m_timingsDialog, &TimingsDialog::onSynchronizerDetailsChanged, Qt::QueuedConnection);
    connect(mod, &AbstractModule::synchronizerOffsetChanged, m_timingsDialog, &TimingsDialog::onSynchronizerOffsetChanged, Qt::QueuedConnection);
}

void MainWindow::setStatusText(const QString& msg)
{
    m_statusBarLabel->setText(msg);
    QApplication::processEvents();
}

void MainWindow::moduleErrorReceived(AbstractModule *, const QString&)
{
    setRunPossible(true);
    setStopPossible(false);
}

void MainWindow::onEnginePreRunStart()
{
    m_timingsDialog->clear();
}

void MainWindow::statusMessageChanged(const QString &message)
{
    setStatusText(message);
}

void MainWindow::on_actionSubjectsLoad_triggered()
{
    QString fileName;
    fileName = QFileDialog::getOpenFileName(this,
                                            tr("Open Animal List Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("JSON Files (*.json)"));
    if (fileName.isEmpty())
        return;


    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const auto bytes = f.readAll();
    if (bytes != nullptr) {
        auto subjDoc = QJsonDocument::fromJson(bytes);
        m_subjectList->fromJson(subjDoc.array());
    }
}

void MainWindow::on_actionSubjectsSave_triggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Animal List Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("JSON Files (*.json)"));

    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".json"))
        fileName = QStringLiteral("%1.json").arg(fileName);

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&f);
    out << QJsonDocument(m_subjectList->toJson()).toJson();
}

void MainWindow::on_actionTimings_triggered()
{
    m_timingsDialog->show();
}
