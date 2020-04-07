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
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include "oop/oopmodule.h"
#include "porteditordialog.h"

class PyScriptModule : public OOPModule
{
    Q_OBJECT
public:

    explicit PyScriptModule(QObject *parent = nullptr)
        : OOPModule(parent)
    {
        m_pyoutWindow = nullptr;
        m_scriptWindow = nullptr;

        // we use the generic Python OOP wrker process for this
        setWorkerBinaryPyWorker();

        m_pyoutWindow = new QTextBrowser;
        m_pyoutWindow->setFontFamily(QStringLiteral("Monospace"));
        m_pyoutWindow->setFontPointSize(10);
        m_pyoutWindow->setWindowTitle(QStringLiteral("Python Console Output"));
        m_pyoutWindow->setWindowIcon(QIcon(":/icons/generic-view"));
        m_pyoutWindow->resize(540, 210);
        addDisplayWindow(m_pyoutWindow);

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
        m_scriptWindow->setWindowIcon(QIcon(":/icons/generic-config"));
        m_scriptWindow->setWindowTitle(QStringLiteral("Python Code"));
        auto scriptLayout = new QHBoxLayout(m_scriptWindow);
        m_scriptWindow->setLayout(scriptLayout);
        scriptLayout->setMargin(2);

        auto menuBar = new QMenuBar();
        auto portsMenu = new QMenu("Ports");
        menuBar->addMenu(portsMenu);
        auto portEditAction = portsMenu->addAction("Edit");
        m_scriptWindow->layout()->setMenuBar(menuBar);
        m_scriptWindow->resize(680, 780);

        m_portsDialog = new PortEditorDialog(this, m_scriptWindow);
        addSettingsWindow(m_scriptWindow);

        m_scriptView = pyDoc->createView(m_scriptWindow);
        scriptLayout->addWidget(m_scriptView);
        pyDoc->setHighlightingMode("python");

        setCaptureStdout(true);
        connect(this, &OOPModule::processStdoutReceived, this, [&](const QString& data) {
            m_pyoutWindow->append(data);
        });

        connect(portEditAction, &QAction::triggered, this, [&](bool) {
            m_portsDialog->exec();
        });
    }

    ~PyScriptModule() override
    {}

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
        m_pyoutWindow->clear();
        loadPythonScript(m_scriptView->document()->text());

        return OOPModule::prepare(testSubject);
    }

    void serializeSettings(const QString&, QVariantHash &settings, QByteArray &extraData) override
    {
        extraData = m_scriptView->document()->text().toUtf8();

        QVariantList varInPorts;
        for (const auto port : inPorts()) {
            QVariantHash po;
            po.insert("id", port->id());
            po.insert("title", port->title());
            po.insert("data_type", port->dataTypeName());
            varInPorts.append(po);
        }

        QVariantList varOutPorts;
        for (const auto port : outPorts()) {
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

        for (const auto pv : varInPorts) {
            const auto po = pv.toHash();
            registerInputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                      po.value("id").toString(),
                                      po.value("title").toString());
        }

        for (const auto pv : varOutPorts) {
            const auto po = pv.toHash();
            registerOutputPortByTypeId(QMetaType::type(qPrintable(po.value("data_type").toString())),
                                       po.value("id").toString(),
                                       po.value("title").toString());
        }

        return true;
    }

private:
    QTextBrowser *m_pyoutWindow;
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

QPixmap PyScriptModuleInfo::pixmap() const
{
    return QPixmap(":/module/python");
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
