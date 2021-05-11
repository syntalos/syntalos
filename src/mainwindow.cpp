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
#include <QTimer>
#include <QTableWidget>
#include <QMdiSubWindow>
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
#include <QInputDialog>
#include <memory>
#include <QListWidgetItem>
#include <QScrollBar>
#include <QHeaderView>
#include <QSvgWidget>
#include <QSvgRenderer>
#include <QFontMetricsF>
#include <KTar>

#include "aboutdialog.h"
#include "globalconfig.h"
#include "globalconfigdialog.h"
#include "engine.h"
#include "moduleapi.h"
#include "sysinfodialog.h"
#include "timingsdialog.h"
#include "utils/tomlutils.h"
#include "utils/colors.h"

// config format API level
static const QString CONFIG_FILE_FORMAT_VERSION = QStringLiteral("1");

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
    m_gconf = new GlobalConfig(this);

    // try to enforce breeze first, then the user-defined theme name, then the system default
    switchIconTheme(QStringLiteral("breeze"));
    switchIconTheme(m_gconf->iconThemeName());

    // create main window UI
    ui->setupUi(this);
    ui->stackedWidget->setCurrentIndex(0);

    // Create status bar
    m_statusBarLabel = new QLabel(QString(), statusBar());
    m_statusBarLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusBar()->addWidget(m_statusBarLabel, 1);
    statusBar()->setSizeGripEnabled(true);

    // Show a welcome status message (a poor replacement for the previous joke messages,
    // but we want to be more serious now (not too much though, just a tiny bit) ^^)
    QFile greetingsRc(QStringLiteral(":/texts/greetings.toml"));
    if (greetingsRc.open(QIODevice::ReadOnly)) {
        QString parseError;
        const auto greetingsVar = parseTomlData(greetingsRc.readAll(), parseError);
        if (parseError.isEmpty()) {
            const auto greetings = greetingsVar.value("greetings", QVariantList()).toList();
            if (!greetings.isEmpty()) {
                const auto myGreeting = greetings[rand() % greetings.length()].toHash();
                setStatusText(myGreeting.value("msg", "Hello World!").toString().trimmed());
                m_statusBarLabel->setToolTip(myGreeting.value("source", "Unknown").toString().trimmed());
                // reset tooltip after 10 seconds
                QTimer::singleShot(10 * 1000, [=]() {
                    m_statusBarLabel->setToolTip(QString());
                });
            }
        } else {
            setStatusText(QStringLiteral("I couldn't load a greeting message :-( - %1").arg(parseError));
        }
    }
    greetingsRc.close();

    // setup general page
    ui->exportBaseDirLabel->setText(QStringLiteral("[No directory selected]"));
    ui->exportDirLabel->setText(QStringLiteral("???"));

    // prepare warning labels
    ui->runWarningIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-warning")).pixmap(16, 16));
    ui->runWarnWidget->setVisible(false);
    ui->cpuWarningIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-information")).pixmap(16, 16));
    ui->cpuWarnWidget->setVisible(false);
    ui->diskSpaceWarningIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-important")).pixmap(16, 16));
    ui->diskSpaceWarnWidget->setVisible(false);
    ui->memoryWarningIconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-important")).pixmap(16, 16));
    ui->memoryWarnWidget->setVisible(false);
    ui->warnNotifyLayout->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    ui->panelRunInfo->setEnabled(false);

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

    // set up test subjects listing
    m_subjectList = new TestSubjectListModel(this);
    ui->subjectListView->setModel(m_subjectList);

    connect(ui->btnSubjectRemove, &QToolButton::clicked, [&]() {
        if (ui->subjectListView->currentIndex().isValid())
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
        sub.active = ui->cbSubjectActive->isChecked();
        sub.comment = ui->remarksTextEdit->toPlainText();
        m_subjectList->addSubject(sub);
    });

    connect(ui->subjectListView, &QListView::activated, [&](const QModelIndex &index) {
        auto sub = m_subjectList->subject(index.row());

        ui->idLineEdit->setText(sub.id);
        ui->groupLineEdit->setText(sub.group);
        ui->cbSubjectActive->setChecked(sub.active);
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
        sub.active = ui->cbSubjectActive->isChecked();
        sub.comment = ui->remarksTextEdit->toPlainText();

        m_subjectList->removeRow(row);
        m_subjectList->insertSubject(row, sub);
        ui->subjectListView->setCurrentIndex(m_subjectList->index(row));
    });

    ui->subjectSelectComboBox->setModel(m_subjectList);

    // set up experimenter listing
    m_experimenterList = new ExperimenterListModel(this);
    ui->experimenterListView->setModel(m_experimenterList);

    connect(ui->btnExperimenterRemove, &QToolButton::clicked, [&]() {
        if (ui->experimenterListView->currentIndex().isValid())
            m_experimenterList->removeRow(ui->experimenterListView->currentIndex().row());
        setExperimenterSelectVisible(!m_experimenterList->isEmpty());
        changeExperimenter(EDLAuthor());
    });

    connect(ui->btnExperimenterAdd, &QToolButton::clicked, [&]() {
        if (m_experimenterList->isEmpty())
            changeExperimenter(EDLAuthor());
        EDLAuthor newPerson;

        newPerson.name = QInputDialog::getText(this,
                                               QStringLiteral("Add new experimenter"),
                                               QStringLiteral("Full name of the new experimenter:")).trimmed();
        if (newPerson.name.isEmpty()) {
            QMessageBox::warning(this,
                                 QStringLiteral("Could not add experimenter"),
                                 QStringLiteral("Can not add a person with no name."));
            return;
        }

        while (true) {
            newPerson.email = QInputDialog::getText(this,
                                                   QStringLiteral("Add new experimenter"),
                                                   QStringLiteral("E-Mail address of the new experimenter (can be left blank):"),
                                                    QLineEdit::Normal,
                                                    newPerson.email).trimmed();
            // very rudimental check whether the entered stuff somewhat resembles an email address
            if (!newPerson.email.isEmpty() && (!newPerson.email.contains('@') || !newPerson.email.contains('.'))) {
                QMessageBox::information(this,
                                         QStringLiteral("E-Mail Invalid"),
                                         QStringLiteral("The entered E-Mail address is invalid. Please try again!"));
            } else {
                break;
            }
        }

        m_experimenterList->addPerson(newPerson);
        setExperimenterSelectVisible(!m_experimenterList->isEmpty());
    });

    connect(ui->tbChooseExperimenter, &QToolButton::clicked, [&]() {
        showExperimenterSelector(QStringLiteral("Person conducting the current experiment:"));
    });

    // diagnostics panels
    m_timingsDialog = new TimingsDialog(this);

    // configure actions
    ui->actionRun->setEnabled(false);
    ui->actionStop->setEnabled(false);

    // connect actions
    connect(ui->actionRun, &QAction::triggered, this, &MainWindow::runActionTriggered);
    connect(ui->actionRunTemp, &QAction::triggered, this, &MainWindow::temporaryRunActionTriggered);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::stopActionTriggered);
    connect(ui->actionProjectSaveAs, &QAction::triggered, this, &MainWindow::projectSaveAsActionTriggered);
    connect(ui->actionProjectSave, &QAction::triggered, this, &MainWindow::projectSaveActionTriggered);
    connect(ui->actionProjectOpen, &QAction::triggered, this, &MainWindow::projectOpenActionTriggered);

    // connect config and about dialog actions
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::aboutActionTriggered);
    connect(ui->actionGlobalConfig, &QAction::triggered, this, &MainWindow::globalConfigActionTriggered);

    // restore main window geometry
    restoreGeometry(m_gconf->mainWinGeometry());

    // get a reference to the current engine
    m_engine = ui->graphForm->engine();
    connect(m_engine, &Engine::runFailed, this, &MainWindow::moduleErrorReceived);
    connect(m_engine, &Engine::statusMessage, this, &MainWindow::statusMessageChanged);
    connect(m_engine, &Engine::moduleCreated, this, &MainWindow::onModuleCreated);
    connect(m_engine, &Engine::preRunStart, this, &MainWindow::onEnginePreRunStart);
    connect(m_engine, &Engine::runStopped, this, &MainWindow::onEngineStopped);
    connect(m_engine, &Engine::resourceWarning, this, &MainWindow::onEngineResourceWarning);
    connect(ui->graphForm, &ModuleGraphForm::busyStart, this, &MainWindow::showBusyIndicatorProcessing);
    connect(ui->graphForm, &ModuleGraphForm::busyEnd, this, &MainWindow::hideBusyIndicator);

    // create loading indicator for long loading/running tasks
    m_busyIndicator = new QSvgWidget(this);
    const auto indicatorWidgetDim = ui->mainToolBar->height() - 2;
    m_busyIndicator->setMaximumSize(QSize(indicatorWidgetDim, indicatorWidgetDim));
    m_busyIndicator->setMinimumSize(QSize(indicatorWidgetDim, indicatorWidgetDim));
    m_busyIndicator->raise();
    m_busyIndicator->hide();

    // don't show experimenter selection yet
    setExperimenterSelectVisible(false);

    // load modules
    m_engine->load();

    // timer to update verious time display during a run
    m_rtElapsedTimer = new QTimer(this);
    m_rtElapsedTimer->setInterval(1000);
    connect(m_rtElapsedTimer, &QTimer::timeout, this, &MainWindow::onElapsedTimeUpdate);

    // reset project filename
    setCurrentProjectFile(QString());

    // switch default focus
    ui->graphForm->setFocus();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setRunPossible(bool enabled)
{
    ui->actionRun->setEnabled(enabled && m_engine->exportDirIsValid());
    ui->actionRunTemp->setEnabled(enabled);
}

void MainWindow::setStopPossible(bool enabled)
{
    ui->actionStop->setEnabled(enabled);
    ui->graphForm->setModifyPossible(!enabled);
    ui->panelRunInfo->setEnabled(enabled);
    ui->panelRunSettings->setEnabled(!enabled);
    ui->actionGlobalConfig->setEnabled(!enabled);
    ui->widgetProjectSettings->setEnabled(!enabled);

    // do not permit save/load while we are running
    ui->actionProjectOpen->setEnabled(!enabled);
    ui->actionProjectSave->setEnabled(!enabled);
    ui->actionProjectSaveAs->setEnabled(!enabled);
    ui->actionSubjectsLoad->setEnabled(!enabled);
    ui->actionSubjectsSave->setEnabled(!enabled);

    if (enabled)
        showBusyIndicatorProcessing();
    else
        hideBusyIndicator();
}

void MainWindow::setExperimenterSelectVisible(bool visible)
{
    ui->experimenterLabel->setVisible(visible);
    ui->experimenterWidget->setVisible(visible);
}

void MainWindow::runActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);
    ui->runWarnWidget->setVisible(false);

    m_engine->setSaveInternalDiagnostics(m_gconf->saveExperimentDiagnostics());
    m_engine->setSimpleStorageNames(ui->cbSimpleStorageNames->isChecked());
    m_engine->run();

    setStopPossible(false);
    setRunPossible(true);
}

void MainWindow::temporaryRunActionTriggered()
{
    setRunPossible(false);
    setStopPossible(true);

    ui->exportDirLabel->setText(QStringLiteral("???"));
    ui->runWarningLabel->setText(QStringLiteral("No data of this run will be saved permanently!"));
    ui->runWarnWidget->setVisible(true);

    m_engine->runEphemeral();

    setStopPossible(false);
    ui->actionRunTemp->setEnabled(true);
    ui->runWarnWidget->setVisible(false);
    updateExportDirDisplay();
    setRunPossible(true);
}

void MainWindow::stopActionTriggered()
{
    ui->actionStop->setEnabled(false);
    showBusyIndicatorProcessing();
    QApplication::processEvents();
    m_engine->stop();
}

bool MainWindow::saveConfiguration(const QString &fileName)
{
    qDebug() << "Saving board as" << fileName;
    KTar tar(fileName);
    if (!tar.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to open new configuration file for writing.";
        return false;
    }
    setStatusText("Saving configuration to file...");

    setCurrentProjectFile(QString());
    QDir confBaseDir(QStringLiteral("%1/..").arg(fileName));

    // save basic settings
    QVariantHash settings;
    settings.insert("version_format", CONFIG_FILE_FORMAT_VERSION);
    settings.insert("version_app", QCoreApplication::applicationVersion());
    settings.insert("time_created", QDateTime::currentDateTime());

    settings.insert("export_base_dir", m_engine->exportBaseDir());
    settings.insert("experiment_id", m_engine->experimentId());

    settings.insert("simple_storage_names", ui->cbSimpleStorageNames->isChecked());

    // basic configuration
    tar.writeFile ("main.toml", qVariantHashToTomlData(settings));

    // save list of subjects
    tar.writeFile ("subjects.toml", qVariantHashToTomlData(m_subjectList->toVariantHash()));

    // save list of experimenters
    tar.writeFile ("experimenters.toml", qVariantHashToTomlData(m_experimenterList->toVariantHash()));

    // save graph settings
    ui->graphForm->graphView()->saveState();
    tar.writeFile ("graph.toml", qVariantHashToTomlData(ui->graphForm->graphView()->settings()));

    // save module settings
    auto modIndex = 0;
    for (auto &mod : m_engine->activeModules()) {
        if (!tar.writeDir(QString::number(modIndex)))
            return false;

        QVariantHash modSettings;
        QByteArray modExtraData;

        mod->serializeSettings(confBaseDir.absolutePath(), modSettings, modExtraData);
        if (!modSettings.isEmpty())
            tar.writeFile(QStringLiteral("%1/%2.toml").arg(modIndex).arg(mod->id()),
                          qVariantHashToTomlData(modSettings));
        if (!modExtraData.isEmpty())
            tar.writeFile(QStringLiteral("%1/%2.dat").arg(modIndex).arg(mod->id()),
                          modExtraData);

        QVariantHash modInfo;
        modInfo.insert("id", mod->id());
        modInfo.insert("name", mod->name());
        modInfo.insert("ui_display_geometry", mod->serializeDisplayUiGeometry());

        // save info about port subscriptions in
        // the form inPortId -> sourceModuleName
        QVariantHash modSubs;
        for (const auto &iport : mod->inPorts()) {
            if (!iport->hasSubscription())
                continue;
            QVariantList srcVal = {iport->outPort()->owner()->name(), iport->outPort()->id()};
            modSubs.insert(iport->id(), srcVal);
        }

        modInfo.insert("subscriptions", modSubs);
        tar.writeFile(QStringLiteral("%1/info.toml").arg(modIndex), qVariantHashToTomlData(modInfo));

        modIndex++;
    }

    tar.close();

    setCurrentProjectFile(fileName);
    setStatusText(QStringLiteral("Board saved at %1.").arg(QTime::currentTime().toString(Qt::TextDate)));
    return true;
}

bool MainWindow::loadConfiguration(const QString &fileName)
{
    KTar tar(fileName);
    if (!tar.open(QIODevice::ReadOnly)) {
        qCritical() << "Unable to open settings file for reading.";
        return false;
    }

    setCurrentProjectFile(QString());
    auto rootDir = tar.directory();

    // load main settings
    auto globalSettingsFile = rootDir->file("main.toml");
    if (globalSettingsFile == nullptr) {
        QMessageBox::critical(this,
                              QStringLiteral("Can not load settings"),
                              QStringLiteral("The settings file is damaged or is no valid Syntalos configuration bundle."));
        setStatusText("");
        return false;
    }

    QString parseError;
    const auto rootObj = parseTomlData(globalSettingsFile->data(), parseError);
    if (!parseError.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Can not load settings"),
                              QStringLiteral("The settings file is damaged or is no valid Syntalos configuration file. %1").arg(parseError));
        setStatusText("");
        return false;
    }

    if (rootObj.value("version_format").toString() != CONFIG_FILE_FORMAT_VERSION) {
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

    setDataExportBaseDir(rootObj.value("export_base_dir").toString());
    ui->expIdEdit->setText(rootObj.value("experiment_id").toString());
    ui->cbSimpleStorageNames->setChecked(rootObj.value("simple_storage_names", true).toBool());

    // load list of subjects
    m_subjectList->clear();
    auto subjectsFile = rootDir->file("subjects.toml");
    if (subjectsFile != nullptr) {
        // not having a list of subjects is totally fine

        setStatusText("Loading subject information...");
        const auto subjData = parseTomlData(subjectsFile->data(), parseError);
        if (parseError.isEmpty())
            m_subjectList->fromVariantHash(subjData);
        else
            qWarning() << "Unable to load test-subject data:" << parseError;
    }

    // load list of experimenters
    m_experimenterList->clear();
    changeExperimenter(EDLAuthor());
    auto experimentersFile = rootDir->file("experimenters.toml");
    if (experimentersFile != nullptr) {
        // not having a list of subjects is totally fine

        setStatusText("Loading experimenter data...");
        const auto peopleData = parseTomlData(experimentersFile->data(), parseError);
        if (parseError.isEmpty())
            m_experimenterList->fromVariantHash(peopleData);
        else
            qWarning() << "Unable to load experimenter data:" << parseError;
    }
    setExperimenterSelectVisible(!m_experimenterList->isEmpty());

    setStatusText("Destroying old modules...");
    m_engine->removeAllModules();
    auto rootEntries = rootDir->entries();
    rootEntries.sort();

    // load graph settings
    auto graphFile = rootDir->file("graph.toml");
    if (graphFile != nullptr) {
        setStatusText("Caching graph settings...");
        const auto graphConfig = parseTomlData(graphFile->data(), parseError);
        if (parseError.isEmpty()) {
            // the graph view will apply stored settings to new nodes automatically
            // from here on.
            ui->graphForm->graphView()->setSettings(graphConfig);
            ui->graphForm->graphView()->restoreState();
        } else {
            qWarning() << "Unable to parse graph configuration:" << parseError;
        }
    }

    // we load the modules in two passes, to ensure they can all register
    // their interdependencies correctly.
    QList<QPair<AbstractModule*, QPair<QVariantHash, QByteArray>>> modSettingsList;

    // add modules
    QList<QPair<AbstractModule*, QVariantHash>> jSubInfo;
    for (auto &ename : rootEntries) {
        auto e = rootDir->entry(ename);
        if (!e->isDirectory())
            continue;
        auto ifile = rootDir->file(QStringLiteral("%1/info.toml").arg(ename));
        if (ifile == nullptr)
            continue;

        auto iobj = parseTomlData(ifile->data(), parseError);
        if (!parseError.isEmpty())
            qWarning().noquote().nospace() << "Issue while loading module info: " << parseError;

        const auto modId = iobj.value("id").toString();
        const auto modName = iobj.value("name").toString();
        const auto uiDisplayGeometry = iobj.value("ui_display_geometry").toHash();
        const auto jSubs = iobj.value("subscriptions").toHash();

        setStatusText(QStringLiteral("Instantiating module: %1(%2)").arg(modId).arg(modName));
        auto mod = m_engine->createModule(modId, modName);
        if (mod == nullptr) {
            QMessageBox::critical(this, QStringLiteral("Can not load settings"),
                                  QStringLiteral("Unable to find module '%1' - please install the module first, then attempt to load this configuration again.").arg(modId));
            setStatusText("Failed to load settings.");
            return false;
        }
        auto sfile = rootDir->file(QStringLiteral("%1/%2.toml").arg(ename).arg(modId));
        QVariantHash modSettings;
        if (sfile != nullptr) {
            modSettings = parseTomlData(sfile->data(), parseError);
            if (!parseError.isEmpty())
                qWarning().noquote().nospace() << "Issue while loading module configuration for " << mod->name() << ": " << parseError;
        }
        sfile = rootDir->file(QStringLiteral("%1/%2.dat").arg(ename).arg(modId));
        QByteArray modSettingsEx;
        if (sfile != nullptr)
            modSettingsEx = sfile->data();

        if (!uiDisplayGeometry.isEmpty())
            mod->restoreDisplayUiGeometry(uiDisplayGeometry);

        // store subscription info to connect modules later
        jSubInfo.append(qMakePair(mod, jSubs));

        // store module-owned configuration for later
        modSettingsList.append(qMakePair(mod, qMakePair(modSettings, modSettingsEx)));
    }

    QDir confBaseDir(QString("%1/..").arg(fileName));

    // load module-owned configurations
    for (auto &pair : modSettingsList) {
        const auto mod = pair.first;
        const auto settings = pair.second;
        setStatusText(QStringLiteral("Loading settings for module: %1(%2)").arg(mod->id()).arg(mod->name()));
        if (!mod->loadSettings(confBaseDir.absolutePath(), settings.first, settings.second)) {
            QMessageBox::critical(this, QStringLiteral("Can not load settings"),
                                  QStringLiteral("Unable to load module settings for '%1'.").arg(mod->name()));
            setStatusText("Failed to load settings.");
            return false;
        }
    }

    // create module connections
    setStatusText("Restoring streams and subscriptions...");
    for (auto &pair : jSubInfo) {
        auto mod = pair.first;
        const auto jSubs = pair.second;
        for (const QString &iPortId : jSubs.keys()) {
            const auto modPortPair = jSubs.value(iPortId).toList();
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

    // we are ready now
    setCurrentProjectFile(fileName);
    setStatusText("Board successfully loaded from file.");

    // (ask to) select an experimenter, if this board file knows some
    if (!m_experimenterList->isEmpty()) {
        if (m_experimenterList->rowCount() == 1) {
            changeExperimenter(m_experimenterList->person(0));
        } else {
            // we have many people registered for this board, ask user to choose one!
            showExperimenterSelector(QStringLiteral("Welcome to this experiment!\n"
                                                    "Please select your name from the list - in case you can't find it,\n"
                                                    "you may select \"[Not selected]\" to select no experimenter."));
        }

        if (m_engine->experimenter().isValid())
            setStatusText(QStringLiteral("Welcome %1! - Board successfully loaded.").arg(m_engine->experimenter().name));
    }

    return true;
}

void MainWindow::setDataExportBaseDir(const QString& dir)
{
    if (dir.isEmpty())
        return;

    m_engine->setExportBaseDir(dir);
    updateExportDirDisplay();
}

void MainWindow::openDataExportDirectory()
{
    auto dir = QFileDialog::getExistingDirectory(this,
                                                 "Select Directory",
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 QFileDialog::ShowDirsOnly);
    setDataExportBaseDir(dir);
}

void MainWindow::showExperimenterSelector(const QString &message)
{
    auto items = m_experimenterList->toStringList();
    items.append(QStringLiteral("[Not selected]"));
    bool ok;
    const auto selection = QInputDialog::getItem(this,
                                                 QStringLiteral("Select an experimenter"),
                                                 message,
                                                 items,
                                                 0, false, &ok);
    if (!ok)
        return;
    const auto idx = items.indexOf(selection);
    if ((idx < 0) || (idx >= m_experimenterList->rowCount()))
        changeExperimenter(EDLAuthor());
    else
        changeExperimenter(m_experimenterList->person(idx));
}

void MainWindow::changeExperimenter(const EDLAuthor &person)
{
    m_engine->setExperimenter(person);
    ui->currentExperimenterLabel->setText(person.isValid()? person.name : QStringLiteral("[Person not set]"));
}

void MainWindow::changeTestSubject(const TestSubject &subject)
{
    m_engine->setTestSubject(subject);
    updateExportDirDisplay();
}

void MainWindow::changeExperimentId(const QString& text)
{
    m_engine->setExperimentId(text);
    updateExportDirDisplay();
}

void MainWindow::shutdown()
{
    // The Spinnaker module crashes in an assertion in proprietary code if the application shuts down when
    // Spinnakers "System" singleton is also destroyed.
    // This does not happen on regular module destruction, for some reason. That and the fact that it  looks
    // nicer UI wise to remove modules early, is why modules are destroyed a bit earlier and explicitly here.
    m_engine->removeAllModules();

    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    setStatusText("Shutting down...");
    QApplication::processEvents();

    if (m_engine->isActive()) {
        connect(m_engine, &Engine::runStopped, this, &MainWindow::shutdown);
        stopActionTriggered();
        event->accept();
    } else {
        shutdown();
    }

    // save main window geometry to global config
    m_gconf->setMainWinGeometry(saveGeometry());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_busyIndicator->move(ui->stackedWidget->width() - m_busyIndicator->width() - 4, ui->menuBar->height() + 4);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    m_busyIndicator->move(ui->stackedWidget->width() - m_busyIndicator->width() - 4, ui->menuBar->height() + 4);
}

void MainWindow::projectSaveAsActionTriggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Select Configuration Filename"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("Syntalos Configuration Files (*.syct)"));

    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".syct"))
        fileName = QStringLiteral("%1.syct").arg(fileName);

    showBusyIndicatorProcessing();
    if (!saveConfiguration(fileName)) {
        QMessageBox::critical(this,
                              QStringLiteral("Can not save configuration"),
                              QStringLiteral("Unable to write configuration file to disk."));
    }
    hideBusyIndicator();
}

void MainWindow::projectSaveActionTriggered()
{
    if (m_currentProjectFname.isEmpty()) {
        // we have no project opened, so we just trigger a dialog instead for the user to pick a filename
        projectSaveAsActionTriggered();
        return;
    }

    // NOTE: We *intentionally* make a deep copy of m_currentProjectFname here,
    // to separate it from the currently in-use m_currentProjectFname that may be
    // changed to an empty string in the saveConfiguration() function (affecting
    // its reference which causes unwanted sideeffects without a full copy)

    showBusyIndicatorProcessing();
    if (!saveConfiguration(QString(m_currentProjectFname))) {
        QMessageBox::critical(this,
                              QStringLiteral("Can not save configuration"),
                              QStringLiteral("Unable to write configuration file to disk."));
    }
    hideBusyIndicator();
}

void MainWindow::projectOpenActionTriggered()
{
    auto fileName = QFileDialog::getOpenFileName(this,
                                                 tr("Select Project Filename"),
                                                 QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                 tr("Syntalos Project Files (*.syct)"));
    if (fileName.isEmpty())
        return;

    setStatusText("Loading settings...");

    // prevent any start/stop/modify action while loading the board
    ui->centralWidget->setEnabled(false);
    ui->menuBar->setEnabled(false);
    ui->mainToolBar->setEnabled(false);
    ui->projectToolBar->setEnabled(false);
;
    showBusyIndicatorProcessing();
    if (!loadConfiguration(fileName)) {
        QMessageBox::critical(this,
                              QStringLiteral("Can not load configuration"),
                              QStringLiteral("Failed to load configuration."));
        m_engine->removeAllModules();
    }
    hideBusyIndicator();

    // we should be permitted to run and modify things
    ui->centralWidget->setEnabled(true);
    ui->menuBar->setEnabled(true);
    ui->mainToolBar->setEnabled(true);
    ui->projectToolBar->setEnabled(true);
}

void MainWindow::on_actionProjectDetails_toggled(bool arg1)
{
    if (arg1)
        ui->stackedWidget->setCurrentWidget(ui->pageProject);
    else
        ui->stackedWidget->setCurrentWidget(ui->pageMain);
}

void MainWindow::globalConfigActionTriggered()
{
    GlobalConfigDialog gcDlg;
    gcDlg.exec();
}

void MainWindow::setCurrentProjectFile(const QString &fileName)
{
    if (fileName.isEmpty()) {
        this->setWindowTitle(QStringLiteral("Syntalos"));
        if (!m_currentProjectFname.isEmpty())
            qDebug().noquote() << "Current board settings file reset.";
        m_currentProjectFname = fileName;
    } else {
        m_currentProjectFname = fileName;
        QFileInfo fi(fileName);
        this->setWindowTitle(QStringLiteral("Syntalos - %2").arg(fi.baseName()));
        qDebug().noquote() << "Current board settings file:" << m_currentProjectFname;
    }
}

void MainWindow::updateExportDirDisplay()
{
    if (!m_engine->exportDirIsValid())
        return;

    ui->exportBaseDirLabel->setText(m_engine->exportBaseDir());

    auto font = ui->exportBaseDirLabel->font();
    font.setBold(false);
    auto palette = ui->exportBaseDirLabel->palette();
    palette.setColor(QPalette::WindowText, ui->dataLocationLabel->palette().color(QPalette::WindowText));
    if (m_engine->exportDirIsTempDir()) {
        font.setBold(true);
        palette.setColor(QPalette::WindowText, syColorDangerHigh());
    }
    ui->exportBaseDirLabel->setPalette(palette);
    ui->exportBaseDirLabel->setFont(font);

    // we can run as soon as we have a valid base directory
    if (!m_engine->isRunning())
        setRunPossible(true);

    palette = ui->exportDirLabel->palette();
    palette.setColor(QPalette::WindowText, ui->dataLocationLabel->palette().color(QPalette::WindowText));
    if (m_engine->exportDirIsTempDir())
        palette.setColor(QPalette::WindowText, syColorDangerHigh());
    ui->exportDirLabel->setPalette(palette);
    ui->exportDirLabel->setText(m_engine->exportDir());
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

void MainWindow::onElapsedTimeUpdate()
{
    ui->elapsedTimeLabel->setText(QTime::fromMSecsSinceStartOfDay(m_engine->currentRunElapsedTime().count()).toString("hh:mm:ss"));
}

void MainWindow::setStatusText(const QString& msg)
{
    m_statusBarLabel->setText(msg);
    QApplication::processEvents();
}

void MainWindow::moduleErrorReceived(AbstractModule *, const QString&)
{
    setStopPossible(false);
}

void MainWindow::onEnginePreRunStart()
{
    m_rtElapsedTimer->start();
    showBusyIndicatorRunning();
    m_timingsDialog->clear();

    ui->diskSpaceWarnWidget->setVisible(false);
    ui->memoryWarnWidget->setVisible(false);
    ui->cpuWarnWidget->setVisible(false);
}

void MainWindow::onEngineStopped()
{
    m_rtElapsedTimer->stop();
    hideBusyIndicator();
    setRunPossible(true);
    setStopPossible(false);

    ui->diskSpaceWarnWidget->setVisible(false);
    ui->memoryWarnWidget->setVisible(false);
    ui->cpuWarnWidget->setVisible(false);
}

void MainWindow::onEngineResourceWarning(Engine::SystemResource kind, bool resolved, const QString &message)
{
    switch (kind) {
    case Engine::StorageSpace:
        ui->diskSpaceWarningLabel->setText(message);
        ui->diskSpaceWarnWidget->setVisible(!resolved);
        break;
    case Engine::Memory:
        ui->memoryWarningLabel->setText(message);
        ui->memoryWarnWidget->setVisible(!resolved);
        break;
    case Engine::CpuCores:
        ui->cpuWarningLabel->setText(message);
        ui->cpuWarnWidget->setVisible(!resolved);
        break;
    case Engine::StreamBuffers:
        ui->memoryWarningLabel->setText(message);
        ui->memoryWarnWidget->setVisible(!resolved);
        break;
    }
}

void MainWindow::statusMessageChanged(const QString &message)
{
    setStatusText(message);
}

void MainWindow::showBusyIndicatorProcessing()
{
    m_busyIndicator->load(QStringLiteral(":/animations/busy.svg"));
    m_busyIndicator->show();
    QApplication::processEvents();
}

void MainWindow::showBusyIndicatorRunning()
{
    m_busyIndicator->load(QStringLiteral(":/animations/running.svg"));
    m_busyIndicator->show();
    QApplication::processEvents();
}

void MainWindow::hideBusyIndicator()
{
    m_busyIndicator->load(QByteArray());
    m_busyIndicator->hide();
    QApplication::processEvents();
}

void MainWindow::on_actionSubjectsLoad_triggered()
{
    QString fileName;
    fileName = QFileDialog::getOpenFileName(this,
                                            QStringLiteral("Open Animal List"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            QStringLiteral("TOML Markup Files (*.toml)\nAll Files (*)"));
    if (fileName.isEmpty())
        return;


    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    const auto bytes = f.readAll();
    if (bytes != nullptr) {
        QString parseError;
        const auto var = parseTomlData(bytes, parseError);
        if (parseError.isEmpty())
            m_subjectList->fromVariantHash(var);
        else
            QMessageBox::critical(this,
                                  QStringLiteral("Unable to parse subjects list"),
                                  QStringLiteral("Unable to load subjects list: %1").arg(parseError));
    }
}

void MainWindow::on_actionSubjectsSave_triggered()
{
    QString fileName;
    fileName = QFileDialog::getSaveFileName(this,
                                            tr("Save Animal List"),
                                            QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                            tr("TOML Markup Files (*.toml)"));

    if (fileName.isEmpty())
        return;
    if (!fileName.endsWith(".toml"))
        fileName = QStringLiteral("%1.toml").arg(fileName);

    QFile f(fileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&f);
    out << qVariantHashToTomlData(m_subjectList->toVariantHash());
}

void MainWindow::on_actionTimings_triggered()
{
    m_timingsDialog->show();
}

void MainWindow::on_actionSystemInfo_triggered()
{
    SysInfoDialog sysInfoDlg(m_engine->sysInfo(), this);
    sysInfoDlg.exec();
}

void MainWindow::on_actionModuleLoadInfo_triggered()
{
    QDialog dlg;
    QHBoxLayout layout;
    QTextEdit logBox;
    layout.addWidget(&logBox);
    dlg.setLayout(&layout);
    auto logText = m_engine->library()->issueLogHtml();
    if (logText.isEmpty())
        logText = QStringLiteral("No issues reported.");
    logBox.setWordWrapMode(QTextOption::NoWrap);
    logBox.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    logBox.setText(QStringLiteral("<html>") + logText);
    dlg.setWindowTitle(QStringLiteral("Dynamic module loader log"));
    dlg.resize(620, 400);
    dlg.exec();
}
