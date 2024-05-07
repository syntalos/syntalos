/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include <qtermwidget5/qtermwidget.h>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDebug>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMetaType>
#include <QProcess>
#include <QShortcut>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolBar>

#include "mlinkmodule.h"
#include "porteditordialog.h"
#include "globalconfig.h"
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
        // we use the generic Python OOP worker process for this
        setModuleBinary(findSyntalosPyWorkerBinary());

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
        addDisplayWindow(m_scriptWindow);

        m_scriptWindow->setWindowIcon(QIcon(":/icons/generic-config"));
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));

        m_scriptView = pyDoc->createView(m_scriptWindow);
        pyDoc->setHighlightingMode("python");

        // create main toolbar
        auto toolbar = new QToolBar(m_scriptWindow);
        toolbar->setMovable(false);
        toolbar->layout()->setMargin(2);
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
        scriptLayout->setMargin(0);
        scriptLayout->addWidget(toolbar);
        scriptLayout->addWidget(splitter);

        // add ports dialog
        m_portsDialog = new PortEditorDialog(this, m_scriptWindow);

        // connect UI events
        setOutputCaptured(true);
        connect(this, &MLinkModule::processOutputReceived, this, [&](const QString &data) {
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
        auto docHelpAction = actionsMenu->addAction("Documentation");
        auto apiHelpAction = actionsMenu->addAction("MLink API Reference");

        menuButton->setMenu(actionsMenu);
        toolbar->addWidget(menuButton);

        connect(docHelpAction, &QAction::triggered, this, [&](bool) {
            QDesktopServices::openUrl(
                QUrl("https://syntalos.readthedocs.io/latest/modules/pyscript.html", QUrl::TolerantMode));
        });
        connect(apiHelpAction, &QAction::triggered, this, [&](bool) {
            QDesktopServices::openUrl(
                QUrl("https://syntalos.readthedocs.io/latest/pysy-mlink-api.html", QUrl::TolerantMode));
        });

        // Don't trigger the text editor document save dialog
        // TODO: Maybe we should save the Syntalos board here instead?
        auto actionCollection = m_scriptView->actionCollection();
        if (actionCollection) {
            auto saveAction = actionCollection->action("file_save");
            if (saveAction) {
                // Remove default connections to disable default save behavior
                disconnect(saveAction, nullptr, nullptr, nullptr);
            }
        }
    }

    ~PyScriptModule() override {}

    void setName(const QString &value) override
    {
        MLinkModule::setName(value);
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));
    }

    bool initialize() override
    {
        if (moduleBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is Syntalos installed correctly?");
            return false;
        }

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
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

        return true;
    }

private:
    QTermWidget *m_pyconsoleWidget;
    KTextEditor::View *m_scriptView;
    PortEditorDialog *m_portsDialog;
    QAction *m_portEditAction;

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
