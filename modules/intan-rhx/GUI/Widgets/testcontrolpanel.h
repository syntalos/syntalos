#ifndef TESTCONTROLPANEL_H
#define TESTCONTROLPANEL_H

#include <controlpanel.h>
#include <queue>

struct ChannelInfo {
    int channelVectorIndex;
    int actualChannelNumber;
    double triangleError;
    double variableError;
    double posAvg;
    double negAvg;
};

enum ConnectedChannels {
    All16,
    Inner16,
    Outer16,
    All32,
    Inner32,
    Outer32,
    All64,
    All128
};

#define PI  3.14159265359
#define HELP_DIALOG_WIDTH 400

class ReportDialog : public QDialog
{
    Q_OBJECT
public:
    ReportDialog(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};

class HelpDialogCheckInputWave : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialogCheckInputWave(QWidget *parent = 0);

signals:

public slots:

};

class HelpDialogTestChip : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialogTestChip(bool isStim, QWidget *parent = 0);

signals:

public slots:

};

class HelpDialogUploadTestStimParameters : public QDialog
{
    Q_OBJECT
public:
    explicit HelpDialogUploadTestStimParameters(QWidget *parent = 0);

signals:

public slots:

};

class TestControlPanel : public AbstractPanel
{
    Q_OBJECT
public:
    explicit TestControlPanel(ControllerInterface* controllerInterface_, AbstractRHXController* rhxController_, SystemState* state_, CommandParser* parser_,
                              MultiColumnDisplay* multiColumnDisplay_, XMLInterface* stimParametersInterface_, ControlWindow* parent=nullptr);
    ~TestControlPanel();

    void rescanPorts();

    void updateForRun() override final;
    void updateForStop() override final;
    void updateSlidersEnabled(YScaleUsed yScaleUsed) override final;
    YScaleUsed slidersEnabled() const override;

    void setCurrentTabName(QString) override final;
    QString currentTabName() const override final;

public slots:
    void checkInputWave();
    void testChip();
    void viewReport();
    void saveReport();
    void uploadStimManual();
    void clearStimManual();

    void updateFromState() override final;

private slots:
    void checkInputWaveHelp();
    void testChipHelp();
    void uploadTestStimParametersHelp();
    void toggleTestAuxInsSlot();
    void changePortComboBoxSlot();

private:
    QHBoxLayout* createSelectionLayout() override final;
    QHBoxLayout* createSelectionToolsLayout() override final;
    QHBoxLayout* createDisplayLayout() override final;

    void updateYScales() override final;

    void generateReport(QVector<QVector<double>> channels);

    QVector<QVector<double>> channels;
    QVector<QVector<QVector<double>>> dcData;

    AbstractRHXController* rhxController;
    MultiColumnDisplay* multiColumnDisplay;
    XMLInterface* stimParametersInterface;

    HelpDialogCheckInputWave *helpDialogCheckInputWave;
    HelpDialogTestChip *helpDialogTestChip;
    HelpDialogUploadTestStimParameters *helpDialogUploadTestStimParameters;

    QPushButton *checkInputWaveButton;

    QLabel *measuredFrequency;
    QLabel *measuredFrequencyFeedback;
    QLabel *measuredAmplitude;
    QLabel *measuredAmplitudeFeedback;

    QPushButton *testChipButton;

    QLineEdit *triangleErrorThresholdLineEdit;
    QLineEdit *settleErrorThresholdLineEdit;
    QLineEdit *stimExpectedVoltageLineEdit;

    QDoubleSpinBox *stimErrorThresholdSpinBox;

    QLabel *reportLabel;
    QPushButton *viewReportButton;
    QPushButton *saveReportButton;
    bool reportPresent;

    QCheckBox* testAuxInsCheckBox;

    QLabel *connectedChannelsLabel;
    ConnectedChannels connectedChannels;

    QPushButton *uploadStimButton;
    //QPushButton *clearStimButton;

    QVector<double> channels_report;
    QVector<double> channels_report_settle;
    QVector<double> channels_report_stim;
    QVector<double> channels_report_pos;
    QVector<double> channels_report_neg;
    QVector<ChannelInfo*> report;

    QComboBox *portComboBox;

    void checkDCWaveforms();
    int calculateVoltages(QVector<double> dcData, QVector<double> &posVoltages, QVector<double> &negVoltages);
    bool posAvgInBounds(double avgVoltage);
    bool negAvgInBounds(double negVoltage);

    void recordDummySegment(double duration, int portIndex);
    void allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                               int xSize, int ySize, int zSize);
    void allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                               int xSize, int ySize);

    int recordShortSegment(QVector<QVector<double>> &channels, double duration, int portIndex,
                           QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames, QVector<QVector<double>> &auxInData);

    int recordDCSegment(double duration, int portIndex, QVector<QVector<QString>> &dcChannelNames);

    int recordFSSegment(QVector<QVector<double>> &channels, double duration, int portIndex, QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames);

    void load16(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData);
    void load32(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData);
    void load64(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData);

    void medianWaveform(QVector<double> &medianChannel, const QVector<QVector<double>> &channels);
    void subtractMatrix(const QVector<double> &minuend, const QVector<double> &subtrahend, QVector<double> &difference);
    double rms(const QVector<double> &waveform);
    double estimateAmplitude(const QVector<double> &waveform);
    double estimateFrequency(double A, const QVector<double> &t, const QVector<double> &waveform);
    double estimatePhase(double f, const QVector<double> &t, const QVector<double> &waveform);
    double rmsError(const QVector<double> &t, const QVector<double> &ytarget, double f, double A, double phase);
    double median(const QVector<double> &arr);
    void addMatrix(const QVector<double> &augend, const QVector<double> &addend, QVector<double> &sum);
    double mean(const QVector<double> &arr);
    void triangle(QVector<double> &y, QVector<double> &t, double f, double A, double phase);
    void baseTriangle(QVector<double> &t, QVector<double> &y);
    void eliminateAverageOffset(QVector<QVector<double> > &channels);
    void eliminateAverageOffset(QVector<double> &channel);
    int amoeba(QVector<double> &t, QVector<double> &ytarget, QVector<QVector<double>> &p, QVector<double> &y, int ndim, double ftol);
    double amotry(QVector<double> &t, QVector<double> &ytarget, QVector<QVector<double> > &p, QVector<double> &y, QVector<double> &psum, int ndim, int ihi, double fac);

    void validateFastSettleChannels(QVector<QVector<double> > &fastSettleChannels, QVector<QVector<double> > &validFastSettleChannels);
    void updateConnectedChannels();

    QVector<int> getConnectedCommandStreams();
    double vectorAvg(QVector<double> vect);
    double vectorAvg(QVector<double> vect, int start, int end);

    bool isOutsideExpectedValue(double actualValue, double expectedValue);

    int ttlOut[16];

    int previousDelay;

    bool testAuxIns;

    double auxIn1Min;
    double auxIn1Max;
    double auxIn1Median;

    double auxIn2Min;
    double auxIn2Max;
    double auxIn2Median;

    double auxIn3Min;
    double auxIn3Max;
    double auxIn3Median;

    double auxInExpectedMin;
    double auxInExpectedMax;
    double auxInExpectedMedian;

    double acceptableDifference;
};

#endif // TESTCONTROLPANEL_H
