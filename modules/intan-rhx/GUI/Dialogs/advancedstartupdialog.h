#ifndef ADVANCEDSTARTUPDIALOG_H
#define ADVANCEDSTARTUPDIALOG_H

#include <QDialog>

class QLabel;
class QCheckBox;
class QDialogButtonBox;
class QGroupBox;

class AdvancedStartupDialog : public QDialog
{
    Q_OBJECT
public:
    AdvancedStartupDialog(bool &useOpenCL_, uint8_t &playbackPorts_, bool demoMode_ = false, QWidget *parent = nullptr);
    static QVector<bool> portsIntToBool(uint8_t portsInt);
    static uint8_t portsBoolToInt(QVector<bool> portsBool);

public slots:
    void accept();

private slots:
    void changeUseOpenCL(bool use);
    void changeSynthMaxChannels(bool max);
    void changeTestMode(bool test);

private:
    void updateUIForTestMode();

    QLabel *useOpenCLDescription;
    QCheckBox *useOpenCLCheckBox;

    QLabel *synthMaxChannelsDescription;
    QCheckBox *synthMaxChannelsCheckBox;

    QLabel *playbackControlDescription;
    QCheckBox *playbackACheckBox;
    QCheckBox *playbackBCheckBox;
    QCheckBox *playbackCCheckBox;
    QCheckBox *playbackDCheckBox;
    QCheckBox *playbackECheckBox;
    QCheckBox *playbackFCheckBox;
    QCheckBox *playbackGCheckBox;
    QCheckBox *playbackHCheckBox;

    QLabel *testModeDescription;
    QCheckBox *testModeCheckBox;

    QDialogButtonBox *buttonBox;

    QGroupBox *playbackControlGroupBox;
    QGroupBox *synthMaxChannelsGroupBox;

    bool *useOpenCL;
    bool tempUseOpenCL;

    bool tempSynthMaxChannels;
    bool tempTest;

    QVector<bool> portsBool;
    uint8_t *playbackPorts;

    bool demoMode;
};

#endif // ADVANCEDSTARTUPDIALOG_H
