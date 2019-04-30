#include "tracedisplay.h"
#include "ui_tracedisplay.h"

TraceDisplay::TraceDisplay(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TraceDisplay)
{
    ui->setupUi(this);
}

TraceDisplay::~TraceDisplay()
{
    delete ui;
}
