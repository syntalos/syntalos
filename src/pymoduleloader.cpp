/*
 * Copyright (C) 2020-2021 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pymoduleloader.h"

#include <iostream>
#include <QDir>
#include <QIcon>

#include "oopmodule.h"

class PythonModule : public OOPModule
{
    Q_OBJECT
public:

    explicit PythonModule(QObject *parent = nullptr)
        : OOPModule(parent)
    {
        // we use the generic Python OOP worker process for this
        setWorkerBinaryPyWorker();

        setCaptureStdout(true);
        connect(this, &OOPModule::processStdoutReceived, this, [&](const QString& data) {
            std::cout << "py." << id().toStdString() << "(" << name().toStdString() << "): "
                      << data.toStdString() << std::endl;
        });
    }

    ~PythonModule() override
    {}

    void setupPorts(const QVariantList &varInPorts, const QVariantList &varOutPorts)
    {
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
    }

    bool initialize() override
    {
        if (workerBinary().isEmpty()) {
            raiseError("Unable to find Python worker binary. Is Syntalos installed correctly?");
            return false;
        }

        setInitialized();
        return true;
    }

    bool prepare(const TestSubject &testSubject) override
    {
        loadPythonFile(m_mainPyFname, m_pyVEnv, m_pyVEnv);
        return OOPModule::prepare(testSubject);
    }

    void serializeSettings(const QString&, QVariantHash &settings, QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        Q_UNUSED(extraData)
    }

    bool loadSettings(const QString&, const QVariantHash &settings, const QByteArray &extraData) override
    {
        Q_UNUSED(settings)
        Q_UNUSED(extraData)
        return true;
    }

    void setMainPyFname(const QString &fname)
    {
        m_mainPyFname = fname;
    }

private:
    QString m_mainPyFname;
    QString m_pyVEnv;
};

class PyModuleInfo : public ModuleInfo
{
public:
    explicit PyModuleInfo(const QString &id, const QString &name,
                          const QString &description, const QPixmap &icon)
        : m_id(id),
          m_name(name),
          m_description(description),
          m_icon (icon) {}

    ~PyModuleInfo() override {}

    QString id() const override { return m_id; }
    QString name() const override { return m_name; };
    QString description() const override { return m_description; };
    QPixmap pixmap() const override { return m_icon; };
    AbstractModule *createModule(QObject *parent = nullptr) override
    {
        auto mod = new PythonModule(parent);
        mod->setMainPyFname(m_pyFname);
        mod->setupPorts(m_portDefInput, m_portDefOutput);
        return mod;
    }

    void setMainPyScriptFname(const QString &pyFname) { m_pyFname = pyFname; }
    void setPortDef(const QVariantList &defInput, const QVariantList &defOutput)
    {
        m_portDefInput = defInput;
        m_portDefOutput = defOutput;
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QPixmap m_icon;

    QString m_pyFname;
    QVariantList m_portDefInput;
    QVariantList m_portDefOutput;
};

ModuleInfo *loadPythonModuleInfo(const QString &modId, const QString &modDir, const QVariantHash &modData)
{
    PyModuleInfo *modInfo = nullptr;
    const auto modDef = modData["syntalos_module"].toHash();
    const auto name = modDef.value("name").toString();
    const auto desc = modDef.value("description").toString();
    const auto iconName = modDef.value("icon").toString();

    if (name.isEmpty())
        throw std::runtime_error("Required 'name' key not found in module metadata.");
    if (desc.isEmpty())
        throw std::runtime_error("Required 'description' key not found in module metadata.");

    QPixmap icon = QPixmap(":/module/generic");
    if (!iconName.isEmpty()) {
        if (!icon.load(QDir(modDir).filePath(iconName)))
            icon = QIcon::fromTheme(iconName, QIcon(":/module/generic")).pixmap(64, 64);
    }

    const auto pyFile = QDir(modDir).filePath(modDef.value("main").toString());
    if (!QFileInfo::exists(pyFile))
        throw std::runtime_error(qPrintable(QStringLiteral("Main entrypoint Python file %1 does not exist").arg(pyFile)));

    modInfo = new PyModuleInfo(modId, name, desc, icon);
    modInfo->setMainPyScriptFname(pyFile);

    const auto portsDef = modData.value("ports").toHash();
    if (!portsDef.isEmpty())
        modInfo->setPortDef(portsDef.value("in").toList(),
                            portsDef.value("out").toList());

    return modInfo;
}

#include "pymoduleloader.moc"
