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
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "runcmdmodule.h"

#include <QMessageBox>
#include <QProcess>

#include "utils/misc.h"
#include "runcmdsettingsdlg.h"

SYNTALOS_MODULE(RunCmdModule)

class RunCmdModule : public AbstractModule
{
    Q_OBJECT

private:
    RunCmdSettingsDlg *m_settings;
    QProcess *m_proc;
    QProcessEnvironment m_procEnv;
    bool m_startProc;
    bool m_inSandbox;

public:
    explicit RunCmdModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_proc = new QProcess(parent);
        m_settings = new RunCmdSettingsDlg;
        addSettingsWindow(m_settings);

        // register event function to check for the current process every 1.5s
        registerTimedEvent(&RunCmdModule::runEvent, milliseconds_t(1500));

        // if we are in a Flatpak sandbox, we can run a command within it or outside of it
        m_inSandbox = isInFlatpakSandbox();
        m_settings->setSandboxUiVisible(m_inSandbox);
    }

    ~RunCmdModule()
    {}

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_SHARED;
    }

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    static QStringList splitCommandLine(const QString & cmdLine)
    {
        QStringList list;
        if (cmdLine.isEmpty())
            return list;

        QString arg;
        bool escape = false;
        enum { Idle, Arg, QuotedArg } state = Idle;
        foreach (QChar const c, cmdLine) {
            if (!escape && c == '\\') { escape = true; continue; }
            switch (state) {
            case Idle:
                if (!escape && c == '"') state = QuotedArg;
                else if (escape || !c.isSpace()) { arg += c; state = Arg; }
                break;
            case Arg:
                if (!escape && c == '"') state = QuotedArg;
                else if (escape || !c.isSpace()) arg += c;
                else { list << arg; arg.clear(); state = Idle; }
                break;
            case QuotedArg:
                if (!escape && c == '"') state = arg.isEmpty() ? Idle : Arg;
                else arg += c;
                break;
            }
            escape = false;
        }

        if (!arg.isEmpty()) list << arg;
        return list;
    }

    bool prepare(const TestSubject& testSubject) override
    {
        m_procEnv = QProcessEnvironment::systemEnvironment();
        m_procEnv.insert("SY_SUBJECT_ID", testSubject.id);
        m_procEnv.insert("SY_SUBJECT_GROUP", testSubject.group);

        if (m_inSandbox && m_settings->runOnHost()) {
            m_proc->setProgram("flatpak-spawn");
            const auto fpsArgs = QStringList() << "--host" << m_settings->executable();
            m_proc->setArguments(fpsArgs + splitCommandLine(m_settings->parametersStr()));
        } else {
            m_proc->setProgram(m_settings->executable());
            m_proc->setArguments(splitCommandLine(m_settings->parametersStr()));
        }
        m_proc->setProcessChannelMode(QProcess::ForwardedChannels);

        if (m_proc->program().isEmpty()) {
            raiseError(QStringLiteral("No executable is set to be run."));
            return false;
        }

        m_startProc = true;
        setStateReady();
        return true;
    }

    void start() override
    {
        const auto unixStartTime = std::chrono::duration_cast<milliseconds_t>((std::chrono::system_clock::now() - m_syTimer->timeSinceStartMsec()).time_since_epoch()).count();
        m_procEnv.insert("SY_START_TIME_UNIX_MS", QString::number(unixStartTime));
        m_proc->setProcessEnvironment(m_procEnv);
        m_proc->start();
        setStatusMessage("Process running.");
    }

    void runEvent(int &intervalMsec)
    {
        if (m_proc->state() != QProcess::Running) {
            // we are not running anymore - check for errors
            if (m_proc->exitStatus() == QProcess::CrashExit) {
                raiseError(QStringLiteral("The process %1 crashed: %2").arg(m_proc->program()).arg(m_proc->errorString()));
            } else {
                if (m_proc->exitCode() != 0)
                    raiseError(QStringLiteral("The process %1 failed with exit code: %2").arg(m_proc->program()).arg(m_proc->exitCode()));
            }

            setStatusMessage("Process terminated.");
            setStateIdle();
            // we don't have to run again, our process is dead
            intervalMsec = -1;
        }
    }

    void stop() override
    {
        if (m_proc->state() != QProcess::Running)
            return;

        m_proc->terminate();

        // give the process 5sec to terminate
        m_proc->waitForFinished(5000);

        // finally kill the unresponsive process
        m_proc->kill();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("executable", m_settings->executable());
        settings.insert("parameters", m_settings->parametersStr());
        settings.insert("run_on_host", m_settings->runOnHost());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settings->setExecutable(settings.value("executable").toString());
        m_settings->setParametersStr(settings.value("parameters").toString());
        m_settings->setRunOnHost(settings.value("run_on_host").toBool());
        return true;
    }
};

QString RunCmdModuleInfo::id() const
{
    return QStringLiteral("runcmd");
}

QString RunCmdModuleInfo::name() const
{
    return QStringLiteral("Run Command");
}

QString RunCmdModuleInfo::description() const
{
    return QStringLiteral("Run an external command when the experiment run was started.");
}

AbstractModule *RunCmdModuleInfo::createModule(QObject *parent)
{
    return new RunCmdModule(parent);
}

#include "runcmdmodule.moc"
