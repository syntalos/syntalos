#include "tracedisplay.h"
#include "ui_tracedisplay.h"

#include "traceplotproxy.h"

TraceDisplay::TraceDisplay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TraceDisplay)
{
    ui->setupUi(this);

    auto twScrollBar = new QScrollBar(this);
    ui->traceView0->addScrollBarWidget(twScrollBar, Qt::AlignBottom);

    // there are 6 ports on the Intan eval port - we hardcode that at time
    for (uint port = 0; port < 6; port++) {
        auto item = new QListWidgetItem;
        item->setData(Qt::UserRole, port);
        item->setText(QString("Port %1").arg(port));
        ui->portListWidget->addItem(item);
    }
}

TraceDisplay::~TraceDisplay()
{
    delete ui;
}

void TraceDisplay::setPlotProxy(TracePlotProxy *proxy)
{
    ui->traceView0->setChart(proxy->plot());
    ui->traceView0->setRenderHint(QPainter::Antialiasing);

    connect(proxy, &TracePlotProxy::maxHorizontalPositionChanged, ui->plotScrollBar, &QScrollBar::setMaximum);
    connect(proxy, &TracePlotProxy::maxHorizontalPositionChanged, ui->plotScrollBar, &QScrollBar::setValue);
    connect(ui->plotScrollBar, &QScrollBar::valueChanged, proxy, &TracePlotProxy::moveTo);
    ui->plotRefreshSpinBox->setValue(proxy->refreshTime());
}

#if 0
ChannelDetails *TraceDisplay::selectedPlotChannelDetails()
{
    if (ui->portListWidget->selectedItems().isEmpty() || ui->chanListWidget->selectedItems().isEmpty()) {
        qCritical() << "Can not determine selected trace: Port/Channel selection does not make sense";
        return nullptr;
    }

    auto portId = ui->portListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();
    auto chanId = ui->chanListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();

    auto details = m_traceProxy->getDetails(portId, chanId);

    return details;
}

void TraceDisplay::on_chanListWidget_itemActivated(QListWidgetItem *item)
{
    Q_UNUSED(item);
    auto details = selectedPlotChannelDetails();
    ui->chanSettingsGroupBox->setEnabled(true);

    if (details != nullptr) {
        ui->chanDisplayCheckBox->setChecked(true);
        ui->multiplierDoubleSpinBox->setValue(details->multiplier);
        ui->yShiftDoubleSpinBox->setValue(details->yShift);
    } else {
        ui->chanDisplayCheckBox->setChecked(false);
        ui->multiplierDoubleSpinBox->setValue(1);
        ui->yShiftDoubleSpinBox->setValue(0);
    }
}

void TraceDisplay::on_multiplierDoubleSpinBox_valueChanged(double arg1)
{
    auto details = selectedPlotChannelDetails();
    if (details == nullptr)
        return;

    ui->plotApplyButton->setEnabled(true);

    details->multiplier = arg1;
}

void TraceDisplay::on_plotApplyButton_clicked()
{
    ui->plotApplyButton->setEnabled(false);
    m_traceProxy->applyDisplayModifiers();
}

void TraceDisplay::on_yShiftDoubleSpinBox_valueChanged(double arg1)
{
    auto details = selectedPlotChannelDetails();
    if (details == nullptr)
        return;

    ui->plotApplyButton->setEnabled(true);

    details->yShift = arg1;
}

void MainWindow::on_portListWidget_itemActivated(QListWidgetItem *item)
{
    Q_UNUSED(item);
    //! FIXME TracePlot module
#if 0
    auto port = item->data(Qt::UserRole).toInt();
    auto waveplot = m_intanUI->getWavePlot();

    ui->chanListWidget->clear();
    if (!waveplot->isPortEnabled(port)) {
        ui->chanListWidget->setEnabled(false);
        ui->chanSettingsGroupBox->setEnabled(false);
        return;
    }
    ui->chanListWidget->setEnabled(true);

    for (int chan = 0; chan < waveplot->getChannelCount(port); chan++) {
        auto item = new QListWidgetItem;
        item->setData(Qt::UserRole, chan);
        item->setText(waveplot->getChannelName(port, chan));
        ui->chanListWidget->addItem(item);
    }
#endif
}

void MainWindow::on_prevPlotButton_toggled(bool checked)
{
    Q_UNUSED(checked);
    // TODO
}

void MainWindow::on_chanDisplayCheckBox_clicked(bool checked)
{
    if (ui->portListWidget->selectedItems().isEmpty() || ui->chanListWidget->selectedItems().isEmpty()) {
        qCritical() << "Can not determine which graph to display: Port/Channel selection does not make sense";
        return;
    }

    auto portId = ui->portListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();
    auto chanId = ui->chanListWidget->selectedItems()[0]->data(Qt::UserRole).toInt();

    if (checked)
        m_traceProxy->addChannel(portId, chanId);
    else
        m_traceProxy->removeChannel(portId, chanId);
}

void MainWindow::on_plotRefreshSpinBox_valueChanged(int arg1)
{
    m_traceProxy->setRefreshTime(arg1);
}
#endif
