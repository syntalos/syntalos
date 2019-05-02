#ifndef TRACEDISPLAY_H
#define TRACEDISPLAY_H

#include <QWidget>

class TracePlotProxy;
class QListWidgetItem;

namespace Ui {
class TraceDisplay;
}

class TraceDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit TraceDisplay(QWidget *parent = nullptr);
    ~TraceDisplay();

    void setPlotProxy(TracePlotProxy *proxy);

#if 0
private slots:
    void on_portListWidget_itemActivated(QListWidgetItem *item);
    void on_chanListWidget_itemActivated(QListWidgetItem *item);
    void on_multiplierDoubleSpinBox_valueChanged(double arg1);
    void on_plotApplyButton_clicked();
    void on_yShiftDoubleSpinBox_valueChanged(double arg1);
    void on_prevPlotButton_toggled(bool checked);

    void on_chanDisplayCheckBox_clicked(bool checked);
    void on_plotRefreshSpinBox_valueChanged(int arg1);
#endif

private:
    Ui::TraceDisplay *ui;
};

#endif // TRACEDISPLAY_H
