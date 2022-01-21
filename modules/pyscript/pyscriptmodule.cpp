/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include <QMessageBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QTextBrowser>
#include <QDebug>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMetaType>
#include <QSplitter>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <QShortcut>

#include "oop/oopmodule.h"
#include "utils/style.h"
#include "porteditordialog.h"

SYNTALOS_MODULE(PyScriptModule);

class PyScriptModule : public OOPModule
{
    Q_OBJECT
public:

    explicit PyScriptModule(QObject *parent = nullptr)
        : OOPModule(parent),
          m_scriptWindow(nullptr)
    {
        // we use the generic Python OOP worker process for this
        setWorkerBinaryPyWorker();

        // set up code editor
        auto editor = KTextEditor::Editor::instance();
        // create a new document
        auto pyDoc = editor->createDocument(this);

        QFile samplePyRc(QStringLiteral(":/texts/pyscript-sample.py"));
        if(samplePyRc.open(QIODevice::ReadOnly)) {
            pyDoc->setText(samplePyRc.readAll());
        }
        samplePyRc.close();

        m_scriptWindow = new QWidget;
        addDisplayWindow(m_scriptWindow);

        m_scriptWindow->setWindowIcon(QIcon(":/icons/generic-config"));
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));

        m_scriptView = pyDoc->createView(m_scriptWindow);
        pyDoc->setHighlightingMode("python");

        // add console output widget
        m_pyconsoleWidget = new QTextBrowser(m_scriptWindow);
        m_pyconsoleWidget->setFontFamily(QStringLiteral("Monospace"));
        m_pyconsoleWidget->setFontPointSize(10);

        m_pyconsoleWidget->setTextColor(SyColorWhite);
        auto pal = m_pyconsoleWidget->palette();
        pal.setColor(QPalette::Base, SyColorDark);
        m_pyconsoleWidget->setPalette(pal);

        auto splitter = new QSplitter(Qt::Vertical, m_scriptWindow);
        splitter->addWidget(m_scriptView);
        splitter->addWidget(m_pyconsoleWidget);
        splitter->setStretchFactor(0, 8);
        splitter->setStretchFactor(1, 1);
        auto scriptLayout = new QHBoxLayout(m_scriptWindow);
        m_scriptWindow->setLayout(scriptLayout);
        scriptLayout->setMargin(2);
        scriptLayout->addWidget(splitter);

        // add ports dialog
        auto menuBar = new QMenuBar();
        auto portsMenu = new QMenu("Ports");
        menuBar->addMenu(portsMenu);
        auto portEditAction = portsMenu->addAction("Edit");
        m_scriptWindow->layout()->setMenuBar(menuBar);
        m_scriptWindow->resize(720, 800);

        m_portsDialog = new PortEditorDialog(this, m_scriptWindow);

        // connect UI events
        setCaptureStdout(true);
        connect(this, &OOPModule::processStdoutReceived, this, [&](const QString& data) {
            m_pyconsoleWidget->append(data);
        });

        connect(portEditAction, &QAction::triggered, this, [&](bool) {
            m_portsDialog->exec();
        });

        // FIXME: Dirty hack: This introduces a shortcut conflict between the KTextEditor-registered one
        // and this one. Ideally hitting Ctrl+S would save the Syntalos board, but instead it triggers
        // the KTextEditor save dialog, which confused some users. Now, we show an error instead, which
        // is also awful, but at least never leads to users doing the wrong thing. Long-term this needs
        // a proper fix (if KTextEditor doesn't have the needed API, I should contribute it...).
        auto shortcut = new QShortcut(QKeySequence(tr("Ctrl+S", "File|Save")),
                                 m_scriptWindow);
        connect(shortcut, &QShortcut::activated, [=]() {});
    }

    ~PyScriptModule() override
    {}

    void setName(const QString &value) override
    {
        OOPModule::setName(value);
        m_scriptWindow->setWindowTitle(QStringLiteral("%1 - Editor").arg(name()));
    }

    bool initialize() override
    {
        if (workerBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is MazeAmaze installed correctly?");
            return false;
        }

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        m_pyconsoleWidget->clear();
        setPythonScript(m_scriptView->document()->text());

        return OOPModule::prepare(testSubject);
    }

    void serializeSettings(const QString&, QVariantHash &settings, QByteArray &extraData) override
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

    bool loadSettings(const QString&, const QVariantHash &settings, const QByteArray &extraData) override
    {
        m_scriptView->document()->setText(QString::fromUtf8(extraData));

        const auto varInPorts = settings.value("ports_in").toList();
        const auto varOutPorts = settings.value("ports_out").toList();

        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            registerInputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                      po.value("id").toString(),
                                      po.value("title").toString());
        }

        for (const auto &pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                       po.value("id").toString(),
                                       po.value("title").toString());
        }

        // update port listing in UI
        m_portsDialog->updatePortLists();

        return true;
    }

private:
    QTextBrowser *m_pyconsoleWidget;
    KTextEditor::View *m_scriptView;
    PortEditorDialog *m_portsDialog;

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
    return QStringLiteral("Control certain aspects of MazeAmaze (most notably Firmata I/O) using a Python script.");
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
