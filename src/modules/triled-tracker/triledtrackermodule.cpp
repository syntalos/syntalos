/**
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

#include "triledtrackermodule.h"

#include <QMessageBox>
#include <QDebug>

#include "imagesourcemodule.h"
#include "ledtrackersettingsdialog.h"

TriLedTrackerModule::TriLedTrackerModule(QObject *parent)
    : ImageSinkModule(parent),
      m_settingsDialog(nullptr)
{
    m_name = QStringLiteral("TriLED Tracker");
}

TriLedTrackerModule::~TriLedTrackerModule()
{
    if (m_settingsDialog != nullptr)
        delete m_settingsDialog;
}

QString TriLedTrackerModule::id() const
{
    return QStringLiteral("triled-tracker");
}

QString TriLedTrackerModule::description() const
{
    return QStringLiteral("Track subject behavior via three LEDs mounted on its head.");
}

QPixmap TriLedTrackerModule::pixmap() const
{
    return QPixmap(":/module/triled-tracker");
}

void TriLedTrackerModule::setName(const QString &name)
{
    ImageSinkModule::setName(name);
}

ModuleFeatures TriLedTrackerModule::features() const
{
    return ModuleFeature::SETTINGS;
}

bool TriLedTrackerModule::initialize(ModuleManager *manager)
{
    assert(!initialized());
    setState(ModuleState::INITIALIZING);

    m_settingsDialog = new LedTrackerSettingsDialog;
    m_settingsDialog->setResultsName("movement");

    // find all modules suitable as frame sources
    Q_FOREACH(auto mod, manager->activeModules()) {
        auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
        if (imgSrcMod == nullptr)
            continue;
        m_frameSourceModules.append(imgSrcMod);
    }
    m_settingsDialog->setSelectedImageSourceMod(m_frameSourceModules.first()); // set first module as default

    setState(ModuleState::READY);
    setInitialized();
    setName(name());
    return true;
}

bool TriLedTrackerModule::prepare(const QString &storageRootDir, const TestSubject &testSubject, HRTimer *timer)
{
    Q_UNUSED(testSubject);
    Q_UNUSED(timer);
    setState(ModuleState::PREPARING);

    m_dataStorageDir = QStringLiteral("%1/tracking").arg(storageRootDir);

    if (m_settingsDialog->resultsName().isEmpty()) {
        raiseError("Tracking result name is not set. Please set it in the module settings to continue.");
        return false;
    }

    auto imgSrcMod = m_settingsDialog->selectedImageSourceMod();
    if (imgSrcMod == nullptr) {
        raiseError("No frame source is set for subject tracking. Please set it in the module settings to continue.");
        return false;
    }

    statusMessage(QStringLiteral("Tracking via %1").arg(imgSrcMod->name()));
    setState(ModuleState::WAITING);
    return true;
}

void TriLedTrackerModule::stop()
{
    statusMessage(QStringLiteral("Tracker stopped."));
}

bool TriLedTrackerModule::canRemove(AbstractModule *mod)
{
    return mod != m_settingsDialog->selectedImageSourceMod();
}

void TriLedTrackerModule::showSettingsUi()
{
    assert(initialized());
    m_settingsDialog->setImageSourceModules(m_frameSourceModules);
    m_settingsDialog->show();
}

void TriLedTrackerModule::hideSettingsUi()
{
    assert(initialized());
    m_settingsDialog->hide();
}

void TriLedTrackerModule::recvModuleCreated(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod != nullptr)
        m_frameSourceModules.append(imgSrcMod);
}

void TriLedTrackerModule::recvModulePreRemove(AbstractModule *mod)
{
    auto imgSrcMod = qobject_cast<ImageSourceModule*>(mod);
    if (imgSrcMod == nullptr)
        return;
    for (int i = 0; i < m_frameSourceModules.size(); i++) {
        auto fsmod = m_frameSourceModules.at(i);
        if (fsmod == imgSrcMod) {
            m_frameSourceModules.removeAt(i);
            break;
        }
    }
}

void TriLedTrackerModule::receiveFrame(const FrameData &frameData)
{

}
