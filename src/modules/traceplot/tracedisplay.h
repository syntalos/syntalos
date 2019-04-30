#ifndef TRACEDISPLAY_H
#define TRACEDISPLAY_H

#include <QWidget>

namespace Ui {
class TraceDisplay;
}

class TraceDisplay : public QWidget
{
    Q_OBJECT

public:
    explicit TraceDisplay(QWidget *parent = nullptr);
    ~TraceDisplay();

private:
    Ui::TraceDisplay *ui;
};

#endif // TRACEDISPLAY_H
