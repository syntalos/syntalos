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

#include "miniscopemodule.h"

#include <QMessageBox>
#include <QJsonDocument>
#include <QDebug>
#include <miniscope.h>

#include "videoviewwidget.h"
#include "miniscopesettingsdialog.h"

using namespace MScope;

QString MiniscopeModuleInfo::id() const
{
    return QStringLiteral("miniscope");
}

QString MiniscopeModuleInfo::name() const
{
    return QStringLiteral("Miniscope");
}

QString MiniscopeModuleInfo::description() const
{
    return QStringLiteral("Record fluorescence images using a UCLA MiniScope.");
}

QPixmap MiniscopeModuleInfo::pixmap() const
{
    return QPixmap(":/module/miniscope");
}

AbstractModule *MiniscopeModuleInfo::createModule(QObject *parent)
{
    return new MiniscopeModule(parent);
}

MiniscopeModule::MiniscopeModule(QObject *parent)
    : AbstractModule(parent),
      m_miniscope(nullptr),
      m_settingsDialog(nullptr),
      m_videoView(nullptr)
{
    m_timer = nullptr;
    m_miniscope = new MiniScope;
    m_settingsDialog = new MiniscopeSettingsDialog(m_miniscope);
    addSettingsWindow(m_settingsDialog);

    m_videoView = new VideoViewWidget;
    m_videoView->setWindowTitle(QStringLiteral("Miniscope View"));
    addSettingsWindow(m_videoView);

    if (m_name == QStringLiteral("Miniscope"))
        m_settingsDialog->setRecName(QStringLiteral("scope"));

    m_miniscope->setScopeCamId(0);

    setName(name());
}

MiniscopeModule::~MiniscopeModule()
{
    delete m_miniscope;
}

void MiniscopeModule::setName(const QString &name)
{
    AbstractModule::setName(name);
    if (m_settingsDialog != nullptr) {
        m_settingsDialog->setWindowTitle(QStringLiteral("Settings for %1").arg(name));
        m_videoView->setWindowTitle(QStringLiteral("%1 - View").arg(name));
    }
}

bool MiniscopeModule::prepare(const QString &storageRootDir, const TestSubject &)
{
    m_recStorageDir = QStringLiteral("%1/miniscope").arg(storageRootDir);
    if (!makeDirectory(m_recStorageDir))
        return false;
    if (m_settingsDialog->recName().isEmpty()) {
        raiseError("Miniscope recording name is empty. Please specify it in the settings to continue.");
        return false;
    }

    m_miniscope->setVideoFilename(QStringLiteral("%1/%2").arg(m_recStorageDir).arg(m_settingsDialog->recName()).toStdString());

    if (!m_miniscope->connect()) {
        raiseError(QString::fromStdString(m_miniscope->lastError()));
        return false;
    }

    // we already start capturing video here, and only launch the recording when later
    if (!m_miniscope->run()) {
        raiseError(QString::fromStdString(m_miniscope->lastError()));
        return false;
    }

    return true;
}

void MiniscopeModule::start()
{
    m_miniscope->setCaptureStartTimepoint(m_timer->startTime());
    if (!m_miniscope->startRecording())
        raiseError(QString::fromStdString(m_miniscope->lastError()));
}

bool MiniscopeModule::runEvent()
{
    auto frame = m_miniscope->currentFrame();
    if (frame.empty()) {
        // something may be wrong
        if (!m_miniscope->recording()) {
            raiseError(QString::fromStdString(m_miniscope->lastError()));
            return false;
        }
        return true;
    }

    m_videoView->showImage(frame);
    statusMessage(QStringLiteral("FPS: %1 Dropped: %2").arg(m_miniscope->currentFps()).arg(m_miniscope->droppedFramesCount()));

    return true;
}

void MiniscopeModule::stop()
{
    m_timer = nullptr;
    m_miniscope->stopRecording();
    m_miniscope->stop();
    m_miniscope->disconnect();
}

void MiniscopeModule::showSettingsUi()
{
    assert(initialized());
    m_settingsDialog->show();
}

QByteArray MiniscopeModule::serializeSettings(const QString &)
{
    QJsonObject jset;

    return jsonObjectToBytes(jset);
}

bool MiniscopeModule::loadSettings(const QString &, const QByteArray &data)
{
    auto jset = jsonObjectFromBytes(data);
    return true;
}
