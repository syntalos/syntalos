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

#include "plotwindow.h"
#include "ui_plotwindow.h"

#include <QDebug>
#include <QScrollBar>
#include <QInputDialog>

#include "moduleapi.h"
#include "timeplotwidget.h"

PlotWindow::PlotWindow(AbstractModule *mod, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::PlotWindow),
      m_mod(mod),
      m_running(false),
      m_defaultSettingsVisible(true)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Time Series Plotter"));
}

PlotWindow::~PlotWindow()
{
    delete ui;
}

void PlotWindow::setSettingsPanelVisible(bool visible)
{
    if (visible) {
        ui->settingsWidget->setVisible(true);
        ui->settingsDisplayBtn->setIcon(QIcon::fromTheme("go-down"));
    } else {
        ui->settingsWidget->setVisible(false);
        ui->settingsDisplayBtn->setIcon(QIcon::fromTheme("go-up"));
    }
}

bool PlotWindow::checkAnyPortSignalsVisible(const QString &portId)
{
    bool anyVisible = false;

    // special case: If there are no signals, we likely will get some metadata
    // during the next run, so we consider *all* of them visible.
    if (m_signalDetails[portId].isEmpty())
        return true;

    for (const auto &sd : m_signalDetails[portId].values()) {
        if (sd.isVisible) {
            anyVisible = true;
            break;
        }
    }
    if (!anyVisible) {
        delete static_cast<QWidget *>(m_plotWidgets[portId]);
        m_plotWidgets.remove(portId);
    }

    return anyVisible;
}

void PlotWindow::on_settingsDisplayBtn_clicked()
{
    setSettingsPanelVisible(!ui->settingsWidget->isVisible());
}

void PlotWindow::updatePortLists()
{
    ui->portListWidget->clear();

    // remove all existing plot displays
    for (QWidget *w : m_plotWidgets.values())
        delete w;
    m_plotWidgets.clear();

    // display all registered ports in our UI
    for (const auto &port : m_mod->inPorts()) {
        auto item = new QListWidgetItem(
            QStringLiteral("%1 [>>%2]").arg(port->title()).arg(port->dataTypeName()), ui->portListWidget);
        item->setData(Qt::UserRole, port->id());

        auto plot = new TimePlotWidget(this);
        ui->plotContainer->layout()->addWidget(plot);
        m_plotWidgets.insert(port->id(), plot);

        if (m_signalDetails.contains(port->id()))
            checkAnyPortSignalsVisible(port->id());
        else
            m_signalDetails.insert(port->id(), QMap<QString, PlotSeriesSettings>());
    }
}

void PlotWindow::setSignalsForPort(const QString &portId, const QStringList &signalNames)
{
    bool signalsVisible = m_plotWidgets.contains(portId);

    // update the settings map
    QSet<QString> removeEntries;
    for (const auto &name : m_signalDetails[portId].keys())
        removeEntries.insert(name);

    for (const auto &name : signalNames) {
        if (!m_signalDetails[portId].contains(name))
            m_signalDetails[portId].insert(name, PlotSeriesSettings(name, signalsVisible));
        removeEntries.remove(name);
    }

    for (const auto &name : removeEntries)
        m_signalDetails[portId].remove(name);
}

TimePlotWidget *PlotWindow::plotWidgetForPort(const QString &portId)
{
    if (!m_plotWidgets.contains(portId))
        return nullptr;
    return m_plotWidgets.value(portId);
}

bool PlotWindow::signalIsShown(const QString &portId, const QString &signalName)
{
    const auto sigDetails = m_signalDetails[portId];

    if (!sigDetails.contains(signalName))
        return m_plotWidgets.contains(portId);

    return sigDetails[signalName].isVisible;
}

PlotSeriesSettings PlotWindow::signalPlotSettingsFor(const QString &portId, const QString &signalName)
{
    return m_signalDetails[portId][signalName];
}

QList<PlotSeriesSettings> PlotWindow::signalPlotSettingsFor(const QString &portId)
{
    return m_signalDetails.value(portId, QMap<QString, PlotSeriesSettings>()).values();
}

void PlotWindow::setSignalPlotSettings(const QString &portId, const PlotSeriesSettings &pss)
{
    m_signalDetails[portId][pss.name] = pss;
}

void PlotWindow::setRunning(bool running)
{
    ui->settingsWidget->setEnabled(!running);
    for (TimePlotWidget *w : m_plotWidgets.values())
        w->setRunning(running);

    m_running = running;

    // save previous settings panel state when we switch to running mode
    if (running)
        m_defaultSettingsVisible = ui->settingsWidget->isVisible();

    // hide settings panel while we are running
    if (running)
        setSettingsPanelVisible(false);
    else
        setSettingsPanelVisible(m_defaultSettingsVisible);
}

bool PlotWindow::defaultSettingsVisible()
{
    if (!m_running)
        m_defaultSettingsVisible = ui->settingsWidget->isVisible();
    return m_defaultSettingsVisible;
}

void PlotWindow::setDefaultSettingsVisible(bool visible)
{
    m_defaultSettingsVisible = visible;
    ui->settingsWidget->setVisible(m_defaultSettingsVisible);
}

void PlotWindow::on_portListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (current == nullptr) {
        ui->sigSettingsGroupBox->setEnabled(false);
        ui->sigListWidget->setEnabled(false);
        ui->removePortBtn->setEnabled(false);
        return;
    }

    const auto signalNames =
        m_signalDetails.value(current->data(Qt::UserRole).toString(), QMap<QString, PlotSeriesSettings>()).keys();
    ui->sigListWidget->clear();
    for (const auto &name : signalNames) {
        auto item = new QListWidgetItem(name, ui->sigListWidget);
        item->setData(Qt::UserRole, name);
    }
    ui->sigListWidget->setEnabled(true);
    ui->removePortBtn->setEnabled(true);
}

void PlotWindow::on_portListWidget_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    on_portListWidget_currentItemChanged(ui->portListWidget->currentItem(), ui->portListWidget->currentItem());
}

void PlotWindow::on_sigListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous)
    if (current == nullptr) {
        ui->sigSettingsGroupBox->setEnabled(false);
        return;
    }

    const auto portId = ui->portListWidget->currentItem()->data(Qt::UserRole).toString();
    const auto sigName = current->data(Qt::UserRole).toString();

    const auto pss = m_signalDetails[portId][sigName];
    ui->sigSettingsGroupBox->setEnabled(true);
    ui->showSignalCheckBox->setChecked(signalIsShown(portId, sigName));
    ui->digitalCheckBox->setChecked(pss.isDigital);
}

void PlotWindow::on_sigListWidget_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    on_sigListWidget_currentItemChanged(ui->sigListWidget->currentItem(), ui->sigListWidget->currentItem());
}

void PlotWindow::on_addPortBtn_clicked()
{
    QMap<QString, int> streamSignalTypeMap;
    const auto allStreamTypes = streamTypeIdMap();

    for (const auto &key : allStreamTypes.keys()) {
        if (key == "FloatSignalBlock")
            streamSignalTypeMap["Float"] = allStreamTypes[key];
        else if (key == "IntSignalBlock")
            streamSignalTypeMap["Int"] = allStreamTypes[key];
    }

    int newPortNumber = m_mod->inPorts().length() + 1;

    bool ok;
    auto item = QInputDialog::getItem(
        this,
        QStringLiteral("Input Port Data Type"),
        QStringLiteral("Data type accepted by the input port:"),
        streamSignalTypeMap.keys(),
        0,
        false,
        &ok);
    if (!ok || item.isEmpty())
        return;

    auto title = QInputDialog::getText(
        this,
        QStringLiteral("Set Port Title"),
        QStringLiteral("A human-readable short port title:"),
        QLineEdit::Normal,
        item + QStringLiteral(" In %1").arg(newPortNumber),
        &ok);
    if (!ok || title.isEmpty())
        return;

    const auto newPortId = QStringLiteral("sigs%1-in").arg(newPortNumber);

    m_mod->registerInputPortByTypeId(streamSignalTypeMap[item], newPortId, title);
    updatePortLists();
}

void PlotWindow::on_removePortBtn_clicked()
{
    auto portItem = ui->portListWidget->currentItem();
    if (portItem == nullptr)
        return;
    ui->removePortBtn->setEnabled(false);
    m_mod->removeInPortById(portItem->data(Qt::UserRole).toString());
    updatePortLists();
}

void PlotWindow::on_showSignalCheckBox_toggled(bool checked)
{
    const auto portId = ui->portListWidget->currentItem()->data(Qt::UserRole).toString();
    const auto sigName = ui->sigListWidget->currentItem()->data(Qt::UserRole).toString();
    if (checked) {
        m_signalDetails[portId][sigName].isVisible = true;

        // ensure the plot widget exists if any signal should be shown
        if (!m_plotWidgets.contains(portId)) {
            auto plot = new TimePlotWidget(this);
            ui->plotContainer->layout()->addWidget(plot);
            m_plotWidgets.insert(portId, plot);
        }
    } else {
        m_signalDetails[portId][sigName].isVisible = false;

        // delete the plot widget if no more signals should be shown
        checkAnyPortSignalsVisible(portId);
    }
}

void PlotWindow::on_digitalCheckBox_toggled(bool checked)
{
    const auto portId = ui->portListWidget->currentItem()->data(Qt::UserRole).toString();
    const auto sigName = ui->sigListWidget->currentItem()->data(Qt::UserRole).toString();
    m_signalDetails[portId][sigName].isDigital = checked;
}
