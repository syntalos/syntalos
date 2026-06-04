/*
 * Copyright (C) 2016-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QCheckBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QTableWidgetItem>

#include "moduleapi.h"

PlotWindow::PlotWindow(AbstractModule *mod, QWidget *parent)
    : QWidget(parent),
      ui(new Ui::PlotWindow),
      m_mod(mod),
      m_canvas(nullptr),
      m_running(false),
      m_defaultSettingsVisible(true)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Time Series Plotter"));

    m_canvas = new PlotCanvas(this);
    ui->plotContainer->layout()->addWidget(m_canvas);
    m_canvas->setBufferSize(static_cast<size_t>(ui->bufferSizeSpinBox->value()) * 1000);
    m_canvas->setUpdateInterval(ui->speedSpinBox->value());

    ui->channelTable->horizontalHeader()->setStretchLastSection(false);
    ui->channelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->channelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    connect(m_canvas, &PlotCanvas::layoutChanged, this, &PlotWindow::refreshChannelTable);
}

PlotWindow::~PlotWindow()
{
    delete ui;
}

PlotCanvas *PlotWindow::canvas() const
{
    return m_canvas;
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

void PlotWindow::on_settingsDisplayBtn_clicked()
{
    setSettingsPanelVisible(!ui->settingsWidget->isVisible());
}

void PlotWindow::refreshChannelTable()
{
    auto t = ui->channelTable;
    QSignalBlocker block(t);
    // Wipe rows + cell widgets first so old QCheckBox children of the table
    // can't briefly render at the table's top-left before being reparented.
    t->setRowCount(0);

    // Group channels by portId so each port can contribute a contiguous block
    // of rows (and a placeholder row when no data has arrived yet).
    QMap<QString, QList<int>> channelsByPort;
    for (int i = 0; i < m_canvas->channelCount(); ++i)
        channelsByPort[m_canvas->channelInfo(i).portId].append(i);

    int row = 0;
    for (const auto &port : m_mod->inPorts()) {
        const QString portId = port->id();
        const QString portTitle = port->title();
        const auto &channels = channelsByPort.value(portId);

        if (channels.isEmpty()) {
            // Placeholder row so the port can be selected and removed even
            // before any data has flowed through it.
            t->insertRow(row);

            auto portItem = new QTableWidgetItem(portTitle);
            portItem->setFlags(portItem->flags() & ~Qt::ItemIsEditable);
            portItem->setData(Qt::UserRole, portId);
            t->setItem(row, 0, portItem);

            auto placeholder = new QTableWidgetItem(QStringLiteral("(no data yet)"));
            placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsEditable);
            auto font = placeholder->font();
            font.setItalic(true);
            placeholder->setFont(font);
            placeholder->setForeground(QBrush(t->palette().color(QPalette::Disabled, QPalette::Text)));
            t->setItem(row, 1, placeholder);

            ++row;
            continue;
        }

        for (int idx = 0; idx < channels.size(); ++idx) {
            const int channelIndex = channels[idx];
            const auto info = m_canvas->channelInfo(channelIndex);
            t->insertRow(row);

            // Show the port title only on the first row of each group to
            // visually convey grouping; subsequent rows leave col 0 blank but
            // still carry the portId in UserRole so removal works from any row.
            auto portItem = new QTableWidgetItem(idx == 0 ? portTitle : QString());
            portItem->setFlags(portItem->flags() & ~Qt::ItemIsEditable);
            portItem->setData(Qt::UserRole, portId);
            t->setItem(row, 0, portItem);

            auto chanItem = new QTableWidgetItem(info.signalName);
            chanItem->setFlags(chanItem->flags() & ~Qt::ItemIsEditable);
            t->setItem(row, 1, chanItem);

            // Create with no parent - setCellWidget reparents to t->viewport().
            // Parenting to `t` directly causes a brief (0,0) render in the table.
            auto showCb = new QCheckBox();
            showCb->setChecked(info.enabled);
            showCb->setEnabled(!m_running);
            connect(showCb, &QCheckBox::toggled, this, [this, channelIndex](bool checked) {
                onShowToggled(channelIndex, checked);
            });
            t->setCellWidget(row, 2, showCb);

            auto digCb = new QCheckBox();
            digCb->setChecked(info.digital);
            digCb->setEnabled(!m_running);
            connect(digCb, &QCheckBox::toggled, this, [this, channelIndex](bool checked) {
                onDigitalToggled(channelIndex, checked);
            });
            t->setCellWidget(row, 3, digCb);

            ++row;
        }
    }
}

void PlotWindow::onShowToggled(int channelIndex, bool checked)
{
    m_canvas->setChannelEnabled(channelIndex, checked);
}

void PlotWindow::onDigitalToggled(int channelIndex, bool checked)
{
    m_canvas->setChannelDigital(channelIndex, checked);
}

void PlotWindow::setRunning(bool running)
{
    m_running = running;
    ui->channelsGroupBox->setEnabled(true); // table itself stays usable; checkboxes disable per-row
    ui->bufferSizeSpinBox->setEnabled(!running);
    ui->addPortBtn->setEnabled(!running);
    ui->removePortBtn->setEnabled(!running);
    ui->resetLayoutBtn->setEnabled(!running);

    m_canvas->setRunning(running);

    // save previous settings panel state when we switch to running mode
    if (running)
        m_defaultSettingsVisible = ui->settingsWidget->isVisible();

    // hide settings panel while we are running
    if (running)
        setSettingsPanelVisible(false);
    else
        setSettingsPanelVisible(m_defaultSettingsVisible);

    refreshChannelTable();
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

int PlotWindow::updateFrequency() const
{
    return ui->speedSpinBox->value();
}

void PlotWindow::setUpdateFrequency(int hz)
{
    ui->speedSpinBox->setValue(hz);
    m_canvas->setUpdateInterval(hz);
}

int PlotWindow::bufferSize() const
{
    return ui->bufferSizeSpinBox->value();
}

void PlotWindow::setBufferSize(int kitems)
{
    ui->bufferSizeSpinBox->setValue(kitems);
    m_canvas->setBufferSize(static_cast<size_t>(kitems) * 1000);
}

void PlotWindow::on_speedSpinBox_valueChanged(int arg1)
{
    m_canvas->setUpdateInterval(arg1);
}

void PlotWindow::on_bufferSizeSpinBox_valueChanged(int arg1)
{
    m_canvas->setBufferSize(static_cast<size_t>(arg1) * 1000);
}

void PlotWindow::on_addPortBtn_clicked()
{
    QMap<QString, int> streamSignalTypeMap;
    const auto allStreamTypes = streamTypeIdIndex();

    for (const auto &[key, value] : allStreamTypes) {
        if (key == "SignalBlockI32")
            streamSignalTypeMap["Integer"] = value;
        else if (key == "SignalBlockF32")
            streamSignalTypeMap["Float"] = value;
        else if (key == "LineReading")
            streamSignalTypeMap["Digital Lines"] = value;
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
        item + QStringLiteral(" %1").arg(newPortNumber),
        &ok);
    if (!ok || title.isEmpty())
        return;

    const auto newPortId = QStringLiteral("sigs%1-in").arg(newPortNumber);
    m_mod->registerInputPortByTypeId(streamSignalTypeMap[item], newPortId, title);
    m_canvas->registerPort(newPortId, 1000.0, QStringLiteral("y"));
    refreshChannelTable();
}

void PlotWindow::on_removePortBtn_clicked()
{
    auto t = ui->channelTable;
    const auto rows = t->selectionModel()->selectedRows();
    if (rows.isEmpty())
        return;
    const int row = rows.first().row();
    auto portItem = t->item(row, 0);
    if (portItem == nullptr)
        return;
    const QString portId = portItem->data(Qt::UserRole).toString();
    if (portId.isEmpty())
        return;

    m_canvas->unregisterPort(portId);
    m_mod->removeInPortById(portId);
    refreshChannelTable();
}

void PlotWindow::on_resetLayoutBtn_clicked()
{
    m_canvas->resetLayoutOneChannelPerGraph();
    refreshChannelTable();
}
