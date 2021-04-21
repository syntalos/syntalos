/*
 * Copyright (C) 2019-2021 Matthias Klumpp <matthias@tenstral.net>
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

#include "intanrhxmodule.h"

#include <QTimer>

#include "boardselectdialog.h"

SYNTALOS_MODULE(IntanRhxModule)

class IntanRhxModule : public AbstractModule
{
    Q_OBJECT
private:
    BoardSelectDialog *m_boardSelectDlg;
    ControlWindow *m_ctlWindow;
    ControllerInterface *m_controllerIntf;

    QTimer *m_evTimer;

public:
    explicit IntanRhxModule(QObject *parent = nullptr)
        : AbstractModule(parent),
          m_evTimer(new QTimer(this))
    {
        m_boardSelectDlg = new BoardSelectDialog;
        m_ctlWindow = m_boardSelectDlg->getControlWindow();
        if (m_ctlWindow != nullptr)
            m_ctlWindow->hide();
        m_controllerIntf = m_boardSelectDlg->getControllerInterface();

        // set up timer to render Intan RHX UX while running
        m_evTimer->setInterval(0);
        connect(m_evTimer, &QTimer::timeout, this, &IntanRhxModule::runDaqUpdate);
    }

    ~IntanRhxModule()
    {
        delete m_boardSelectDlg;
        delete m_ctlWindow;
    }

    bool initialize() override
    {
        if (m_ctlWindow == nullptr) {
            raiseError(QStringLiteral("No reference to control window found. This is an internal error."));
            return false;
        }
        addDisplayWindow(m_ctlWindow, false);

        return true;
    }

    ModuleFeatures features() const override
    {
        ModuleFeatures flags = ModuleFeature::SHOW_SETTINGS |
                                ModuleFeature::SHOW_DISPLAY |
                                ModuleFeature::CORE_AFFINITY |
                                ModuleFeature::REALTIME;
        return flags;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::NONE;
    }

    bool prepare(const TestSubject&) override
    {
        // the Intan module is a singleton, so we can "grab" this very generic name here
        auto dstore = getOrCreateDefaultDataset(QStringLiteral("intan-signals"));

        // we use the ugly scanning method -for now
        dstore->setDataScanPattern(QStringLiteral("*.rhd"));
        dstore->setAuxDataScanPattern(QStringLiteral("*.tsync"));

        const auto intanBasePart = QStringLiteral("%1_data.rhd").arg(dstore->collectionId().toString(QUuid::WithoutBraces).left(4));
        const auto intanBaseFilename = dstore->pathForDataBasename(intanBasePart);
        if (intanBaseFilename.isEmpty())
            return false;
        m_ctlWindow->setSaveFilenameTemplate(intanBaseFilename);

        // run (but wait for the starting signal)
        m_ctlWindow->recordControllerSlot();
        m_evTimer->start();
        return true;
    }

    void start() override
    {
        AbstractModule::start();
    }

    void stop() override
    {
        m_evTimer->stop();
        m_ctlWindow->stopControllerSlot();
        AbstractModule::stop();
    }

private slots:
    void runDaqUpdate()
    {
        m_controllerIntf->controllerRunIter();
    }

private:

};

QString IntanRhxModuleInfo::id() const
{
    return QStringLiteral("intan-rhx");
}

QString IntanRhxModuleInfo::name() const
{
    return QStringLiteral("Intan RHX");
}

QString IntanRhxModuleInfo::description() const
{
    return QStringLiteral("Record electrophysiological signals from any Intan RHD or RHS system using "
                          "an RHD USB interface board, RHD recording controller, or RHS stim/recording controller.");
}

QString IntanRhxModuleInfo::license() const
{
    return QStringLiteral("Copyright (c) 2020-2021 <a href=\"https://intantech.com/\">Intan Technologies</a> [GPLv3+]");
}

QIcon IntanRhxModuleInfo::icon() const
{
    return QIcon(":/module/intan-rhx");
}

AbstractModule *IntanRhxModuleInfo::createModule(QObject *parent)
{
    return new IntanRhxModule(parent);
}

#include "intanrhxmodule.moc"
