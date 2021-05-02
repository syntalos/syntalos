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

#include "tracedisplay.h"
#include "ui_tracedisplay.h"

#include <QDebug>
#include <QTimer>
#include <QLineSeries>

#include "traceplot.h"


class PlotChannelData final : public QObject
{
    Q_OBJECT
public:
    explicit PlotChannelData(QObject *parent = nullptr)
        : QObject(parent),
          multiplier(1),
          xPos(0),
          storeOrig(false),
          m_enabled(false)
    {
        data.reserve(60000);
    }

    void reset()
    {
        xPos = 0;
        dataPrev = data;
        data.clear();
        dataOrig.clear();
    }

    void addNewYValue(double xval)
    {
        if ((multiplier > 1) || (yShift != 0)) {
            if (multiplier <= 0)
                multiplier = 1;

            storeOrig = true;
            data.append(QPointF(xPos, xval * multiplier + yShift));
        } else {
            data.append(QPointF(xPos, xval));
        }

        if (storeOrig)
            dataOrig.append(QPointF(xPos, xval));

        xPos += 1;
    }

    void registerChannel(TracePlot *plot)
    {
        if (this->m_enabled)
            return;

        QLineSeries *series = new QLineSeries();
        series->setUseOpenGL(true);
        plot->addSeries(series);
        this->series = series;

        plot->createDefaultAxes();

        auto axisY = plot->axes(Qt::Vertical).back();
        axisY->setMax(250);
        axisY->setMin(-250);
        axisY->setTitleText(QStringLiteral("µV"));
        auto font = axisY->titleFont();
        font.setPointSize(8);
        axisY->setTitleFont(font);

        this->series->points().reserve(60000);
        this->m_enabled = true;
    }

    void unregisterChannel(TracePlot *plot)
    {
        if (!this->m_enabled)
            return;

        plot->removeSeries(this->series);
        this->m_enabled = false;
    }

    bool enabled()
    {
        return m_enabled;
    }

    QtCharts::QXYSeries *series;
    int chanId;
    int chanDataIndex;

    double multiplier;
    double yShift;

    QList<QPointF> data;
    QList<QPointF> dataOrig;
    QList<QPointF> dataPrev;

    int xPos;
    bool storeOrig;

private:
    bool m_enabled;
};

TraceDisplay::TraceDisplay(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::TraceDisplay),
      m_plot(new TracePlot),
      m_maxXVal(0),
      m_timer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Traces"));
    setWindowIcon(QIcon(":/module/traceplot"));

    m_timer->setSingleShot(true);
    m_timer->setInterval(400);
    connect(m_timer, &QTimer::timeout, this, &TraceDisplay::repaintPlot);

    auto twScrollBar = new QScrollBar(this);
    ui->traceView0->addScrollBarWidget(twScrollBar, Qt::AlignBottom);

    ui->traceView0->setChart(m_plot);
    ui->traceView0->setRenderHint(QPainter::Antialiasing);

    connect(ui->plotScrollBar, &QScrollBar::valueChanged, this, &TraceDisplay::plotMoveTo);
    ui->plotRefreshSpinBox->setValue(m_timer->interval());

    m_plot->setAnimationOptions(QChart::SeriesAnimations);
}

TraceDisplay::~TraceDisplay()
{
    delete ui;
}

void TraceDisplay::addIntPort(std::shared_ptr<StreamInputPort<IntSignalBlock> > port)
{
    auto item = new QListWidgetItem(ui->portListWidget);
    m_portsChannels.append(qMakePair(port, QList<PlotChannelData*>()));
    item->setData(Qt::UserRole, m_portsChannels.size() - 1);
    item->setText(port->title());
}

void TraceDisplay::addFloatPort(std::shared_ptr<StreamInputPort<FloatSignalBlock>> port)
{
    auto item = new QListWidgetItem(ui->portListWidget);
    m_portsChannels.append(qMakePair(port, QList<PlotChannelData*>()));
    item->setData(Qt::UserRole, m_portsChannels.size() - 1);
    item->setText(port->title());
}

void TraceDisplay::updatePortChannels()
{
    for (auto &pcPair : m_portsChannels) {
        auto port = pcPair.first;
        if (!port->hasSubscription())
            continue;
        const auto mdata = port->subscriptionVar()->metadata();
        const auto firstChanNo = mdata.value(QStringLiteral("channel_index_first")).toInt();
        const auto lastChanNo = mdata.value(QStringLiteral("channel_index_last")).toInt();

        // sanity check
        if (firstChanNo > lastChanNo) {
            qWarning().noquote().nospace() << "Ignored traceplot port " << port->id() << ": Channel count limits are invalid.";
            return;
        }

        int dataIdx = 0;
        QList<PlotChannelData*> channels;
        for (int chan = firstChanNo; chan <= lastChanNo; chan++) {
            auto pcd = new PlotChannelData(this);
            pcd->chanId = chan;
            pcd->chanDataIndex = dataIdx;
            channels.append(pcd);

            dataIdx++;
        }

        // delete the old elements
        for (int i = 0; i < pcPair.second.size(); i++)
          delete pcPair.second.takeAt(0);
        pcPair.second = channels;
    }

    // propagate active subscriptions
    resetPlotConfig();
}

template<typename T>
static inline bool updateDataForActiveChannels(QList<QPair<std::shared_ptr<StreamSubscription<T>>, QList<PlotChannelData*>>> &activeSubChans)
{
    bool updated = false;

    for (const auto pair : activeSubChans) {
        const auto sub = pair.first;

        auto maybeSigBlock = sub->peekNext();
        if (!maybeSigBlock.has_value())
            continue;

        const auto sigBlock = maybeSigBlock.value();

        for (const auto pcd : pair.second) {
            if (!pcd->enabled())
                continue;

            for (ssize_t i = 0; i < sigBlock.data.col(pcd->chanDataIndex).size(); i++)
                pcd->addNewYValue(sigBlock.data(i, pcd->chanDataIndex));

            updated = true;
        }
    }

    return updated;
}

void TraceDisplay::updatePlotData(bool adjustView)
{
    bool updated = false;

    // floating-point channels
    updated = updateDataForActiveChannels<FloatSignalBlock>(m_activeFSubChans);

    // integer channels
    updated = updateDataForActiveChannels<IntSignalBlock>(m_activeISubChans) || updated;

    if (!updated)
        return;

    if (!m_timer->isActive())
        m_timer->start();

    if (adjustView)
        plotAdjustView();
}

void TraceDisplay::plotAdjustView()
{
    if (m_maxXVal < 2000)
        return;
    auto axisX = m_plot->axes(Qt::Horizontal).back();
    axisX->setRange(m_maxXVal - 2000, m_maxXVal);
}

void TraceDisplay::plotMoveTo(int position)
{
    auto axisX = m_plot->axes(Qt::Horizontal).back();
    axisX->setRange(position, position + 2000);
}

void TraceDisplay::resetPlotConfig()
{
    m_activeFSubChans.clear();
    m_activeISubChans.clear();

    for (const auto &pair : m_portsChannels) {
        auto port = pair.first;
        for (const auto pcd : pair.second)
            pcd->reset();

        if (port->hasSubscription()) {
            auto fPortSub = std::dynamic_pointer_cast<StreamSubscription<FloatSignalBlock>>(port->subscriptionVar());
            if (fPortSub.get() != nullptr) {
                m_activeFSubChans.append(qMakePair(fPortSub, pair.second));
                continue;
            }

            auto iPortSub = std::dynamic_pointer_cast<StreamSubscription<IntSignalBlock>>(port->subscriptionVar());
            if (iPortSub.get() != nullptr) {
                m_activeISubChans.append(qMakePair(iPortSub, pair.second));
                continue;
            }
        }
    }

    m_maxXVal = 0;
}

void TraceDisplay::repaintPlot()
{
    for (const auto &pair : m_portsChannels) {
        for (const auto pcd : pair.second) {
            if (!pcd->enabled())
                continue;

            // replace is *much* faster than append(QPointF)
            // see https://bugreports.qt.io/browse/QTBUG-55714
            pcd->series->replace(pcd->data);

            // set & broadcast our maximum horizontal position
            if (pcd->xPos > m_maxXVal) {
                m_maxXVal = pcd->xPos;
                ui->plotScrollBar->setMaximum(m_maxXVal);
                ui->plotScrollBar->setValue(m_maxXVal);
            }
        }
    }
}

void TraceDisplay::on_multiplierDoubleSpinBox_valueChanged(double arg1)
{
    auto pcd = selectedPlotChannelData();
    if (pcd == nullptr)
        return;

    ui->plotApplyButton->setEnabled(true);

    pcd->multiplier = arg1;
}

void TraceDisplay::on_plotApplyButton_clicked()
{
    ui->plotApplyButton->setEnabled(false);

    for (const auto &pair : m_portsChannels) {
        for (const auto &pcd : pair.second) {
            if (!pcd->enabled())
                continue;
            if (pcd->dataOrig.size() == 0)
                pcd->dataOrig = pcd->data;

            if (pcd->multiplier <= 0)
                pcd->multiplier = 1;

            pcd->storeOrig = true;

            for (auto i = 0; i < pcd->dataOrig.size(); i++) {
                pcd->data[i].setY(pcd->dataOrig[i].y() * pcd->multiplier + pcd->yShift);
            }
        }
    }

    repaintPlot();
}

void TraceDisplay::on_yShiftDoubleSpinBox_valueChanged(double arg1)
{
    auto pcd = selectedPlotChannelData();
    if (pcd == nullptr)
        return;

    ui->plotApplyButton->setEnabled(true);

    pcd->yShift = arg1;
}

void TraceDisplay::on_portListWidget_currentItemChanged(QListWidgetItem *item, QListWidgetItem *)
{
    auto portIndex = item->data(Qt::UserRole).toInt();
    if ((portIndex < 0) || (portIndex > m_portsChannels.size() - 1))
        return;
    auto streamChan = m_portsChannels[portIndex];
    const auto channels = streamChan.second;

    ui->chanListWidget->clear();
    if (channels.empty()) {
        ui->chanListWidget->setEnabled(false);
        ui->chanSettingsGroupBox->setEnabled(false);
        return;
    }
    ui->chanListWidget->setEnabled(true);

    for (const auto &pcd : channels) {
        auto item = new QListWidgetItem(ui->chanListWidget);
        item->setData(Qt::UserRole, pcd->chanDataIndex);
        item->setText(QStringLiteral("Channel %1").arg(pcd->chanId));
    }
}

void TraceDisplay::on_chanListWidget_currentItemChanged(QListWidgetItem *, QListWidgetItem *)
{
    auto pcd = selectedPlotChannelData();
    if (pcd != nullptr) {
        ui->chanSettingsGroupBox->setEnabled(true);

        ui->chanDisplayCheckBox->setChecked(pcd->enabled());
        ui->multiplierDoubleSpinBox->setValue(pcd->multiplier);
        ui->yShiftDoubleSpinBox->setValue(pcd->yShift);
    } else {
        ui->chanSettingsGroupBox->setEnabled(false);

        ui->chanDisplayCheckBox->setChecked(false);
        ui->multiplierDoubleSpinBox->setValue(1);
        ui->yShiftDoubleSpinBox->setValue(0);
    }
}

void TraceDisplay::on_prevPlotButton_toggled(bool checked)
{
    Q_UNUSED(checked)
    // TODO
}

void TraceDisplay::on_chanDisplayCheckBox_clicked(bool checked)
{
    if (ui->portListWidget->selectedItems().isEmpty() || ui->chanListWidget->selectedItems().isEmpty()) {
        qCritical() << "Can not determine which graph to display: Port/Channel selection does not make sense";
        return;
    }

    auto portIndex = ui->portListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();
    if ((portIndex < 0) || (portIndex > m_portsChannels.size() - 1))
        return;
    auto pcPair = m_portsChannels[portIndex];

    auto chanDataIdx = ui->chanListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();

    const bool hasChanged = checked != pcPair.second[chanDataIdx]->enabled();
    if (checked)
        pcPair.second[chanDataIdx]->registerChannel(m_plot);
    else
        pcPair.second[chanDataIdx]->unregisterChannel(m_plot);

    // we changed what is displayed, so we reset the view and DAQ rules
    if (hasChanged)
        resetPlotConfig();
}

void TraceDisplay::on_plotRefreshSpinBox_valueChanged(int arg1)
{
    m_timer->setInterval(arg1);
}

PlotChannelData *TraceDisplay::selectedPlotChannelData()
{
    if (!ui->portListWidget->currentItem() || !ui->chanListWidget->currentItem()) {
        qCritical() << "Can not determine selected trace: Port/Channel selection does not make sense";
        return nullptr;
    }

    auto portId = ui->portListWidget->currentItem()->data(Qt::UserRole).toInt();
    auto chanIntIdx = ui->chanListWidget->currentItem()->data(Qt::UserRole).toInt();

    if (portId < 0 || portId >= m_portsChannels.count())
        return nullptr;
    const auto channels = m_portsChannels[portId].second;
    if (chanIntIdx < 0 || chanIntIdx >= channels.count())
        return nullptr;

    return channels[chanIntIdx];
}

#include "tracedisplay.moc"
