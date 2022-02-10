#ifndef SCROLLABLEMESSAGEBOXDIALOG_H
#define SCROLLABLEMESSAGEBOXDIALOG_H

#include <QDialog>
#include <QMessageBox>

class QLabel;

class ScrollableMessageBoxDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ScrollableMessageBoxDialog(QWidget *parent = nullptr, const QString &title = "", const QString &text = "");

private:
    QLabel *message;
    QPushButton *okButton;
};

#endif // SCROLLABLEMESSAGEBOXDIALOG_H
