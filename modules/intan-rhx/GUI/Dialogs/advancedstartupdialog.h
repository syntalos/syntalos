#ifndef ADVANCEDSTARTUPDIALOG_H
#define ADVANCEDSTARTUPDIALOG_H

#include <QDialog>

class QLabel;
class QCheckBox;
class QDialogButtonBox;

class AdvancedStartupDialog : public QDialog
{
    Q_OBJECT
public:
    AdvancedStartupDialog(bool &useOpenCL_, QWidget *parent = nullptr);

public slots:
    void accept();

private slots:
    void changeUseOpenCL(bool use);

private:
    QLabel *useOpenCLDescription;
    QCheckBox *useOpenCLCheckBox;
    QDialogButtonBox *buttonBox;

    bool *useOpenCL;
    bool tempUseOpenCL;
};

#endif // ADVANCEDSTARTUPDIALOG_H
