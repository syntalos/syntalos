/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "pyscriptmodule.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <KActionCollection>
#pragma GCC diagnostic pop
#include <qtermwidget6/qtermwidget.h>
#include <QCoreApplication>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMetaType>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QToolBar>
#include <iostream>

#include "mlinkmodule.h"
#include "porteditordialog.h"
#include "globalconfig.h"
#include "pyvenvmanager.h"
#include "utils/style.h"

SYNTALOS_MODULE(PyScriptModule);

class PyScriptModule : public MLinkModule
{
    Q_OBJECT
public:
    explicit PyScriptModule(QObject *parent = nullptr)
        : MLinkModule(parent),
          m_scriptWindow(nullptr)
    {
        GlobalConfig gconf;

        // script modules are transient
        setWorkerMode(ModuleWorkerMode::TRANSIENT);

        // set up code editor
        auto editor = KTextEditor::Editor::instance();
        // create a new document
        auto pyDoc = editor->createDocument(this);

        QFile samplePyRc(QStringLiteral(":/texts/pyscript-sample.py"));
        if (samplePyRc.open(QIODevice::ReadOnly)) {
            pyDoc->setText(samplePyRc.readAll());
        }
        samplePyRc.close();

        m_scriptWindow = new QWidget;
        addSettingsWindow(m_scriptWindow);

        m_scriptWindow->setWindowIcon(QIcon(":/icons/generic-config"));
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));

        m_scriptView = pyDoc->createView(m_scriptWindow);
        pyDoc->setMode(QStringLiteral("Python"));

        // create main toolbar
        auto toolbar = new QToolBar(m_scriptWindow);
        toolbar->setMovable(false);
        toolbar->layout()->setContentsMargins(2, 2, 2, 2);
        m_scriptWindow->resize(720, 800);
        m_portEditAction = toolbar->addAction("Edit Ports");
        setWidgetIconFromResource(m_portEditAction, "edit-ports");

        auto spacer = new QWidget(toolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);

        // add console output widget
        m_pyconsoleWidget = new QTermWidget(0, m_scriptWindow);
        m_pyconsoleWidget->setShellProgram(nullptr);
        m_pyconsoleWidget->setFlowControlEnabled(false);

        auto pal = m_pyconsoleWidget->palette();
        pal.setColor(QPalette::Text, SyColorWhite);
        pal.setColor(QPalette::Base, SyColorDark);
        m_pyconsoleWidget->setPalette(pal);

        auto splitter = new QSplitter(Qt::Vertical, m_scriptWindow);
        splitter->addWidget(m_scriptView);
        splitter->addWidget(m_pyconsoleWidget);
        splitter->setStretchFactor(0, 8);
        splitter->setStretchFactor(1, 5);
        auto scriptLayout = new QVBoxLayout(m_scriptWindow);
        m_scriptWindow->setLayout(scriptLayout);
        scriptLayout->setContentsMargins(0, 0, 0, 0);
        scriptLayout->addWidget(toolbar);
        scriptLayout->addWidget(splitter);

        // add ports dialog
        m_portsDialog = new PortEditorDialog(this, m_scriptWindow);

        // connect UI events
        setOutputCaptured(true);
        connect(this, &MLinkModule::processOutputReceived, this, [this](OutChannelType, const QString &data) {
            m_pyconsoleWidget->sendText(data);
        });

        connect(m_portEditAction, &QAction::triggered, this, [&](bool) {
            m_portsDialog->exec();
        });

        // add menu
        auto menuButton = new QToolButton(toolbar);
        menuButton->setIcon(QIcon::fromTheme("application-menu"));
        menuButton->setPopupMode(QToolButton::InstantPopup);
        auto actionsMenu = new QMenu(m_scriptWindow);

        m_runInGdbAction = actionsMenu->addAction("Run under GDB");
        m_runInGdbAction->setCheckable(true);
        connect(m_runInGdbAction, &QAction::toggled, this, [this](bool enabled) {
            setPyWorkerBinary(enabled);
        });
        if (!gconf.showDevelModules())
            m_runInGdbAction->setVisible(false);

        auto docHelpAction = actionsMenu->addAction("Documentation");
        auto apiHelpAction = actionsMenu->addAction("MLink API Reference");

        menuButton->setMenu(actionsMenu);
        toolbar->addWidget(menuButton);

        connect(docHelpAction, &QAction::triggered, this, [&](bool) {
            QDesktopServices::openUrl(QUrl("https://syntalos.org/docs/modules/pyscript/", QUrl::TolerantMode));
        });
        connect(apiHelpAction, &QAction::triggered, this, [&](bool) {
            QDesktopServices::openUrl(QUrl("https://syntalos.org/docs/pysy-mlink-api/", QUrl::TolerantMode));
        });

        // Don't trigger the text editor document save dialog
        // TODO: Maybe we should save the Syntalos board here instead?
        auto actionCollection = m_scriptView->actionCollection();
        if (actionCollection) {
            auto saveAction = actionCollection->action("file_save");
            if (saveAction) {
                // avoid KTextEditor's default save dialog in the embedded editor
                saveAction->setEnabled(false);
            }
        }
    }

    ~PyScriptModule() override
    {
        delete m_scriptView;
    }

    ModuleFeatures features() const final
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    QString findPyWorkerBinary()
    {
        QString workerBinary = moduleRootDir() + "/worker/pyworker";
        QFileInfo fi(workerBinary);
        if (!fi.exists())
            workerBinary = moduleRootDir() + "/pyworker";
        return workerBinary;
    }

    void setName(const QString &value) final
    {
        MLinkModule::setName(value);
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));
    }

    bool initialize() override
    {
        // id() is not set during construction, so resolve the worker binary here
        // when the module identity is available.
        setPyWorkerBinary(m_runInGdbAction->isChecked());

        if (moduleBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is Syntalos installed correctly?");
            return false;
        }

        if (!ensureBaseVirtualEnv())
            return false;

        setInitialized();
        return true;
    }

    void showSettingsUi() final
    {
        m_scriptWindow->setWindowTitle(name());

        // set an initial position if we do not have one yet
        if (m_scriptWindow->pos().isNull()) {
            auto pos = QCursor::pos();
            pos.setY(pos.y() - (m_scriptWindow->height() / 2));
            m_scriptWindow->move(pos);
        }

        m_scriptWindow->show();
        m_scriptWindow->raise();
        m_scriptWindow->activateWindow();
    }

    bool prepare(const TestSubject &testSubject) override
    {
        if (!ensureAndSetBaseVirtualEnv())
            return false;

        m_portEditAction->setEnabled(false);
        m_pyconsoleWidget->clear();
        setScript(m_scriptView->document()->text());

        return MLinkModule::prepare(testSubject);
    }

    void stop() override
    {
        MLinkModule::stop();
        terminateProcess();
        m_portEditAction->setEnabled(true);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &extraData) override
    {
        extraData = m_scriptView->document()->text().toUtf8();

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
        if (m_runInGdbAction->isChecked())
            settings.insert("run_in_debugger", true);
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &extraData) override
    {
        m_scriptView->document()->setText(QString::fromUtf8(extraData));

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

        if (settings.value("run_in_debugger", false).toBool())
            setPyWorkerBinary(true);

        return true;
    }

private:
    void setPyWorkerBinary(bool runInDebugger)
    {
        disconnect(m_outFwdConn);

        // Ensure pyworker can import syntalos_mlink with deterministic precedence:
        // build-tree first, installed fallback second, inherited paths last.
        auto penv = QProcessEnvironment::systemEnvironment();
        QStringList pythonPath = findSyntalosMlinkPyModulePaths();
        const QString existingPP = penv.value(QStringLiteral("PYTHONPATH"));
        if (!existingPP.isEmpty())
            pythonPath << existingPP.split(':', Qt::SkipEmptyParts);
        pythonPath.removeDuplicates();
        if (!pythonPath.isEmpty()) {
            penv.insert(QStringLiteral("PYTHONPATH"), pythonPath.join(':'));
        }

        // switch to unbuffered mode so our parent receives Python output
        // (e.g. from print() & Co.) faster.
        penv.insert(QStringLiteral("PYTHONUNBUFFERED"), "1");
        setModuleBinaryEnv(penv);

        const auto pyWorkerBinary = findPyWorkerBinary();
        if (!runInDebugger) {
            setModuleBinary(pyWorkerBinary);
            setModuleBinaryArgs(QStringList());

            m_runInGdbAction->blockSignals(true);
            m_runInGdbAction->setChecked(false);
            m_runInGdbAction->blockSignals(false);
            return;
        }

        const auto gdbExe = QStandardPaths::findExecutable(QStringLiteral("gdb"));
        if (gdbExe.isEmpty()) {
            // text warning, to we at least know what happened if this times out on noninteractive CI
            LOG_WARNING(m_log, "The `gdb` debugger binary was not found!");

            // user warning
            QMessageBox::warning(
                m_scriptWindow,
                QStringLiteral("GNU Debugger not found"),
                QStringLiteral("Unable to run under GDB because the `gdb` executable could not be found."));

            m_runInGdbAction->blockSignals(true);
            m_runInGdbAction->setChecked(false);
            m_runInGdbAction->blockSignals(false);
            return;
        }

        setModuleBinary(gdbExe);
        setModuleBinaryArgs(
            QStringList() << QStringLiteral("-q") << QStringLiteral("-batch") << QStringLiteral("-ex")
                          << QStringLiteral("run") << QStringLiteral("-ex") << QStringLiteral("bt")
                          << QStringLiteral("--args") << pyWorkerBinary);
        m_runInGdbAction->blockSignals(true);
        m_runInGdbAction->setChecked(true);
        m_runInGdbAction->blockSignals(false);

        // we also always forward output to the log when running under gdb
        m_outFwdConn = connect(
            this, &MLinkModule::processOutputReceived, this, [this](OutChannelType channel, const QString &data) {
                if (channel == OutChannelType::ChannelStdout)
                    LOG_RUNTIME_METADATA(m_log, quill::LogLevel::Info, "stdout-gdb", 0, "", "{}", data);
                else
                    LOG_RUNTIME_METADATA(m_log, quill::LogLevel::Warning, "stderr-gdb", 0, "", "{}", data);
            });
    }

    /**
     * Quicker function that just ensures that the base venv exists at all,
     * if we are configured to use it.
     */
    bool ensureAndSetBaseVirtualEnv()
    {
        GlobalConfig gconf;
        if (!gconf.useVenvForPyScript()) {
            setPythonVirtualEnv(QString());
            return true;
        }

        const auto venvDir = pythonVEnvDirForName("base");
        setPythonVirtualEnv(venvDir);
        if (QDir(venvDir).exists())
            return true;

        return ensureBaseVirtualEnv();
    }

    /**
     * Make sure the base virtualenv is set up properly, if we are using one.
     */
    bool ensureBaseVirtualEnv()
    {
        GlobalConfig gconf;
        if (!gconf.useVenvForPyScript()) {
            setPythonVirtualEnv(QString());
            return true;
        }

        const auto venvDir = pythonVEnvDirForName("base");
        setPythonVirtualEnv(venvDir);
        const auto venvStatus = pythonVirtualEnvStatus(QStringLiteral("base"));
        if (venvStatus == PyVirtualEnvStatus::VALID)
            return true;

        if (venvStatus == PyVirtualEnvStatus::MISSING) {
            const auto reply = QMessageBox::question(
                nullptr,
                QStringLiteral("Create shared Python virtual environment?"),
                QStringLiteral(
                    "Python Script modules are configured to use a shared virtual environment, but the base "
                    "environment does not exist yet.\n\n"
                    "Should Syntalos create it now?"),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;

            processUiEvents();
            const auto result = createPythonVirtualEnv(QStringLiteral("base"));
            if (!result.has_value()) {
                QMessageBox::warning(
                    nullptr,
                    QStringLiteral("Failed to create shared Python virtual environment"),
                    QStringLiteral("Failed to set up the shared Python virtual environment: %1").arg(result.error()));
                return false;
            }
        } else if (venvStatus == PyVirtualEnvStatus::INTERPRETER_MISSING) {
            QMessageBox::information(
                nullptr,
                QStringLiteral("Recreating shared Python virtual environment"),
                QStringLiteral(
                    "The Python interpreter used to create the shared virtual environment is no longer "
                    "available. Syntalos must recreate the environment before Python Script modules can run."));

            processUiEvents();
            const auto result = createPythonVirtualEnv(QStringLiteral("base"), QString(), true);
            if (!result.has_value()) {
                QMessageBox::warning(
                    nullptr,
                    QStringLiteral("Failed to create shared Python virtual environment"),
                    QStringLiteral("Failed to set up the shared Python virtual environment: %1").arg(result.error()));
                return false;
            }
        }

        return true;
    }

    QTermWidget *m_pyconsoleWidget;
    KTextEditor::View *m_scriptView;
    PortEditorDialog *m_portsDialog;
    QAction *m_portEditAction;
    QAction *m_runInGdbAction;
    QMetaObject::Connection m_outFwdConn;

    QWidget *m_scriptWindow;
};

QString PyScriptModuleInfo::id() const
{
    return QStringLiteral("pyscript");
}

QString PyScriptModuleInfo::name() const
{
    return QStringLiteral("Python Script");
}

QString PyScriptModuleInfo::description() const
{
    return QStringLiteral("Write custom Python code to control other modules and experiment behavior.");
}

ModuleCategories PyScriptModuleInfo::categories() const
{
    return ModuleCategory::SCRIPTING;
}

QIcon PyScriptModuleInfo::icon() const
{
    return QIcon(":/module/python");
}

QColor PyScriptModuleInfo::color() const
{
    return qRgba(252, 220, 149, 255);
}

AbstractModule *PyScriptModuleInfo::createModule(QObject *parent)
{
    return new PyScriptModule(parent);
}

#include "pyscriptmodule.moc"
