/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include "oop/oopmodule.h"

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
        m_scriptWindow->resize(680, 780);
        addSettingsWindow(m_scriptWindow);

        m_scriptView = pyDoc->createView(m_scriptWindow);
        scriptLayout->addWidget(m_scriptView);
        pyDoc->setHighlightingMode("python");

        setCaptureStdout(true);
        connect(this, &OOPModule::processStdoutReceived, this, [&](const QString& data) {
            m_pyoutWindow->append(data);
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

    bool prepare(const QString &storageRootDir, const TestSubject &testSubject) override
    {
        m_pyoutWindow->clear();
        loadPythonScript(m_scriptView->document()->text());

        return OOPModule::prepare(storageRootDir, testSubject);
    }

    QByteArray serializeSettings(const QString &) override
    {
        QJsonObject jsettings;
        jsettings.insert("script", m_scriptView->document()->text());

        return jsonObjectToBytes(jsettings);
    }

    bool loadSettings(const QString &, const QByteArray &data) override
    {
        auto jsettings = jsonObjectFromBytes(data);
        m_scriptView->document()->setText(jsettings.value("script").toString());

        return true;
    }

private:
    QTextBrowser *m_pyoutWindow;
    KTextEditor::View *m_scriptView;
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
