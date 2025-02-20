#include "controlpanelconfiguretab.h"
#include "testcontrolpanel.h"
#include "controlwindow.h"

#include <algorithm>

HelpDialogCheckInputWave::HelpDialogCheckInputWave(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Check Input Wave"));

    QLabel *label1 = new QLabel("Scans for a connected chip on Port A and verifies that amplifier inputs are connected to a triangle wave with 100 Hz frequency and 400 " + MicroVoltsSymbol + " amplitude. "
                                "In order for the \"Test Chip\" function to work correctly, this check should be run "
                                "first to ensure that the test circuitry is providing the expected waveform.", this);
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel("Measured frequency should be between 90 Hz and 110 Hz to proceed with chip testing.", this);
    label2->setWordWrap(true);

    QLabel *label3 = new QLabel("Measured amplitude should be between 300 " + MicroVoltsSymbol + " and 600 " + MicroVoltsSymbol + " to proceed with chip testing.", this);
    label3->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);

    resize(HELP_DIALOG_WIDTH, height());
    setLayout(mainLayout);
}

HelpDialogTestChip::HelpDialogTestChip(bool isStim, QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Test Chip"));

    QLabel *label1 = new QLabel("Test connected chip for basic functionality.", this);
    label1->setWordWrap(true);

    QLabel *label2 = new QLabel("Determines RMS Error from each channel compared to a triangle waveform to test AC amplifiers. "
                                "Channels with error above the specified threshold are considered unacceptable.", this);
    label2->setWordWrap(true);

    QLabel *label3;
    if (isStim) {
        label3 = new QLabel("Then uploads stimulation parameters that create a repeating biphasic pulse of 200 positive current units for 400 timesteps, "
                            "200 negative current units for 400 timesteps, and 0 current units for 800 timesteps. "
                            "Default settings are 30 kS/s (33.3 " + MicroSecondsSymbol + " timestep) and 0.5 " + MicroAmpsSymbol + " current step.\n\n"
                            "Reads the on-chip DC amplifiers to verify that these current pulses produce the expected voltage for the expected length of time. Assumes "
                            "that each channel of the chip is connected to a series resistor (e.g., 10 k" + OmegaSymbol + ") that converts the stimulation current to a "
                            "voltage of reasonable magnitude. Channels that show an average positive or negative voltage outside the specified error threshold are considered unacceptable.", this);
    } else {
        label3 = new QLabel("Also measures amplifier readings when amplifier fast settle is enabled, grounding amplifier inputs. "
                            "Variance from ground is displayed as Fast Settle Error.", this);
    }
    label3->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);
    mainLayout->addWidget(label2);
    mainLayout->addWidget(label3);

    resize(HELP_DIALOG_WIDTH, height());
    setLayout(mainLayout);
}

HelpDialogUploadTestStimParameters::HelpDialogUploadTestStimParameters(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Upload Test Stim Parameters"));

    QLabel *label1 = new QLabel("Uploads stimulation parameters that create a repeating biphasic pulse of 200 positive current units for 400 timesteps, "
                                "200 negative current units for 400 timesteps, and 0 current units for 800 timesteps. "
                                "Default settings are 30 kS/s (33.3 " + MicroSecondsSymbol + " timestep) and 0.5 " + MicroAmpsSymbol + " current step.\n\n"
                                "This is automatically done in the Test Chip functionality, but these parameters are then cleared after the test completes. "
                                "If these parameters are uploaded and the board runs, the DC amplifiers should show expected stimulation voltage pulses "
                                "(assuming current is being pushed through a resistor resulting in reasonable voltage levels), while the AC amplifiers will "
                                "become heavily saturated and unable to provide accurate readings until the stimulation parameters are cleared.\n\n"
                                "Stimulation parameters are automatically cleared when Rescan Ports executes.", this);
    label1->setWordWrap(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(label1);

    resize(HELP_DIALOG_WIDTH, height());
    setLayout(mainLayout);
}

TestControlPanel::TestControlPanel(ControllerInterface *controllerInterface_, AbstractRHXController *rhxController_, SystemState *state_, CommandParser *parser_, MultiColumnDisplay *multiColumnDisplay_, XMLInterface *stimParametersInterface_, ControlWindow *parent) :
    AbstractPanel(controllerInterface_, state_, parser_, parent),
    rhxController(rhxController_),
    multiColumnDisplay(multiColumnDisplay_),
    stimParametersInterface(stimParametersInterface_),
    helpDialogCheckInputWave(nullptr),
    helpDialogTestChip(nullptr),
    helpDialogUploadTestStimParameters(nullptr),
    portComboBox(nullptr),
    previousDelay(-1),
    auxIn1Min(3.3),
    auxIn1Max(0),
    auxIn1Median(0),
    auxIn2Min(3.3),
    auxIn2Max(0),
    auxIn2Median(0),
    auxIn3Min(3.3),
    auxIn3Max(0),
    auxIn3Median(0),
    auxInExpectedMin(1.0),
    auxInExpectedMax(2.0),
    auxInExpectedMedian(1.5),
    acceptableDifference(0.2)
{
    reportPresent = false;
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    portComboBox = new QComboBox(this);
    portComboBox->addItems(QStringList({"Port A", "Port B", "Port C", "Port D"}));
    if (state->numSPIPorts == 8) {
        portComboBox->addItems(QStringList({"Port E", "Port F", "Port G", "Port H"}));
    }
    connect(portComboBox, SIGNAL(currentTextChanged(QString)), this, SLOT(changePortComboBoxSlot()));

    testAuxInsCheckBox = new QCheckBox(tr("Test Aux Ins"), this);
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        testAuxInsCheckBox->hide();
    } else {
        connect(testAuxInsCheckBox, SIGNAL(toggled(bool)), this, SLOT(toggleTestAuxInsSlot()));
        testAuxInsCheckBox->setChecked(true);
    }

    QHBoxLayout *portRow = new QHBoxLayout;
    portRow->addWidget(portComboBox);
    portRow->addStretch();
    portRow->addWidget(testAuxInsCheckBox);

    QHBoxLayout *checkInputWaveRow = new QHBoxLayout;
    checkInputWaveButton = new QPushButton(tr("Check Input Waveform"), this);
    checkInputWaveButton->setToolTip("Shortcut: C");
    checkInputWaveButton->setFocusPolicy(Qt::NoFocus);

    QToolButton *helpDialogCheckInputWaveButton = new QToolButton(this);
    helpDialogCheckInputWaveButton->setText("?");
    helpDialogCheckInputWaveButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(helpDialogCheckInputWaveButton, SIGNAL(clicked()), this, SLOT(checkInputWaveHelp()));

    QToolButton *helpDialogTestChipButton = new QToolButton(this);
    helpDialogTestChipButton->setText("?");
    helpDialogTestChipButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(helpDialogTestChipButton, SIGNAL(clicked()), this, SLOT(testChipHelp()));

    QToolButton *helpDialogUploadTestStimParametersButton = new QToolButton(this);
    helpDialogUploadTestStimParametersButton->setText("?");
    helpDialogUploadTestStimParametersButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(helpDialogUploadTestStimParametersButton, SIGNAL(clicked()), this, SLOT(uploadTestStimParametersHelp()));

    checkInputWaveRow->addWidget(checkInputWaveButton);
    checkInputWaveRow->addStretch();
    checkInputWaveRow->addWidget(helpDialogCheckInputWaveButton);
    connect(checkInputWaveButton, SIGNAL(clicked(bool)), this, SLOT(checkInputWave()));

    QHBoxLayout *measuredFrequencyRow = new QHBoxLayout;
    QLabel *measuredFrequencyLabel = new QLabel(tr("Measured Frequency:"), this);
    measuredFrequency = new QLabel("N/A", this);
    measuredFrequencyFeedback = new QLabel("Not tested", this);
    measuredFrequencyFeedback->setStyleSheet("QLabel { color : orange; }");

    measuredFrequencyRow->addStretch();
    measuredFrequencyRow->addWidget(measuredFrequencyLabel);
    measuredFrequencyRow->addWidget(measuredFrequency);
    measuredFrequencyRow->addWidget(measuredFrequencyFeedback);

    QHBoxLayout *measuredAmplitudeRow = new QHBoxLayout;
    QLabel *measuredAmplitudeLabel = new QLabel(tr("Measured Amplitude:"), this);
    measuredAmplitude = new QLabel("N/A", this);
    measuredAmplitudeFeedback = new QLabel("Not tested", this);
    measuredAmplitudeFeedback->setStyleSheet("QLabel { color : orange; }");
    measuredAmplitudeRow->addStretch();
    measuredAmplitudeRow->addWidget(measuredAmplitudeLabel);
    measuredAmplitudeRow->addWidget(measuredAmplitude);
    measuredAmplitudeRow->addWidget(measuredAmplitudeFeedback);

    QVBoxLayout *checkInputWaveLayout = new QVBoxLayout;
    checkInputWaveLayout->addLayout(checkInputWaveRow);
    checkInputWaveLayout->addLayout(measuredFrequencyRow);
    checkInputWaveLayout->addLayout(measuredAmplitudeRow);

    QGroupBox *checkInputWaveGroupBox = new QGroupBox(this);
    checkInputWaveGroupBox->setLayout(checkInputWaveLayout);
    checkInputWaveGroupBox->setStyleSheet("QGroupBox { border: 1px solid gray; border-radius: 9px; margin-top: 0.5em; }");

    QHBoxLayout *testChipRow = new QHBoxLayout;
    testChipButton = new QPushButton(tr("Test Chip"), this);
    testChipButton->setToolTip("Shortcut: T");
    testChipButton->setFocusPolicy(Qt::NoFocus);
    testChipRow->addWidget(testChipButton);
    testChipRow->addStretch();
    testChipRow->addWidget(helpDialogTestChipButton);
    connect(testChipButton, SIGNAL(clicked(bool)), this, SLOT(testChip()));

    QHBoxLayout *triangleErrorThresholdRow = new QHBoxLayout;
    QLabel *triangleErrorThresholdLabel = new QLabel(tr("Triangle Waveform RMS Error Threshold:"), this);
    QString triangleErrorDefaultString = "20.0";
    triangleErrorThresholdLineEdit = new QLineEdit(triangleErrorDefaultString, this);
    triangleErrorThresholdLineEdit->setFixedWidth(40);
    triangleErrorThresholdRow->addStretch();
    triangleErrorThresholdRow->addWidget(triangleErrorThresholdLabel);
    triangleErrorThresholdRow->addWidget(triangleErrorThresholdLineEdit);

    QHBoxLayout *variableErrorThresholdRow = new QHBoxLayout;
    variableErrorThresholdRow->addStretch();

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        QLabel *stimExpectedVoltageLabel = new QLabel("Stim Expected Voltage:", this);
        QLabel *stimErrorThresholdLabel = new QLabel("Stim Error Threshold:", this);
        stimExpectedVoltageLineEdit = new QLineEdit("1.0");
        stimErrorThresholdSpinBox = new QDoubleSpinBox(this);
        stimErrorThresholdSpinBox->setValue(25.0);
        stimErrorThresholdSpinBox->setSuffix("%");
        stimErrorThresholdSpinBox->setSingleStep(1.0);
        stimErrorThresholdSpinBox->setDecimals(1);
        stimErrorThresholdSpinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        stimErrorThresholdSpinBox->setFixedWidth(40);
        stimExpectedVoltageLineEdit->setFixedWidth(40);
        variableErrorThresholdRow->addWidget(stimExpectedVoltageLabel);
        variableErrorThresholdRow->addWidget(stimExpectedVoltageLineEdit);
        variableErrorThresholdRow->addWidget(stimErrorThresholdLabel);
        variableErrorThresholdRow->addWidget(stimErrorThresholdSpinBox);
    } else {
        QLabel *settleErrorThresholdLabel = new QLabel("Settle Error Threshold:", this);
        settleErrorThresholdLineEdit = new QLineEdit("9.0");
        settleErrorThresholdLineEdit->setFixedWidth(40);
        variableErrorThresholdRow->addWidget(settleErrorThresholdLabel);
        variableErrorThresholdRow->addWidget(settleErrorThresholdLineEdit);
    }

    QHBoxLayout *reportRow = new QHBoxLayout;
    reportLabel = new QLabel(tr(""), this);
    viewReportButton = new QPushButton(tr("View Report"), this);
    viewReportButton->setFocusPolicy(Qt::NoFocus);
    viewReportButton->setToolTip("Shortcut: V");
    viewReportButton->setEnabled(false);
    saveReportButton = new QPushButton(tr("Save Report"), this);
    saveReportButton->setFocusPolicy(Qt::NoFocus);
    saveReportButton->setEnabled(false);
    reportRow->addWidget(reportLabel);
    reportRow->addStretch();
    reportRow->addWidget(viewReportButton);
    reportRow->addStretch();
    reportRow->addWidget(saveReportButton);
    reportRow->addStretch();
    connect(viewReportButton, SIGNAL(clicked(bool)), this, SLOT(viewReport()));
    connect(saveReportButton, SIGNAL(clicked(bool)), this, SLOT(saveReport()));

    QVBoxLayout *testChipLayout = new QVBoxLayout;
    testChipLayout->addLayout(testChipRow);
    testChipLayout->addLayout(triangleErrorThresholdRow);
    testChipLayout->addLayout(variableErrorThresholdRow);
    testChipLayout->addLayout(reportRow);

    QGroupBox *testChipGroupBox = new QGroupBox(this);
    testChipGroupBox->setLayout(testChipLayout);
    testChipGroupBox->setStyleSheet("QGroupBox { border: 1px solid gray; border-radius: 9px; margin-top: 0.5em; }");

    connectedChannelsLabel = new QLabel("", this);

    QVBoxLayout *testingGroupLayout = new QVBoxLayout;
    testingGroupLayout->addLayout(portRow);
    testingGroupLayout->addWidget(checkInputWaveGroupBox);
    testingGroupLayout->addWidget(testChipGroupBox);
    testingGroupLayout->addWidget(connectedChannelsLabel);

    QGroupBox *testingGroupBox = new QGroupBox(tr("Chip Testing"), this);
    testingGroupBox->setObjectName("Testing");
    //testingGroupBox->setStyleSheet("#Testing { border: 2px solid gray; border-radius: 9px; margin-top: 0.5em; background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FFFFFF, stop: 1 #DDBBFF); } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3x; }");
    testingGroupBox->setStyleSheet("#Testing { border: 2px solid gray; border-radius: 9px; margin-top: 0.5em; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3x; }");
    testingGroupBox->setLayout(testingGroupLayout);

    QLabel *nameLabel1 = new QLabel(tr("Name:"), this);
    selectionNameLabel = new QLabel(tr("no selection"), this);

    uploadStimButton = new QPushButton(tr("Upload Test Stim Parameters"), this);
    uploadStimButton->setFocusPolicy(Qt::NoFocus);
    uploadStimButton->setToolTip("Upload stimulation parameters so that running triggers test stimulation. Shortcut: U");
    QHBoxLayout *manualStimRow = new QHBoxLayout;
    manualStimRow->addWidget(nameLabel1);
    manualStimRow->addWidget(selectionNameLabel);
    manualStimRow->addStretch();
    manualStimRow->addWidget(uploadStimButton);
    manualStimRow->addWidget(helpDialogUploadTestStimParametersButton);
    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        uploadStimButton->setHidden(true);
        helpDialogUploadTestStimParametersButton->setHidden(true);
    }
    connect(uploadStimButton, SIGNAL(clicked(bool)), this, SLOT(uploadStimManual()));

    filterDisplaySelector->hide();

    tabWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    tabWidget->addTab(configureTab, tr("Config"));

    QVBoxLayout *hotkeysLayout = new QVBoxLayout;
    hotkeysLayout->addWidget(new QLabel("R: Rescan Ports", this));
    hotkeysLayout->addWidget(new QLabel("Spacebar: Run/Stop", this));
    hotkeysLayout->addWidget(new QLabel("C: Check Input Waveform", this));
    hotkeysLayout->addWidget(new QLabel("T: Test Chip", this));
    hotkeysLayout->addWidget(new QLabel("V: View Report", this));
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        hotkeysLayout->addWidget(new QLabel("U: Upload Stim Parameters", this));
    }
    hotkeysLayout->addWidget(new QLabel("< or >: Zoom in/out on time scale", this));
    hotkeysLayout->addWidget(new QLabel("+ or -: Zoom in/out on voltage scale", this));
    hotkeysLayout->addWidget(new QLabel("Mouse wheel: Scroll through channels", this));
    hotkeysLayout->addWidget(new QLabel("Ctrl + Mouse wheel: adjust spacing of channels", this));


    QGroupBox *hotkeysBox = new QGroupBox("Keyboard Hotkeys", this);
    hotkeysBox->setLayout(hotkeysLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(testingGroupBox);
    mainLayout->addLayout(manualStimRow);
    mainLayout->addStretch();
    mainLayout->addLayout(createSelectionLayout());
    mainLayout->addLayout(createSelectionToolsLayout());
    mainLayout->addWidget(filterDisplaySelector);
    mainLayout->addLayout(createDisplayLayout());
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(hotkeysBox);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setMinimumWidth(mainWidget->sizeHint().width());

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    YScaleUsed yScaleUsed;
    updateSlidersEnabled(yScaleUsed);

    updateFromState();
    setFocus();
}

TestControlPanel::~TestControlPanel()
{
    if (helpDialogCheckInputWave) delete helpDialogCheckInputWave;
    if (helpDialogTestChip) delete helpDialogTestChip;
    if (helpDialogUploadTestStimParameters) delete helpDialogUploadTestStimParameters;
}

void TestControlPanel::updateForRun()
{
    configureTab->updateForRun();
    checkInputWaveButton->setEnabled(false);
    testChipButton->setEnabled(false);
    uploadStimButton->setEnabled(false);
    viewReportButton->setEnabled(false);
    saveReportButton->setEnabled(false);
}

void TestControlPanel::updateForStop()
{
    setEnabled(true);

    configureTab->updateForStop();
    checkInputWaveButton->setEnabled(true);
    testChipButton->setEnabled(true);
    uploadStimButton->setEnabled(true);
    if (reportPresent) {
        viewReportButton->setEnabled(true);
        saveReportButton->setEnabled(true);
    }
}

void TestControlPanel::updateSlidersEnabled(YScaleUsed yScaleUsed)
{
    wideSlider->setEnabled(yScaleUsed.widepassYScaleUsed);
    wideLabel->setEnabled(yScaleUsed.widepassYScaleUsed);
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        variableSlider->setEnabled(yScaleUsed.dcYScaleUsed);
        variableLabel->setEnabled(yScaleUsed.dcYScaleUsed);
    }
}

YScaleUsed TestControlPanel::slidersEnabled() const
{
    YScaleUsed yScaleUsed;
    yScaleUsed.widepassYScaleUsed = wideSlider->isEnabled();
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        yScaleUsed.dcYScaleUsed = variableSlider->isEnabled();
    }
    return yScaleUsed;
}

void TestControlPanel::setCurrentTabName(QString)
{
    tabWidget->setCurrentWidget(configureTab);
}

QString TestControlPanel::currentTabName() const
{
    return tr("Config");
}

void TestControlPanel::checkInputWaveHelp()
{
    if (!helpDialogCheckInputWave) {
        helpDialogCheckInputWave = new HelpDialogCheckInputWave(this);
    }
    helpDialogCheckInputWave->show();
    helpDialogCheckInputWave->raise();
    helpDialogCheckInputWave->activateWindow();
}

void TestControlPanel::testChipHelp()
{
    if (!helpDialogTestChip) {
        helpDialogTestChip = new HelpDialogTestChip(state->getControllerTypeEnum() == ControllerStimRecord, this);
    }
    helpDialogTestChip->show();
    helpDialogTestChip->raise();
    helpDialogTestChip->activateWindow();
}

void TestControlPanel::uploadTestStimParametersHelp()
{
    if (!helpDialogUploadTestStimParameters) {
        helpDialogUploadTestStimParameters = new HelpDialogUploadTestStimParameters(this);
    }
    helpDialogUploadTestStimParameters->show();
    helpDialogUploadTestStimParameters->raise();
    helpDialogUploadTestStimParameters->activateWindow();
}

void TestControlPanel::toggleTestAuxInsSlot()
{
    state->testAuxIns->setValue(testAuxInsCheckBox->isChecked());
}

void TestControlPanel::changePortComboBoxSlot()
{
    state->testingPort->setValue(portComboBox->currentText().right(1));
}

void TestControlPanel::updateFromState()
{
    state->signalSources->getSelectedSignals(selectedSignals);

    filterDisplaySelector->updateFromState();
    updateSelectionName();
    updateElectrodeImpedance();
    updateReferenceChannel();
    updateColor();
    updateEnableCheckBox();
    updateYScales();

    clipWaveformsCheckBox->setChecked(state->clipWaveforms->getValue());
    timeScaleComboBox->setCurrentIndex(state->tScale->getIndex());

    configureTab->updateFromState();

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        updateStimTrigger();
        updateStimParamDialogButton();
    }
}

void TestControlPanel::updateYScales()
{
    wideSlider->setValue(state->yScaleWide->getIndex());
}

QHBoxLayout* TestControlPanel::createSelectionLayout()
{
    enableCheckBox = new QCheckBox(tr("Enable"), this);
    enableCheckBox->hide();
    connect(enableCheckBox, SIGNAL(clicked()), this, SLOT(enableChannelsSlot()));

    colorAttribute = new ColorWidget(this);
    colorAttribute->hide();
    connect(colorAttribute, SIGNAL(clicked()), this, SLOT(promptColorChange()));

    selectionImpedanceLabel = new QLabel(tr("no selection"), this);
    selectionImpedanceLabel->hide();
    selectionReferenceLabel = new QLabel(tr("no selection"), this);
    selectionReferenceLabel->hide();
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        selectionStimTriggerLabel = new QLabel(tr("no selection"), this);
        selectionStimTriggerLabel->hide();
    }

    renameButton  = new QPushButton(tr("Rename"), this);
    renameButton->hide();
    renameButton->setFixedWidth(renameButton->fontMetrics().horizontalAdvance(renameButton->text()) + 14);
    connect(renameButton, SIGNAL(clicked()), controlWindow, SLOT(renameChannel()));

    setRefButton  = new QPushButton(tr("Set Ref"), this);
    setRefButton->hide();
    setRefButton->setFixedWidth(setRefButton->fontMetrics().horizontalAdvance(setRefButton->text()) + 14);
    connect(setRefButton, SIGNAL(clicked()), controlWindow, SLOT(setReference()));

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        setStimButton = new QPushButton(tr("Set Stim"), this);
        setStimButton->hide();
        setStimButton->setFixedWidth(setStimButton->fontMetrics().horizontalAdvance(setStimButton->text()) + 14);
        connect(setStimButton, SIGNAL(clicked()), this, SLOT(openStimParametersDialog()));
    }

    QGridLayout* selectionGrid = new QGridLayout;
    selectionGrid->setColumnMinimumWidth(1, 90);

    QHBoxLayout *selectionLayout = new QHBoxLayout;
    selectionLayout->addLayout(selectionGrid);
    selectionLayout->addStretch(1);
    return selectionLayout;
}

QHBoxLayout* TestControlPanel::createSelectionToolsLayout()
{
    QHBoxLayout *selectionToolsLayout = new QHBoxLayout;
    selectionToolsLayout->addWidget(enableCheckBox);
    selectionToolsLayout->addWidget(colorAttribute);
    selectionToolsLayout->addWidget(renameButton);
    selectionToolsLayout->addWidget(setRefButton);
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        selectionToolsLayout->addWidget(setStimButton);
    }
    return selectionToolsLayout;
}

QHBoxLayout* TestControlPanel::createDisplayLayout()
{
    timeScaleComboBox = new QComboBox(this);
    state->tScale->setupComboBox(timeScaleComboBox);
    connect(timeScaleComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeTimeScale(int)));

    clipWaveformsCheckBox = new QCheckBox(tr("Clip Waves"), this);
    clipWaveformsCheckBox->setFocusPolicy(Qt::NoFocus);
    connect(clipWaveformsCheckBox, SIGNAL(checkStateChanged(Qt::CheckState)), this, SLOT(clipWaveforms(Qt::CheckState)));

    QVBoxLayout *timeScaleColumn = new QVBoxLayout;
    timeScaleColumn->addWidget(clipWaveformsCheckBox);
    timeScaleColumn->addWidget(new QLabel(tr("Time Scale"), this));
    timeScaleColumn->addWidget(timeScaleComboBox);

    const int SliderHeight = 50;

    wideSlider = new QSlider(Qt::Vertical, this);
    wideSlider->setFixedHeight(SliderHeight);
    wideSlider->setRange(0, state->yScaleWide->numberOfItems() - 1);
    wideSlider->setInvertedAppearance(true);
    wideSlider->setInvertedControls(true);
    wideSlider->setPageStep(1);
    wideSlider->setValue(state->yScaleWide->getIndex());
    wideSlider->setTickPosition(QSlider::TicksRight);
    connect(wideSlider, SIGNAL(valueChanged(int)), this, SLOT(changeWideScale(int)));

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        variableSlider = new QSlider(Qt::Vertical, this);
        variableSlider->setFixedHeight(SliderHeight);
        variableSlider->setRange(0, state->yScaleDC->numberOfItems() - 1);
        connect(variableSlider, SIGNAL(valueChanged(int)), this, SLOT(changeDCScale(int)));
        variableSlider->setInvertedAppearance(true);
        variableSlider->setInvertedControls(true);
        variableSlider->setPageStep(1);
        variableSlider->setTickPosition(QSlider::TicksRight);
    }


    wideLabel = new QLabel(tr("WIDE"), this);

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        variableLabel = new QLabel(tr("DC"), this);
    }

    QVBoxLayout *wideColumn = new QVBoxLayout;
    wideColumn->addWidget(wideSlider);
    wideColumn->setAlignment(wideSlider, Qt::AlignHCenter);
    wideColumn->addWidget(wideLabel);
    wideColumn->setAlignment(wideLabel, Qt::AlignHCenter);


    QVBoxLayout *variableColumn = new QVBoxLayout;
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        variableColumn->addWidget(variableSlider);
        variableColumn->setAlignment(variableSlider, Qt::AlignHCenter);
        variableColumn->addWidget(variableLabel);
        variableColumn->setAlignment(variableLabel, Qt::AlignHCenter);
    }

    QGridLayout *displayGrid = new QGridLayout;
    displayGrid->setHorizontalSpacing(0);
    displayGrid->addLayout(wideColumn, 0, 0);
    displayGrid->addLayout(variableColumn, 0, 3);

    QHBoxLayout* displayLayout = new QHBoxLayout;
    displayLayout->addLayout(displayGrid);
    displayLayout->addLayout(timeScaleColumn);
    return displayLayout;
}








void TestControlPanel::rescanPorts()
{
    configureTab->rescanPorts();
}


void TestControlPanel::checkInputWave()
{
    configureTab->rescanPorts();

    int numPorts = 0;
    int portIndex = 0;
    for (int i = 0; i < state->signalSources->numPortGroups(); ++i) {
        if (state->signalSources->portGroupByIndex(i)->numAmpChannels->getValue() > 0) {
            numPorts++;
            portIndex = i;
        }
    }

    if (numPorts != 1) {
        QMessageBox::critical(this, "Incorrect Number of Connected Ports",
                              "Cannot check parameters of input triangle wave with more or less than 1 port at a time.");
        return;
    }

    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        configureTab->fastSettleCheckBox->setChecked(false);
    }

    // Default setting for time scale, 400 ms is suitable for a 100 Hz wave.
    timeScaleComboBox->setCurrentIndex(5);
    state->yScaleWide->setValue("600");

    // Run for 0.6 seconds for dummy data
    QProgressDialog progress(QObject::tr("Checking Input Wave"), QString(), 0, 6);
    progress.setWindowTitle(tr("Progress"));
    progress.setMinimumDuration(0);
    progress.setModal(true);
    for (int i = 0; i < 6; ++i) {
        recordDummySegment(0.1, portIndex);
        progress.show();
        progress.setValue(i);
    }

    // Load short segment of data into 'channels' vector and 'ampData' vector
    QVector<QVector<double>> channels;
    QVector<QVector<QVector<double>>> ampData;
    QVector<QVector<QString>> ampChannelNames;
    QVector<QVector<double>> auxInData;
    int numSamples = recordShortSegment(channels, 0.4, portIndex, ampData, ampChannelNames, auxInData);

    // Load this segment into plotter
    multiColumnDisplay->loadWaveformDataDirectAmp(ampData, ampChannelNames, auxInData);

    // Determine median waveform of the channels
    QVector<double> median;
    median.resize(numSamples);
    medianWaveform(median, channels);

    // Estimate the amplitude of the median waveform
    double A = estimateAmplitude(median);

    // Create a 'time' vector
    QVector<double> t;
    t.resize(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        t[i] = i * (1 / state->sampleRate->getNumericValue());
    }

    // Estimate the frequency of the median waveform
    double f = estimateFrequency(A, t, median);

    measuredFrequency->setText(QString::number((int) round(f)) + " Hz");
    measuredAmplitude->setText(QString::number((int) round(A)) + " uV");

    QString styleSheetRed("QLabel {color : red; }");
    QString styleSheetGreen("QLabel {color : green; }");

    if (f < 90) {
        measuredFrequencyFeedback->setText("Low: target 100 Hz");
        measuredFrequencyFeedback->setStyleSheet(styleSheetRed);
    } else if (f > 110) {
        measuredFrequencyFeedback->setText("High: target 100 Hz");
        measuredFrequencyFeedback->setStyleSheet(styleSheetRed);
    } else {
        measuredFrequencyFeedback->setText("Good: target 100 Hz");
        measuredFrequencyFeedback->setStyleSheet(styleSheetGreen);
    }

    if (A < 300) {
        measuredAmplitudeFeedback->setText("Low: target 400 uV");
        measuredAmplitudeFeedback->setStyleSheet(styleSheetRed);
    } else if (A > 600) {
        measuredAmplitudeFeedback->setText("High: target 400 uV");
        measuredAmplitudeFeedback->setStyleSheet(styleSheetRed);
    } else {
        measuredAmplitudeFeedback->setText("Good: target 400 uV");
        measuredAmplitudeFeedback->setStyleSheet(styleSheetGreen);
    }
}

//Record 'duration' seconds of data that is just placed in a dummy vector and not used
void TestControlPanel::recordDummySegment(double duration, int portIndex)
{
    QVector<QVector<double>> dummy;

    // Disable external fast settling, since this interferes with DAC commands in AuxCmd1.
    controllerInterface->enableExternalFastSettle(false);

    // Turn LEDs on to indicate that data acquisition is running.
    for (int i = 0; i < 16; ++i) {
        ttlOut[i] = 0;
    }
    ttlOut[15] = 1;
    int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};

    rhxController->setLedDisplay(ledArray);
    rhxController->setTtlOut(ttlOut);

    // Record for duration - translate this to number of data blocks
    double samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    double sample_period = 1 / state->sampleRate->getNumericValue();
    double datablock_period = samplesPerDataBlock * sample_period;
    int numBlocks = qCeil(duration / datablock_period);
    int numSamples = samplesPerDataBlock * numBlocks;

    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(numSamples);

    // Determine number of channels and streams based on the type of chip plugged in
    int numChannels = state->signalSources->portGroupByIndex(portIndex)->numAmpChannels->getValue();
    int numStreams = 0;
    int channelsPerStream = RHXDataBlock::channelsPerStream(state->getControllerTypeEnum());

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        if (numChannels == 16) {
            numStreams = 1;
        } else if (numChannels == 32) {
            numStreams = 2;
        } else {
            QMessageBox::critical(this, "Unexpected Number of Connected Channels",
                    "Cannot check parameters of input triangle wave with the unexpected number of " + QString::number(numChannels) + " channels.");
            return;
        }
    }

    else {
        if (numChannels == 16) {
            numStreams = 1;
            channelsPerStream = 16;
        } else if (numChannels == 32) {
            numStreams = 1;
        } else if (numChannels == 64) {
            numStreams = 2;
        } else if (numChannels == 128) {
            numStreams = 4;
        } else {
            QMessageBox::critical(this, "Unexpected Number of Connected Channels",
                    "Cannot check parameters of input triangle wave with the unexpected number of " + QString::number(numChannels) + " channels.");
            return;
        }
    }

    // Put duration of data into amplifierPreFilter.
    rhxController->run();
    while (rhxController->isRunning()) {
        qApp->processEvents();
        //Possibly put in progress bar and LED increment here
    }

    std::deque<RHXDataBlock*> dataQueue;
    rhxController->readDataBlocks(numBlocks, dataQueue);

    // BEGIN SIMPLIFY LOADAMPLIFIERDATA
    QVector<QVector<QVector<double>>> ampData;
    allocateDoubleArray3D(ampData, numStreams, channelsPerStream, numSamples);

    int block, t, channel, stream;
    int indexAmp = 0;
    for (block = 0; block < numBlocks; ++block) {
        // Load and scale amplifier waveforms (sampled at amplifier sampling rate)
        for (t = 0; t < samplesPerDataBlock; ++t) {
            for (channel = 0; channel < channelsPerStream; ++channel) {
                for (stream = 0; stream < numStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    ampData[stream][channel][indexAmp] = 0.195 * (dataQueue.front()->amplifierData(stream, channel, t) - 32768);
                }
            }
            ++indexAmp;
        }
        // We are done with this RHXDataBlock object; remove it from dataQueue
        dataQueue.pop_front();
    }

    // END SIMPLIFY LOADAMPLIFIERDATA

    // Load data into 'dummy' vector
    // Size channels vector
    dummy.resize(numChannels);
    for (int channel = 0; channel < numChannels; channel++) {
        dummy[channel].resize(numSamples);
    }

    for (int stream = 0; stream < numStreams; stream++) {
        for (int channel = 0; channel < channelsPerStream; channel++) {
            for (int sample = 0; sample < numSamples; sample++) {
                dummy[(stream * channelsPerStream) + channel][sample] = ampData[stream][channel][sample];
            }
        }
    }
    return;
}

// Allocates memory for a 3-D array of doubles.
void TestControlPanel::allocateDoubleArray3D(QVector<QVector<QVector<double> > > &array3D,
                                            int xSize, int ySize, int zSize)
{
    if (xSize == 0) return;
    array3D.resize(xSize);
    for (int i = 0; i < xSize; ++i) {
        array3D[i].resize(ySize);
        for (int j = 0; j < ySize; ++j) {
            array3D[i][j].resize(zSize);
        }
    }
}

// Allocates memory for a 2-D array of doubles.
void TestControlPanel::allocateDoubleArray2D(QVector<QVector<double> > &array2D,
                                            int xSize, int ySize)
{
    if (xSize == 0) return;
    array2D.resize(xSize);
    for (int i = 0; i < xSize; ++i) {
        array2D[i].resize(ySize);
    }
}

void TestControlPanel::updateConnectedChannels()
{
    QString message;
    switch (connectedChannels) {
    case Inner16:
        message = "Test assumes inner 16 channels are connected."; break;
    case Outer16:
        message = "Test assumes outer 16 channels are connected."; break;
    case All16:
        message = "Test assumes all 16 channels are connected."; break;
    case All32:
        message = "Test assumes all 32 channels are connected."; break;
    case Inner32:
        message = "Test assumes inner 32 channels are connected."; break;
    case Outer32:
        message = "Test assumes outer 32 channels are connected."; break;
    case All64:
        message = "Test assumes all 64 channels are connected."; break;
    case All128:
        message = "Test assumes all 128 channels are connected."; break;
    }
    connectedChannelsLabel->setText(message);
}

// Record a short segment of 'duration' seconds, placing the data in 'channels' vector
// A test is run for 64-channel chips (inside the load64() function) to determine if only the inner 32 channels should be considered, or only the outer 32, or all 64
int TestControlPanel::recordShortSegment(QVector<QVector<double> > &channels, double duration, int portIndex,
                                         QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames, QVector<QVector<double>> &auxInData)
{
    // Disable external fast settling, since this interferes with DAC commands in AuxCmd1.
    rhxController->enableExternalFastSettle(false);

    // Turn LEDs on to indicate that data acquisition is running.
    ttlOut[15] = 1;
    int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    rhxController->setLedDisplay(ledArray);
    rhxController->setTtlOut(ttlOut);

    // Record for duration - translate this to number of data blocks
    double samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    double sample_period = 1 / state->sampleRate->getNumericValue();
    double datablock_period = samplesPerDataBlock * sample_period;
    int numBlocks = qCeil(duration / datablock_period);
    int numSamples = samplesPerDataBlock * numBlocks;

    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(numSamples);

    // Determine number of channels and streams based on the type of chip plugged in
    int numChannels = state->signalSources->portGroupByIndex(portIndex)->numAmpChannels->getValue();
    int numAuxChannels = state->signalSources->portGroupByIndex(portIndex)->numAuxChannels->getValue();
    int numStreams = 0;
    int channelsPerStream = RHXDataBlock::channelsPerStream(state->getControllerTypeEnum());

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        if (numChannels == 16) {
            numStreams = 1;
        } else if (numChannels == 32) {
            numStreams = 2;
        }
    }

    else {
        if (numChannels == 16) {
            numStreams = 1;
            channelsPerStream = 16;
        } else if (numChannels == 32) {
            numStreams = 1;
        } else if (numChannels == 64) {
            numStreams = 2;
        } else if (numChannels == 128) {
            numStreams = 4;
        }
    }

    // Put duration of data into amplifierPreFilter.
    rhxController->run();
    while (rhxController->isRunning()) {
        qApp->processEvents();
        //Possibly put in progress bar and LED increment here
    }

    std::deque<RHXDataBlock*> dataQueue;
    rhxController->readDataBlocks(numBlocks, dataQueue);

    // BEGIN SIMPLIFY LOADAMPLIFIERDATA
    allocateDoubleArray3D(ampData, numStreams, channelsPerStream, numSamples);
    allocateDoubleArray2D(auxInData, numAuxChannels, numSamples / 4);
    ampChannelNames.resize(numStreams);
    for (int i = 0; i < ampChannelNames.size(); ++i) {
        ampChannelNames[i].resize(channelsPerStream);
    }

    for (int stream = 0; stream < numStreams; ++stream) {
        for (int channel = 0; channel < channelsPerStream; ++channel) {
            ampChannelNames[stream][channel] = state->signalSources->getAmplifierChannel(stream, channel)->getNativeName() + "|WIDE";
        }
    }

    int block, t, channel, stream;
    int indexAmp = 0, indexAux = 0;
    for (block = 0; block < numBlocks; ++block) {
        // Load and scale amplifier waveforms (sampled at amplifier sampling rate)
        for (t = 0; t < samplesPerDataBlock; ++t) {
            for (channel = 0; channel < channelsPerStream; ++channel) {
                for (stream = 0; stream < numStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    ampData[stream][channel][indexAmp] = 0.195 * (dataQueue.front()->amplifierData(stream, channel, t) - 32768);
                }
            }
            ++indexAmp;
        }

        // Load and scale aux waveforms (sampled at 1/4 amplifier sampling rate)
        for (t = 0; t < samplesPerDataBlock / 4; ++t) {
            // Aux waveform units = volts
            // t + 0 returns 73 (read from ROM), t + 1 returns AuxIn1, t + 2 returns AuxIn2, t + 3 returns AuxIn3
            if (state->getControllerTypeEnum() != ControllerStimRecord) {
                auxInData[0][indexAux] = 0.0000374F * (dataQueue.front()->auxiliaryData(0, 1, 4 * t + 1));
                auxInData[1][indexAux] = 0.0000374F * (dataQueue.front()->auxiliaryData(0, 1, 4 * t + 2));
                auxInData[2][indexAux] = 0.0000374F * (dataQueue.front()->auxiliaryData(0, 1, 4 * t + 3));
            }
            ++indexAux;
        }
        // We are done with this RHXDataBlock object; remove it from dataQueue
        dataQueue.pop_front();
    }
    // END SIMPLIFY LOADAMPLIFIERDATA

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        if (numChannels == 32) {
            load32(numSamples, channels, ampData);
        }

        else {
            load16(numSamples, channels, ampData);
        }
    }

    else {
        // Load data into 'channels' vector which will have the triangle check done to it - unique for 64-channel, which may look at 32 channels at a time
        if (numChannels == 64) {
            load64(numSamples, channels, ampData);
        }

        // Load data into 'channels' vector which will have the triangle check done to it - unique for 32-channel, which may look at inner 16 channels
        else if (numChannels == 32) {
            load32(numSamples, channels, ampData);
        }

        // Load data into 'channels' vector which will have the triangle check done to it - for true 16-channel chip, look at all channels
        else if (numChannels == 16) {
            load16(numSamples, channels, ampData);
        }

        // Load data into 'channels' vector which will have the triangle check done to it - for non-64 channel headstages, look at all channels
        else if (numChannels == 128) {
            // Size channels vector
            connectedChannels = All128;
            channels.resize(numChannels);
            for (int channel = 0; channel < numChannels; ++channel) {
                channels[channel].resize(numSamples);
            }

            // Load data into channels vector
            for (int stream = 0; stream < numStreams; ++stream) {
                for (int channel = 0; channel < channelsPerStream; ++channel) {
                    for (int sample = 0; sample < numSamples; ++sample) {
                        channels[(stream * channelsPerStream) + channel][sample] = ampData[stream][channel][sample];
                    }
                }
            }
        }

        else {
            qDebug() << "ERROR... UNRECOGNIZED NUM CHANNELS: " << numChannels;
        }
    }

    updateConnectedChannels();
    return numSamples;
}

int TestControlPanel::recordDCSegment(double duration, int portIndex, QVector<QVector<QString>> &dcChannelNames)
{
    // Turn LEDs on to indicate that data acquisition is running.
    ttlOut[15] = 1;
    int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    rhxController->setLedDisplay(ledArray);
    rhxController->setTtlOut(ttlOut);

    // Record for duration - translate this to number of data blocks
    double samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    double sample_period = 1 / state->sampleRate->getNumericValue();
    double datablock_period = samplesPerDataBlock * sample_period;
    int numBlocks = qCeil(duration / datablock_period);
    int numSamples = samplesPerDataBlock * numBlocks;

    rhxController->setStimCmdMode(true);
    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(numSamples);

    // Determine number of channels and streams based on the type of chip plugged in
    int numChannels = state->signalSources->portGroupByIndex(portIndex)->numAmpChannels->getValue();
    int numStreams = 0;
    int channelsPerStream = RHXDataBlock::channelsPerStream(state->getControllerTypeEnum());
    if (numChannels == 16) {
        numStreams = 1;
    } else if (numChannels == 32) {
        numStreams = 2;
    }

    // Put duration of data into amplifierPreFilter.
    rhxController->run();
    while (rhxController->isRunning()) {
        qApp->processEvents();
        //Possibly put in progress bar and LED increment here
    }
    rhxController->setStimCmdMode(false);

    std::deque<RHXDataBlock*> dataQueue;
    rhxController->readDataBlocks(numBlocks, dataQueue);

    // BEGIN SIMPLIFY LOADAMPLIFIERDATA
    allocateDoubleArray3D(dcData, numStreams, channelsPerStream, numSamples);
    dcChannelNames.resize(numStreams);
    for (int i = 0; i < dcChannelNames.size(); ++i) {
        dcChannelNames[i].resize(channelsPerStream);
    }

    for (int stream = 0; stream < numStreams; ++stream) {
        for (int channel = 0; channel < channelsPerStream; ++channel) {
            dcChannelNames[stream][channel] = state->signalSources->getAmplifierChannel(stream, channel)->getNativeName() + "|DC";
        }
    }

    int block, t, channel, stream;
    int indexAmp = 0;
    for (block = 0; block < numBlocks; ++block) {
        // Load and scale amplifier waveforms (sampled at amplifier sampling rate)
        for (t = 0; t < samplesPerDataBlock; ++t) {
            for (channel = 0; channel < channelsPerStream; ++channel) {
                for (stream = 0; stream < numStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    dcData[stream][channel][indexAmp] = -0.01923F * (dataQueue.front()->dcAmplifierData(stream, channel, t) - 512);
                }
            }
            ++indexAmp;
        }
        // We are done with this RHXDataBlock object; remove it from dataQueue
        dataQueue.pop_front();
    }
    // END SIMPLIFY LOADAMPLIFIERDATA

    return numSamples;
}

int TestControlPanel::recordFSSegment(QVector<QVector<double> > &channels, double duration, int portIndex, QVector<QVector<QVector<double>>> &ampData, QVector<QVector<QString>> &ampChannelNames)
{
    //Disable external fast settling, since this intereres with DAC commands in AuxCmd1.
    rhxController->enableExternalFastSettle(false);

    //Turn LEDs on to indicate that data acquisition is running.
    ttlOut[15] = 1;
    int ledArray[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    rhxController->setLedDisplay(ledArray);
    rhxController->setTtlOut(ttlOut);

    //Record for duration - translate this to number of data blocks (should be 34 for 0.1 s)
    double samplesPerDataBlock = RHXDataBlock::samplesPerDataBlock(state->getControllerTypeEnum());
    double sample_period = 1 / state->sampleRate->getNumericValue();
    double datablock_period = samplesPerDataBlock * sample_period;
    int numBlocks = qCeil(duration / datablock_period);
    int numSamples = samplesPerDataBlock * numBlocks;

    rhxController->setContinuousRunMode(false);
    rhxController->setMaxTimeStep(numSamples);

    //Determine number of channels and streams based on the type of chip plugged in
    int numChannels = state->signalSources->portGroupByIndex(portIndex)->numAmpChannels->getValue();
    int numStreams = 0;
    int channelsPerStream = RHXDataBlock::channelsPerStream(state->getControllerTypeEnum());

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        if (numChannels == 16) {
            numStreams = 1;
        } else if (numChannels == 32) {
            numStreams = 2;
        }
    }

    else {
        if (numChannels == 16) {
            numStreams = 1;
            channelsPerStream = 16;
        } else if (numChannels == 32) {
            numStreams = 1;
        } else if (numChannels == 64) {
            numStreams = 2;
        } else if (numChannels == 128) {
            numStreams = 4;
        }
    }

    // Put 0.1 s of data
    rhxController->run();
    while (rhxController->isRunning()) {
        qApp->processEvents();
        // Possibly put in progress bar and LED increment here
    }

    std::deque<RHXDataBlock*> dataQueue;
    rhxController->readDataBlocks(numBlocks, dataQueue);

    // BEGIN SIMPLIFY LOADAMPLIFIERDATA
    allocateDoubleArray3D(ampData, numStreams, channelsPerStream, numSamples);
    ampChannelNames.resize(numStreams);
    for (int i = 0; i < ampChannelNames.size(); ++i) {
        ampChannelNames[i].resize(channelsPerStream);
    }

    for (int stream = 0; stream < numStreams; ++stream) {
        for (int channel = 0; channel < channelsPerStream; ++channel) {
            ampChannelNames[stream][channel] = state->signalSources->getAmplifierChannel(stream, channel)->getNativeName() + "|WIDE";
        }
    }

    int block, t, channel, stream;
    int indexAmp = 0;
    for (block = 0; block < numBlocks; ++block) {
        // Load and scale amplifier waveforms (sampled at amplifier sampling rate)
        for (t = 0; t < samplesPerDataBlock; ++t) {
            for (channel = 0; channel < channelsPerStream; ++channel) {
                for (stream = 0; stream < numStreams; ++stream) {
                    // Amplifier waveform units = microvolts
                    ampData[stream][channel][indexAmp] = 0.195 * (dataQueue.front()->amplifierData(stream, channel, t) - 32768);
                }
            }
            ++indexAmp;
        }
        // We are done with this RHXDataBlock object; remove it from dataQueue
        dataQueue.pop_front();
    }
    // END SIMPLIFY LOADAMPLIFIERDATA

    //Load data into 'channels' vector
    channels.resize(numChannels);
    for (int channel = 0; channel < numChannels; channel++) {
        channels[channel].resize(numSamples);
    }

    for (int stream = 0; stream < numStreams; stream++) {
        for (int channel = 0; channel < channelsPerStream; channel++) {
            for (int sample = 0; sample < numSamples; sample++) {
                channels[(stream * channelsPerStream) + channel][sample] = ampData[stream][channel][sample];
            }
        }
    }
    return numSamples;
}

void TestControlPanel::load16(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData)
{
    int numChannels = 16;
    channels.resize(numChannels);
    for (int channel = 0; channel < numChannels; ++channel) {
        channels[channel].resize(numSamples);
    }

    // Load data into channels vector
    for (int channel = 0; channel < numChannels; ++channel) {
        for (int sample = 0; sample < numSamples; ++sample) {
            channels[channel][sample] = ampData[0][channel][sample];
        }
    }
    connectedChannels = All16;
    return;
}


//Load data from a 32-channel chip into channels
//Run a test to determine if the inner 16 channels or all 32 channels should be loaded
void TestControlPanel::load32(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData)
{
    //Load inner and outer 16 channels into innerChannels and outerChannels vectors
    QVector<QVector<double>> innerChannels;
    QVector<QVector<double>> outerChannels;
    innerChannels.resize(16);
    outerChannels.resize(16);
    for (int channel = 0; channel < 16; channel++) {
        innerChannels[channel].resize(numSamples);
        outerChannels[channel].resize(numSamples);
    }

    for (int sample = 0; sample < numSamples; sample++) {
        for (int channel = 0; channel < 16; channel++) {
            if (state->getControllerTypeEnum() == ControllerStimRecord) {
                innerChannels[channel][sample] = ampData[0][channel][sample];
                outerChannels[channel][sample] = ampData[1][channel][sample];
            } else {
                innerChannels[channel][sample] = ampData[0][channel + 8][sample];
                outerChannels[channel][sample] = ampData[0][channel < 8 ? channel : channel + 16][sample];
            }
        }
    }

    //Get inner and outer median waveforms
    QVector<double> innerMedian;
    QVector<double> outerMedian;
    innerMedian.resize(numSamples);
    outerMedian.resize(numSamples);
    medianWaveform(innerMedian, innerChannels);
    medianWaveform(outerMedian, outerChannels);

    //Subtract the two median waveforms to get the residual waveform
    QVector<double> residual;
    residual.resize(numSamples);
    subtractMatrix(innerMedian, outerMedian, residual);

    //Take absolute value of all samples in residual waveform
    for (int sample = 0; sample < numSamples; sample++) {
        residual[sample] = abs(residual[sample]);
    }

    //Calculate the rms
    double rms_residual = rms(residual);

    //If rms_residual is below the threshold, resize channels to 32 and load data
    if (rms_residual < 10 || state->getControllerTypeEnum() == ControllerStimRecord) {
        connectedChannels = All32;
        channels.resize(32);
        for (int channel = 0; channel < 32; channel++) {
            channels[channel].resize(numSamples);
        }

        //Load data into channels vector
        for (int channel = 0; channel < 32; channel++) {
            for (int sample = 0; sample < numSamples; sample++) {
                if (state->getControllerTypeEnum() == ControllerStimRecord) {
                    channels[channel][sample] = ampData[channel / 16][channel % 16][sample];
                } else {
                    channels[channel][sample] = ampData[0][channel][sample];
                }
            }
        }
        return;
    }

    //If above, try to best-fit inner 16 and outer 16, load whichever is lowest
    //Estimate amplitude of median waveforms
    double inner_A = estimateAmplitude(innerMedian);
    double outer_A = estimateAmplitude(outerMedian);

    //Detected amplitude outside of good range, so let other 16 channels be loaded in
    if ((inner_A < 200) || (outer_A < 200) || (inner_A > 600) || (outer_A > 600)) {

        channels.resize(16);
        for (int channel = 0; channel < 16; channel++) {
            channels[channel].resize(numSamples);
        }

        for (int channel = 0; channel < 16; channel++) {
            for (int sample = 0; sample < numSamples; sample++) {
                //Inner channels have bad amplitude
                if ((inner_A < 200) || (inner_A > 600)) {
                    channels[channel][sample] = outerChannels[channel][sample];
                }
                //Outer channels have bad amplitude
                else {
                    channels[channel][sample] = innerChannels[channel][sample];
                }
            }
        }

        if ((inner_A < 200) || (inner_A > 600)) {
            connectedChannels = Outer16;
        }
        else {
            connectedChannels = Inner16;
        }
        return;
    }

    //Create a 'time' vector
    QVector<double> t;
    t.resize(numSamples);
    for (int i = 0; i < numSamples; i++) {
        t[i] = i * (1 / state->sampleRate->getNumericValue());
    }

    //Estimate frequency of median waveforms
    double inner_f = estimateFrequency(inner_A, t, innerMedian);
    double outer_f = estimateFrequency(outer_A, t, outerMedian);

    //Detected frequency outside of good range, so let other 16 channels be loaded in
    if ((inner_f < 50) || (outer_f < 50) || (inner_f > 200) || (outer_f > 200)) {

        channels.resize(16);
        for (int channel = 0; channel < 16; channel++) {
            channels[channel].resize(numSamples);
        }

        for (int channel = 0; channel < 16; channel++) {
            for (int sample = 0; sample < numSamples; sample++) {
                //Inner channels have bad frequency
                if ((inner_f < 50) || (inner_f > 200)) {
                    channels[channel][sample] = outerChannels[channel][sample];
                }
                //Outer channels have bad frequency
                else {
                    channels[channel][sample] = innerChannels[channel][sample];
                }
            }
        }

        if ((inner_f < 50) || (inner_f > 200)) {
            connectedChannels = Outer16;
        }
        else {
            connectedChannels = Inner16;
        }
        return;
    }

    //Estimate phase of median waveforms
    double inner_phase = estimatePhase(inner_f, t, innerMedian);
    double outer_phase = estimatePhase(outer_f, t, outerMedian);

    //Compare best-fit lines to each group of 16 channels
    QVector<double> inner_channels_report;
    QVector<double> outer_channels_report;
    inner_channels_report.resize(16);
    outer_channels_report.resize(16);

    for (int channel = 0; channel < 16; channel++) {
        inner_channels_report[channel] = rmsError(t, innerChannels[channel], inner_f, inner_A, inner_phase);
        outer_channels_report[channel] = rmsError(t, outerChannels[channel], outer_f, outer_A, outer_phase);
    }

    //Take rms of vector of channels error to get a single value describing the error of inner or outer
    double inner_rms = rms(inner_channels_report);
    double outer_rms = rms(outer_channels_report);

    //Load the lower-error group into the channels vector
    channels.resize(16);
    for (int channel = 0; channel < 16; channel++) {
        channels[channel].resize(numSamples);
    }

    for (int channel = 0; channel < 16; channel++) {
        for (int sample = 0; sample < numSamples; sample++) {
            channels[channel][sample] = (inner_rms < outer_rms) ? (innerChannels[channel][sample]) : (outerChannels[channel][sample]);
        }
    }

    if (inner_rms < outer_rms) {
        connectedChannels = Inner16;
    }
    else {
        connectedChannels = Outer16;
    }
}

//Load data from a 64-channel chip into channels
// Run a test to determine if the inner 32 channels, outer 32 channels, or all 64 channels should be loaded
void TestControlPanel::load64(int numSamples, QVector<QVector<double> > &channels, QVector<QVector<QVector<double>>> &ampData) {

    // Load inner and outer 32 channels into innerChannels and outerChannels vectors
    QVector<QVector<double>> innerChannels;
    QVector<QVector<double>> outerChannels;
    innerChannels.resize(32);
    outerChannels.resize(32);
    for (int channel = 0; channel < 32; ++channel) {
        innerChannels[channel].resize(numSamples);
        outerChannels[channel].resize(numSamples);
    }
    for (int sample = 0; sample < numSamples; ++sample) {
        for (int channel = 0; channel < 32; ++channel) {
            innerChannels[channel][sample] = ampData[channel/16][channel + ((channel/16) == 0 ? 16 : -16)][sample];
            outerChannels[channel][sample] = ampData[channel/16][channel][sample];
        }
    }

    // Get inner and outer median waveforms
    QVector<double> innerMedian;
    QVector<double> outerMedian;
    innerMedian.resize(numSamples);
    outerMedian.resize(numSamples);
    medianWaveform(innerMedian, innerChannels);
    medianWaveform(outerMedian, outerChannels);

    // Subtract the two median waveforms to get the residual waveform
    QVector<double> residual;
    residual.resize(numSamples);
    subtractMatrix(innerMedian, outerMedian, residual);

    // Take absolute value of all samples in residual waveform
    for (int sample = 0; sample < numSamples; ++sample) {
        residual[sample] = abs(residual[sample]);
    }

    // Calculate the rms
    double rms_residual = rms(residual);

    // If rms_residual is below the threshold, resize channels to 64 and load data
    if (rms_residual < 10) {
        connectedChannels = All64;
        channels.resize(64);
        for (int channel = 0; channel < 64; ++channel) {
            channels[channel].resize(numSamples);
        }

        // Load data into channels vector
        for (int stream = 0; stream < 2; ++stream) {
            for (int channel = 0; channel < 32; ++channel) {
                for (int sample = 0; sample < numSamples; ++sample) {
                    channels[(stream * 32) + channel][sample] = ampData[stream][channel][sample];
                }
            }
        }
        return;
    }

    // If above, try to best-fit inner 32 and outer 32, load whichever is lowest
    // Estimate amplitude of median waveforms
    double inner_A = estimateAmplitude(innerMedian);
    double outer_A = estimateAmplitude(outerMedian);

    // Detected amplitude outside of good range, so let other 32 channels be loaded in
    if ((inner_A < 200) || (outer_A < 200) || (inner_A > 600) || (outer_A > 600)) {

        channels.resize(32);
        for (int channel = 0; channel < 32; ++channel) {
            channels[channel].resize(numSamples);
        }

        for (int channel = 0; channel < 32; ++channel) {
            for (int sample = 0; sample < numSamples; ++sample) {
                // Inner channels have bad amplitude
                if ((inner_A < 200) || (inner_A > 600)) {
                    channels[channel][sample] = outerChannels[channel][sample];
                }
                // Outer channels have bad amplitude
                else {
                    channels[channel][sample] = innerChannels[channel][sample];
                }
            }
        }

        if ((inner_A < 200) || (inner_A > 600)) {
            connectedChannels = Outer32;
        } else {
            connectedChannels = Inner32;
        }
        return;
    }

    // Create a 'time' vector
    QVector<double> t;
    t.resize(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        t[i] = i * (1 / state->sampleRate->getNumericValue());
    }

    // Estimate frequency of median waveforms
    double inner_f = estimateFrequency(inner_A, t, innerMedian);
    double outer_f = estimateFrequency(outer_A, t, outerMedian);

    // Detected frequency outside of good range, so let other 32 channels be loaded in
    if ((inner_f < 50) || (outer_f < 50) || (inner_f > 200) || (outer_f > 200)) {

        channels.resize(32);
        for (int channel = 0; channel < 32; ++channel) {
            channels[channel].resize(numSamples);
        }

        for (int channel = 0; channel < 32; ++channel) {
            for (int sample = 0; sample < numSamples; ++sample) {
                // Inner channels have bad frequency
                if ((inner_f < 50) || (inner_f > 200)) {
                    channels[channel][sample] = outerChannels[channel][sample];
                }
                // Outer channels have bad frequency
                else {
                    channels[channel][sample] = innerChannels[channel][sample];
                }
            }
        }

        if ((inner_f < 50) || (inner_f > 200)) {
            connectedChannels = Outer32;
        } else {
            connectedChannels = Inner32;
        }

        return;
    }

    // Estimate phase of median waveforms
    double inner_phase = estimatePhase(inner_f, t, innerMedian);
    double outer_phase = estimatePhase(outer_f, t, outerMedian);

    // Compare best-fit lines to each group of 32 channels
    QVector<double> inner_channels_report;
    QVector<double> outer_channels_report;
    inner_channels_report.resize(32);
    outer_channels_report.resize(32);

    for (int channel = 0; channel < 32; ++channel) {
        inner_channels_report[channel] = rmsError(t, innerChannels[channel], inner_f, inner_A, inner_phase);
        outer_channels_report[channel] = rmsError(t, outerChannels[channel], outer_f, outer_A, outer_phase);
    }

    outer_channels_report[5] = rmsError(t, outerChannels[5], outer_f, outer_A, outer_phase);

    // Take rms of vector of channels error to get a single value describing the error of inner or outer
    double inner_rms = rms(inner_channels_report);
    double outer_rms = rms(outer_channels_report);

    // Load the lower-error group into the channels vector
    channels.resize(32);
    for (int channel = 0; channel < 32; ++channel) {
        channels[channel].resize(numSamples);
    }

    for (int channel = 0; channel < 32; ++channel) {
        for (int sample = 0; sample < numSamples; ++sample) {
            channels[channel][sample] = (inner_rms < outer_rms) ? (innerChannels[channel][sample]) : (outerChannels[channel][sample]);
        }
    }

    if (inner_rms < outer_rms) {
        connectedChannels = Inner32;
    }
    else {
        connectedChannels = Outer32;
    }
}

//Sample-by-sample, get the median of all channels, and store the median in 'medianchannel'
void TestControlPanel::medianWaveform(QVector<double> &medianChannel, const QVector<QVector<double>> &channels)
{
    QVector<double> oneSample;
    oneSample.resize(channels.size());

    for (int sample = 0; sample < medianChannel.size(); sample++) {
        //For each individual sample, get the median of all channels
        for (int channel = 0; channel < channels.size(); channel++) {
            oneSample[channel] = channels[channel][sample];
        }
        medianChannel[sample] = median(oneSample);
    }
}

//Subtract 'subtrahend' matrix from 'minuend' matrix, placing result in 'difference' matrix
void TestControlPanel::subtractMatrix(const QVector<double> &minuend, const QVector<double> &subtrahend, QVector<double> &difference)
{
    QVector<double> neg;
    neg.resize(subtrahend.size());

    for (int index = 0; index < subtrahend.size(); index++) {
        neg[index] = -1 * subtrahend[index];
    }

    addMatrix(minuend, neg, difference);
}

//Calculate the root-mean-square of the numbers stored in 'waveform'
double TestControlPanel::rms(const QVector<double> &waveform)
{
    //Calculate sum of squares
    double square_sum = 0;
    double L = (double) waveform.size();
    for (int sample = 0; sample < L; sample++) {
        square_sum = square_sum + pow(waveform[sample],2);
    }

    //Calculate mean of sum of squares
    double mean = square_sum/L;

    //Return rms
    return sqrt(mean);
}

//Estimate the amplitude of 'waveform' (assumes that waveform is an ideal triangle wave)
double TestControlPanel::estimateAmplitude(const QVector<double> &waveform)
{
    double rms_waveform = rms(waveform);

    //Calculate A from rms for triangle waves
    return sqrt(3) * rms_waveform;
}

//Estimate the frequency of 'waveform' by counting the number of crossings across the t-axis
double TestControlPanel::estimateFrequency(double A, const QVector<double> &t, const QVector<double> &waveform)
{
    bool hysteresis_activated = false;
    int L = t.size() - 1;
    QVector<double> crossings;

    for (int sample = 0; sample < L; sample++) {
        //Count this sample if a crossing occurs before the hysteresis activates
        if (!hysteresis_activated) {
            //Crossing positive to negative
            if (waveform[sample] >= 0 && waveform[sample + 1] < 0) {
                crossings.append(t[sample]);
                hysteresis_activated = true;
            }
            //Crossing negative to positive
            else if (waveform[sample] < 0 && waveform[sample + 1] >= 0) {
                crossings.append(t[sample]);
                hysteresis_activated = true;
            }
        }

        //If the absolute value of this sample reaches 10% of the amplitude, deactivate the hysteresis
        if (abs(waveform[sample]) >= 0.1 * A)
            hysteresis_activated = false;
    }

    int D = crossings.size() - 1;

    //Determine the average duration between crossings
    QVector<double> differences;
    differences.resize(D);
    for (int crossing = 0; crossing < D; crossing++) {
        differences[crossing] = crossings[crossing + 1] - crossings[crossing];
    }

    double mean_duration = mean(differences);

    //Determine the frequency
    return (1 / (2*mean_duration));
}

//Estimate the phase by finding the first time the waveform crosses the t-axis positive to negative
double TestControlPanel::estimatePhase(double f, const QVector<double> &t, const QVector<double> &waveform)
{
    double T = 1/f;
    int L = t.size();
    double first_descent = 0.1;

    for (int sample = 0; sample < L; sample++) {
        //Find the time of  the first descent (crossing the x-axis positive to negative)
        if ((waveform[sample] >= 0) && (waveform[sample + 1] < 0)) {
            first_descent = t[sample];
            break;
        }
    }

    //Percentage of first_descent compared to T
    double percentage = first_descent/T - 0.25;
    percentage = fmod(percentage, 1);
    double phase = percentage * (2 * PI);
    if (phase < 0)
        phase += 2 * PI;
    return phase;
}

//Calculate the rms error that expresses the difference between the 'ytarget' waveform and the ideal triangle waveform created by parameters 'f', 'A', and 'phase'
double TestControlPanel::rmsError(const QVector<double> &t, const QVector<double> &ytarget, double f, double A, double phase)
{
    int numSamples = t.size();

    //Copy t vector
    QVector<double> tCopy;
    tCopy.resize(numSamples);
    for (int i = 0; i < numSamples; i++) {
        tCopy[i] = t[i];
    }

    //Create 'ytest' triangle waveform for each trial, based on the three variables
    QVector<double> ytest;
    ytest.resize(numSamples);
    triangle(ytest, tCopy, f, A, phase);

    //Eliminate offset from triangle waveform
    eliminateAverageOffset(ytest);

    //Accumulate error across all samples
    double error_iterator = 0;
    for (int sample = 0; sample < numSamples; sample++) {
        //For each sample, square the difference between target and test
        double single_sample_error = pow(ytarget[sample] - ytest[sample], 2);

        //Accumulate each sample's error into error_iterator
        error_iterator += single_sample_error;
    }

    //divide by # samples to get mean, then take square root to get rms
    return sqrt(error_iterator/numSamples);
}

//Calculate the median of the numbers stored in 'arr'
double TestControlPanel::median(const QVector<double> &arr)
{
    QVector<double> arrCopy;
    arrCopy.resize(arr.size());
    for (int i = 0; i < arr.size(); i++) {
        arrCopy[i] = arr[i];
    }

    //Arrange 'arrCopy' lowest to highest
    std::sort(arrCopy.begin(), arrCopy.end());

    //if odd # of elements...
    if ((arrCopy.size() % 2) == 1)
        return arrCopy[arrCopy.size()/2];

    //if even # of elements...
    else
        return (arrCopy[arrCopy.size()/2] + arrCopy[arrCopy.size()/2 - 1])/2;
}

//Add 'addend' matrix to 'augend' matrix, placing result in 'sum' matrix
void TestControlPanel::addMatrix(const QVector<double> &augend, const QVector<double> &addend, QVector<double> &sum)
{
    for (int index = 0; index < augend.size(); index++) {
        sum[index] = augend[index] + addend[index];
    }
}

//Calculate the mean of the numbers stored in 'arr'
double TestControlPanel::mean(const QVector<double> &arr)
{
    double sum = 0;
    for (int i = 0; i < arr.size(); i++) {
        sum += arr[i];
    }
    return (sum/arr.size());
}

//Load the ideal triangle waveform with parameters 'f', 'A', and 'phase' into 'y' vector
void TestControlPanel::triangle(QVector<double> &y, QVector<double> &t, double f, double A, double phase)
{
    double T = 1/f;

    QVector<double> base_t;
    base_t.resize(t.size());
    for (int i = 0; i < base_t.size(); i++) {
        base_t[i] = ((i * 1/state->sampleRate->getNumericValue()) / T) - (phase / (2*PI));
    }

    QVector<double> basetriangle_result;
    basetriangle_result.resize(t.size());
    baseTriangle(base_t, basetriangle_result);
    for (int i = 0; i < y.size(); i++) {
        y[i] = A * basetriangle_result[i];
    }
}


//Create the 'base' triangle that is then changed in the 'triangle' function to a given frequency, amplitude, and phase
void TestControlPanel::baseTriangle(QVector<double> &t, QVector<double> &y)
{
    int L = t.size();

    for (int i = 0; i < L; i++) {
        //if t[i] is outside the range 0 <= x < 1, shift it until it is
        while (t[i] >= 1) {
            t[i] = t[i] - 1;
        }
        while (t[i] < 0) {
            t[i] = t[i] + 1;
        }

        //if t[i] is in the range 0 <= t < 0.5
        if (t[i] < 0.5)
            y[i] = -4*t[i] + 1;
        else
            y[i] = 4*t[i] - 3;
    }
}

//Eliminate average offset from all channels within a vector of channels
void TestControlPanel::eliminateAverageOffset(QVector<QVector<double> > &channels)
{
    for (int channel = 0; channel < channels.size(); channel++) {
        eliminateAverageOffset(channels[channel]);
    }
}


//Eliminate average offset from a channel by subtracting the mean sample value from all samples
void TestControlPanel::eliminateAverageOffset(QVector<double> &channel)
{
    double average = mean(channel);
    for (int sample = 0; sample < channel.size(); sample++) {
        channel[sample] = channel[sample] - average;
    }
}

void TestControlPanel::testChip()
{
    configureTab->rescanPorts(true, portComboBox->currentIndex());
    state->clipWaveforms->setValue(false);

    int numPorts = 0;
    int portIndex = 0;
    for (int i = 0; i < state->signalSources->numPortGroups(); ++i) {
        if (state->signalSources->portGroupByIndex(i)->numAmpChannels->getValue() > 0) {
            numPorts++;
            portIndex = i;
        }
    }

    if (numPorts != 1) {
        QMessageBox::critical(this, "Incorrect Number of Connected Ports",
                              "Cannot check parameters of input triangle wave with more or less than 1 port at a time.");
        return;
    }

    // Default settings for display scale combo boxes.
    timeScaleComboBox->setCurrentIndex(5);
    state->yScaleWide->setValue("500");

    QVector<QVector<QVector<double>>> ampData;
    QVector<QVector<QString>> ampChannelNames;
    QVector<QVector<double>> fastSettleChannels;

    QVector<QVector<double>> auxInData;

    QProgressDialog progress(QObject::tr("Testing Chip"), QString(), 0, 12);
    progress.setWindowTitle(tr("Progress"));
    progress.setMinimumDuration(0);
    progress.setModal(true);

    // For non-Stim, run fast settle test
    if (state->getControllerTypeEnum() != ControllerStimRecord) {

        // Set fast settle true
        configureTab->fastSettleCheckBox->setChecked(true);
        configureTab->enableFastSettle(true);

        // Record fast settle data
        //recordDummySegment(2.0, portIndex);
        for (int i = 0; i < 10; i++) {
            recordDummySegment(0.1, portIndex);
        }
        progress.setValue(1);
        //recordFSSegment(fastSettleChannels, 1.0, portIndex, ampData, ampChannelNames);
        recordFSSegment(fastSettleChannels, 0.4, portIndex, ampData, ampChannelNames);
        eliminateAverageOffset(fastSettleChannels);

        // Set fast settle false
        configureTab->fastSettleCheckBox->setChecked(false);
        configureTab->enableFastSettle(false);
    }
    progress.setValue(2);

    //Run for 3 seconds for dummy data
    for (int i = 0; i < 30; i++) {
    //for (int i = 0; i < 5; i++) {
    //for (int i = 0; i < 20; i++) {
        recordDummySegment(0.1, portIndex);
    }
    progress.setValue(3);

    ampData.clear();
    auxInData.clear();
    ampChannelNames.clear();

    //Load short segment of data into 'channels' vector
    int numSamples = recordShortSegment(channels, 0.4, portIndex, ampData, ampChannelNames, auxInData);

    progress.setValue(4);

    //Subtract the average value from each of the waveforms (to eliminate transient offset)
    eliminateAverageOffset(channels);

    // Get largest max - min difference across data. If less than 40 uV, chip is likely dead.
    double maxSample = 0, minSample = 0;
    for (int stream = 0; stream < ampData.size(); stream++) {
        for (int channel = 0; channel < ampData[stream].size(); channel++) {
            for (int index = 0; index < ampData[stream][channel].size(); index++) {
                double thisSample = ampData[stream][channel][index];
                if (thisSample > maxSample) maxSample = thisSample;
                if (thisSample < minSample) minSample = thisSample;
            }
        }
    }
    bool allChannelsDead = false;
    if (maxSample - minSample < 40.0) {
        allChannelsDead = true;
    }

    auxIn1Min = auxIn2Min = auxIn3Min = 3.3;
    auxIn1Max = auxIn2Max = auxIn3Max = 0;
    // If testing aux ins is suitable, get median of each channel, and max-min of each channel
    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        for (int index = 0; index < auxInData[0].size(); index++) {
            double thisSample1 = auxInData[0][index];
            double thisSample2 = auxInData[1][index];
            double thisSample3 = auxInData[2][index];

            if (thisSample1 > auxIn1Max) auxIn1Max = thisSample1;
            if (thisSample1 < auxIn1Min) auxIn1Min = thisSample1;

            if (thisSample2 > auxIn2Max) auxIn2Max = thisSample2;
            if (thisSample2 < auxIn2Min) auxIn2Min = thisSample2;

            if (thisSample3 > auxIn3Max) auxIn3Max = thisSample3;
            if (thisSample3 < auxIn3Min) auxIn3Min = thisSample3;
        }
        auxIn1Median = median(auxInData[0]);
        auxIn2Median = median(auxInData[1]);
        auxIn3Median = median(auxInData[2]);
    }

    //Determine median waveform of the channels
    QVector<double> median;
    median.resize(numSamples);
    medianWaveform(median, channels);

    //Estimate the amplitude of the median waveform
    double A = estimateAmplitude(median);

    //Create a 'time' vector
    QVector<double> t;
    t.resize(numSamples);
    for (int i = 0; i < numSamples; i++) {
        t[i] = i * (1 / state->sampleRate->getNumericValue());
    }

    //Estimate the frequency of the median waveform
    double f = estimateFrequency(A, t, median);

    //Estimate the phase of the median waveform
    double phase = estimatePhase(f, t, median);

    /* Amoeba method */

    QVector<double> p_initial;
    p_initial.resize(3);
    p_initial[0] = f;
    p_initial[1] = A;
    p_initial[2] = phase;

    int restarts = 0;
    progress.setValue(5);

    int finalLoops = 0;

    while (restarts < 50) {

        // Don't bother iterating over loop if it's clear that channels are dead.
        if (allChannelsDead) break;

        QVector<double> offset_f;
        offset_f.reserve(3);
        offset_f[0] = 0.1 * p_initial[0];
        offset_f[1] = 0;
        offset_f[2] = 0;

        QVector<double> offset_A;
        offset_A.reserve(3);
        offset_A[0] = 0;
        offset_A[1] = 0.1 * p_initial[1];
        offset_A[2] = 0;

        QVector<double> offset_phase;
        offset_phase.reserve(3);
        offset_phase[0] = 0;
        offset_phase[1] = 0;
        offset_phase[2] = restarts * PI/10;

        QVector<double> p1;
        p1.resize(3);
        QVector<double> p2;
        p2.resize(3);
        QVector<double> p3;
        p3.resize(3);

        addMatrix(p_initial, offset_f, p1);
        addMatrix(p_initial, offset_A, p2);
        addMatrix(p_initial, offset_phase, p3);

        QVector<QVector<double>> p;
        p.resize(4);
        p[0] = p_initial;
        p[1] = p1;
        p[2] = p2;
        p[3] = p3;

        QVector<double> y;
        y.resize(4);

        for (int index = 0; index < 4; index++) {
            y[index] = rmsError(t, median, p[index][0], p[index][1], p[index][2]);
        }

        //int nfunk = amoeba(t, median, p, y, 3, 0.01);
        //qDebug() << "nfunk: " << nfunk;
        amoeba(t, median, p, y, 3, 0.01);

        if (y[0] < y[1] && y[0] < y[2] && y[0] < y[3])
            p_initial = p[0];
        else if (y[1] < y[0] && y[1] < y[2] && y[1] < y[3])
            p_initial = p[1];
        else if (y[2] < y[0] && y[2] < y[1] && y[2] < y[3])
            p_initial = p[2];
        else
            p_initial = p[3];

        if (restarts % 10 == 0) {
            progress.setValue(progress.value() + 1);
        }

        if (finalLoops >= 2) {
            break;
        }

        // Escape early if threshold is met.
        // Check if all channels satisfy channels_report requirement - if so, can escape early.
        double maxError = 0;
        double threshold = triangleErrorThresholdLineEdit->text().toDouble();
        for (int channel = 0; channel < channels.size(); channel++) {
            double thisError = rmsError(t, channels[channel], p_initial[0], p_initial[1], p_initial[2]);
            maxError = (std::max)(maxError, thisError);
        }
        if (maxError < threshold) {
            finalLoops++;
        }

        restarts++;
    }

    progress.setValue(11);
    p_initial[2] = fmod(p_initial[2], 2*PI);

    QVector<double> p_final;
    p_final.resize(3);

    p_final = p_initial;

    //Make sure low-error frequency isn't outside the range 90 - 110 Hz
    if (p_final[0] < 90 || p_final[0] > 110)
        p_final[0] = 100;

    //Make sure low-error amplitude isn't outside the range 300 to 600 uV
    if (p_final[1] < 300 || p_final[1] > 600)
        p_final[1] = 400;

    //double y_final = y_initial;

    //Compare the best-fit line to each channel
    channels_report.resize(channels.size());
    for (int channel = 0; channel < channels.size(); channel++) {
        channels_report[channel] = rmsError(t, channels[channel], p_final[0], p_final[1], p_final[2]);
    }

    //Convert fastSettleChannels to validFastSettleChannels (to account for possibility that 64-channel headstage is plugged in, with inner or outer 32 channels valid
    if (state->getControllerTypeEnum() != ControllerStimRecord) {
        QVector<QVector<double>> validFastSettleChannels;
        validateFastSettleChannels(fastSettleChannels, validFastSettleChannels);
        channels_report_settle.resize(validFastSettleChannels.size());
        for (int channel = 0; channel < validFastSettleChannels.size(); channel++) {
            double sum = 0;
            for (int sample = 0; sample < validFastSettleChannels[channel].size(); sample++) {
                sum += pow(validFastSettleChannels[channel][sample], 2);
            }
            channels_report_settle[channel] = pow(sum/validFastSettleChannels[channel].size(), 0.5);
        }
        multiColumnDisplay->loadWaveformDataDirectAmp(ampData, ampChannelNames, auxInData);
    }

    else {
        // 0) Set plot to display Wideband on Col. 1 and DC on Col. 2
        state->arrangeBy->setIndex(1);
        state->filterDisplay2->setIndex(5);
        QString displaySettingsString;
        displaySettingsString = "ShowColumn:Port A:1456,723,22,1,0,0,0,0,0:ShowPinned:;ShowColumn:Port A:1456,723,22,1,733,0,0,0,0:ShowPinned:";
        state->displaySettings->setValue(displaySettingsString);
        multiColumnDisplay->restoreFromDisplaySettingsString(displaySettingsString);
        filterDisplaySelector->updateFromState();

        // 1) Set up stimulation parameters
        // Get the command stream(s) of any connected chips
        QVector<int> connectedStreams = getConnectedCommandStreams();
        // Upload stim parameters to command stream(s)
        for (int streamIndex = 0; streamIndex < connectedStreams.size(); ++streamIndex) {
            controllerInterface->uploadAutoStimParameters(connectedStreams[streamIndex]);
        }

        // 2) Acquire and stimulate for 400 ms, plot DC
        QVector<QVector<QString>> dcChannelNames;
        recordDCSegment(0.4, portIndex, dcChannelNames);

        // 3) Load this segment into plotter
        multiColumnDisplay->loadWaveformDataDirectAmpDC(ampData, ampChannelNames, dcData, dcChannelNames);

        // 4) Populate report with whether DC waveforms are acceptable
        checkDCWaveforms();
    }

    //Allow user to access report
    reportPresent = true;
    viewReportButton->setEnabled(true);
    saveReportButton->setEnabled(true);

    generateReport(channels);

    // For Stim, clear stim parameters before ending
    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        QVector<int> connectedStreams = getConnectedCommandStreams();
        for (auto it = connectedStreams.begin(); it != connectedStreams.end(); ++it) {
            controllerInterface->clearStimParameters(*it);
        }
    }

    // For non-Stim, run for 0.1 to get amp settle switched immediately
    else {
        configureTab->fastSettleCheckBox->setChecked(false);
        configureTab->enableFastSettle(false);
        for (int i = 0; i < 1; i++) {
            recordDummySegment(0.1, portIndex);
        }
    }
    progress.setValue(12);
}

void TestControlPanel::generateReport(QVector<QVector<double> > channels)
{
    // Clear 'report' QVector
    if (report.size() != 0) {
        for (int channel = 0; channel < report.size(); channel++) {
            delete report.at(channel);
        }
    }

    // Populate 'report' QVector with measurements
    report.resize(channels.size());
    for (int channel = 0; channel < report.size(); channel++) {
        report.replace(channel, new ChannelInfo);
        report[channel]->actualChannelNumber = channel;
        report[channel]->channelVectorIndex = channel;
        report[channel]->triangleError = channels_report[channel];
        report[channel]->variableError = state->getControllerTypeEnum() == ControllerStimRecord ? channels_report_stim[channel] : channels_report_settle[channel];
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            report[channel]->posAvg = channels_report_pos[channel];
            report[channel]->negAvg = channels_report_neg[channel];
        }

        if (connectedChannels == Inner32)
            report[channel]->actualChannelNumber = channel + 16;
        else if (connectedChannels == Outer32 && channel > 15)
            report[channel]->actualChannelNumber = channel + 32;
        else if (connectedChannels == Inner16)
            report[channel]->actualChannelNumber = channel + 8;
        else if (connectedChannels == Outer16 && channel > 7)
            report[channel]->actualChannelNumber = channel + 16;
    }

    bool high_error = false;
    int threshold = triangleErrorThresholdLineEdit->text().toDouble();

    for (int channel = 0; channel < channels.size(); channel++) {
        if (channels_report[channel] > threshold || std::isnan(channels_report[channel])) {
            high_error = true;
        }
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            if (channels_report_stim[channel] != 0) {
                high_error = true;
            }
        } else {
            if (channels_report_settle[channel] > settleErrorThresholdLineEdit->text().toDouble()) {
                high_error = true;
            }
        }
    }

    if (state->getControllerTypeEnum() != ControllerStimRecord && state->testAuxIns->getValue()) {
        if (isOutsideExpectedValue(auxIn1Min, auxInExpectedMin)) high_error = true;
        if (isOutsideExpectedValue(auxIn1Max, auxInExpectedMax)) high_error = true;
        if (isOutsideExpectedValue(auxIn1Median, auxInExpectedMedian)) high_error = true;

        if (isOutsideExpectedValue(auxIn2Min, auxInExpectedMin)) high_error = true;
        if (isOutsideExpectedValue(auxIn2Max, auxInExpectedMax)) high_error = true;
        if (isOutsideExpectedValue(auxIn2Median, auxInExpectedMedian)) high_error = true;

        if (isOutsideExpectedValue(auxIn3Min, auxInExpectedMin)) high_error = true;
        if (isOutsideExpectedValue(auxIn3Max, auxInExpectedMax)) high_error = true;
        if (isOutsideExpectedValue(auxIn3Median, auxInExpectedMedian)) high_error = true;
    }

    viewReportButton->setStyleSheet(high_error ? "QPushButton {background-color: red; }" : "QPushButton {background-color: green; }");
    reportLabel->setText(high_error ? "Bad" : "Good");
    reportLabel->setStyleSheet(high_error ? "QLabel { color: red; }" : "QLabel { color : green; }");
}

bool TestControlPanel::isOutsideExpectedValue(double actualValue, double expectedValue)
{
    return (actualValue > (1.0 + acceptableDifference) * expectedValue || actualValue < (1.0 - acceptableDifference) * expectedValue);
}

void TestControlPanel::checkDCWaveforms()
{
    channels_report_stim.resize(dcData.size() * 16);
    channels_report_pos.resize(dcData.size() * 16);
    channels_report_neg.resize(dcData.size() * 16);
    QVector<double> avgVoltage;
    avgVoltage.resize(dcData.size() * 16);
    for (int i = 0; i < avgVoltage.size(); ++i) {
        avgVoltage[i] = 0.0;
    }
    for (int stream = 0; stream < dcData.size(); ++stream) {
        for (int channel = 0; channel < dcData[stream].size(); ++channel) {
            QVector<double> posVoltages, negVoltages;
            channels_report_stim[stream*16 + channel] = calculateVoltages(dcData[stream][channel], posVoltages, negVoltages);
            channels_report_pos[stream*16 + channel] = vectorAvg(posVoltages);
            channels_report_neg[stream*16 + channel] = vectorAvg(negVoltages);
            if (channels_report_stim[stream*16 + channel] == 0) {
                channels_report_stim[stream*16 + channel] = !(posAvgInBounds(vectorAvg(posVoltages)) && negAvgInBounds(vectorAvg(negVoltages)));
            }
        }
    }
}

bool TestControlPanel::posAvgInBounds(double avgVoltage)
{
    double expectedVoltage = stimExpectedVoltageLineEdit->text().toDouble();
    double zeroUpperBound = expectedVoltage * (stimErrorThresholdSpinBox->value() / 100.0);
    double zeroLowerBound = -1 * zeroUpperBound;
    double positiveLowerBound = expectedVoltage + zeroLowerBound;
    double positiveUpperBound = expectedVoltage + zeroUpperBound;

    return (avgVoltage >= positiveLowerBound && avgVoltage <= positiveUpperBound);
}

bool TestControlPanel::negAvgInBounds(double avgVoltage)
{
    double expectedVoltage = stimExpectedVoltageLineEdit->text().toDouble();
    double zeroUpperBound = expectedVoltage * (stimErrorThresholdSpinBox->value() / 100.0);
    double zeroLowerBound = -1 * zeroUpperBound;
    double negativeLowerBound = -1 * expectedVoltage + zeroLowerBound;
    double negativeUpperBound = -1 * expectedVoltage + zeroUpperBound;

    return (avgVoltage >= negativeLowerBound && avgVoltage <= negativeUpperBound);
}

// Return 0 for no error, 1 for error. Populate posVoltages and negVoltages with all values of stim voltages.
int TestControlPanel::calculateVoltages(QVector<double> dcData, QVector<double> &posVoltages, QVector<double> &negVoltages)
{
    double expectedVoltage = stimExpectedVoltageLineEdit->text().toDouble();
    double zeroUpperBound = expectedVoltage * (stimErrorThresholdSpinBox->value() / 100.0);
    double zeroLowerBound = -1 * zeroUpperBound;
    double positiveLowerBound = expectedVoltage + zeroLowerBound;
//    double positiveUpperBound = expectedVoltage + zeroUpperBound;
//    double negativeLowerBound = -1 * expectedVoltage + zeroLowerBound;
//    double negativeUpperBound = -1 * expectedVoltage + zeroUpperBound;

    int firstRisingEdge = -1;
    int samplesPerCycle = 1600;
    int cycles = 6;
    int totalSamples = dcData.size();
    int lastPossibleRisingEdge = totalSamples - (samplesPerCycle * cycles);
    int transientSamplesToIgnore = 10;

    // 1) Find first rising edge to positiveThreshold.
    // If this doesn't happen before lastPossibleRisingEdge, return False
    for (int sample = transientSamplesToIgnore; sample < lastPossibleRisingEdge; ++sample) {
        if (dcData[sample] < positiveLowerBound && dcData[sample + 1] >= positiveLowerBound) {
            firstRisingEdge = sample + 1;
            break;
        }
    }
    if (firstRisingEdge == -1) {
        // positiveThreshold never gets reached.
        // So, populate posVoltages with a single value - the highest sample value reached in what should be the positive region,
        // and populate negVoltages with a single value - the lowest sample value reached in what should be the negative region,
        // and return 1 (error).
        posVoltages.append(*std::max_element(dcData.begin() + transientSamplesToIgnore, dcData.begin() + 400));
        negVoltages.append(*std::min_element(dcData.begin() + 400 + transientSamplesToIgnore, dcData.begin() + 800));
        qDebug() << "violates expected A";
        return 1;
    }

    // 2) Check for 6 repeats of: 400 samples at expectedVoltage ... 400 samples at -expectedVoltage ... 800 samples at 0 V
    int errorFlag = 0;
    int currentIndex = firstRisingEdge + transientSamplesToIgnore;
    for (int cycle = 0; cycle < cycles; cycle++) {
        // All of the next 390 samples should be between positiveLowerBound and positiveUpperBound
        //for (int sample = currentIndex; sample < 400 - transientSamplesToIgnore; ++sample) {
        for (int sample = currentIndex; sample < currentIndex + 400 - transientSamplesToIgnore; ++sample) {
            posVoltages.append(dcData[sample]);
        }
        currentIndex += 400;
        // All of the next 390 samples should be between negativeLowerBound and negativeUpperBound
        //for (int sample = currentIndex; sample < 400 - transientSamplesToIgnore; ++sample) {
        for (int sample = currentIndex; sample < currentIndex + 400 - transientSamplesToIgnore; ++sample) {
            negVoltages.append(dcData[sample]);
        }
        currentIndex += 400; // Look 10 samples beyond the transient
        // All of the next 790 samples should between zeroLowerBound and zeroUpperBound
        //for (int sample = currentIndex; sample < 800 - transientSamplesToIgnore; ++sample) {
        for (int sample = currentIndex; sample < currentIndex + 800 - transientSamplesToIgnore; ++sample) {
            if (dcData[sample] < zeroLowerBound || dcData[sample] > zeroUpperBound) {
                qDebug() << "violates expected D... cycle: " << cycle << " sample index: " << sample << " voltage: " << dcData[sample];
                errorFlag = 1;
            }
        }
        currentIndex += 800; // Look 10 samples beyond the transient
    }

    return errorFlag;
}

//// Return 0 for no error, 1 for error. Populate posVoltages and negVoltages with all values of stim voltages.
//int TestControlPanel::checkDCWaveform(QVector<double> dcData, QVector<double> &posVoltages, QVector<double> &negVoltages)
//{
//    double expectedVoltage = stimExpectedVoltageLineEdit->text().toDouble();
//    double zeroUpperBound = expectedVoltage * (stimErrorThresholdSpinBox->value() / 100.0);
//    double zeroLowerBound = -1 * zeroUpperBound;
//    double positiveLowerBound = expectedVoltage + zeroLowerBound;
//    double positiveUpperBound = expectedVoltage + zeroUpperBound;
//    double negativeLowerBound = -1 * expectedVoltage + zeroLowerBound;
//    double negativeUpperBound = -1 * expectedVoltage + zeroUpperBound;

//    int firstRisingEdge = -1;
//    int samplesPerCycle = 1600;
//    int cycles = 6;
//    int totalSamples = dcData.size();
//    int lastPossibleRisingEdge = totalSamples - (samplesPerCycle * cycles);
//    int transientSamplesToIgnore = 10;

//    // 1) Find first rising edge to positiveThreshold.
//    // If this doesn't happen before lastPossibleRisingEdge, return False
//    for (int sample = transientSamplesToIgnore; sample < lastPossibleRisingEdge; ++sample) {
//        if (dcData[sample] < positiveLowerBound && dcData[sample + 1] >= positiveLowerBound) {
//            firstRisingEdge = sample + 1;
//            break;
//        }
//    }
//    if (firstRisingEdge == -1) {
//        qDebug() << "violates expected A";
//        return 1;
//    }

//    // 2) Check for 6 repeats of: 400 samples at expectedVoltage ... 400 samples at -expectedVoltage ... 800 samples at 0 V
//    int errorFlag = 0;
//    int currentIndex = firstRisingEdge + transientSamplesToIgnore;
//    //qDebug() << "Starting first cycle at index: " << currentIndex;
//    for (int cycle = 0; cycle < cycles; cycle++) {
//        // All of the next 390 samples should be between positiveLowerBound and positiveUpperBound
//        //for (int sample = currentIndex; sample < 400 - transientSamplesToIgnore; ++sample) {
//        for (int sample = currentIndex; sample < currentIndex + 400 - transientSamplesToIgnore; ++sample) {
//            if (dcData[sample] < positiveLowerBound || dcData[sample] > positiveUpperBound) {
//                qDebug() << "violates expected B... cycle: " << cycle << " sample index: " << sample << " voltage: " << dcData[sample];
//                errorFlag = 1;
//            } else {
//                //qDebug() << "Appending to positive: " << dcData[sample];
//                posVoltages.append(dcData[sample]);
//            }
//        }
//        currentIndex += 400;
//        // All of the next 390 samples should be between negativeLowerBound and negativeUpperBound
//        //for (int sample = currentIndex; sample < 400 - transientSamplesToIgnore; ++sample) {
//        for (int sample = currentIndex; sample < currentIndex + 400 - transientSamplesToIgnore; ++sample) {
//            if (dcData[sample] < negativeLowerBound || dcData[sample] > negativeUpperBound) {
//                qDebug() << "violates expected C... cycle: " << cycle << " sample index: " << sample << " voltage: " << dcData[sample];
//                errorFlag = 1;
//            } else {
//                negVoltages.append(dcData[sample]);
//            }
//        }
//        currentIndex += 400; // Look 10 samples beyond the transient
//        // All of the next 790 samples should between zeroLowerBound and zeroUpperBound
//        //for (int sample = currentIndex; sample < 800 - transientSamplesToIgnore; ++sample) {
//        for (int sample = currentIndex; sample < currentIndex + 800 - transientSamplesToIgnore; ++sample) {
//            if (dcData[sample] < zeroLowerBound || dcData[sample] > zeroUpperBound) {
//                qDebug() << "violates expected D... cycle: " << cycle << " sample index: " << sample << " voltage: " << dcData[sample];
//                errorFlag = 1;
//            }
//        }
//        currentIndex += 800; // Look 10 samples beyond the transient
//    }

//    return errorFlag;
//}

// Load data in fastSettleChannels into validFastSettleChannels
void TestControlPanel::validateFastSettleChannels(QVector<QVector<double> > &fastSettleChannels, QVector<QVector<double> > &validFastSettleChannels)
{
    int numSamples = fastSettleChannels[0].size();
    int numChannels = 0;
    int innerOffset = 0;
    int outerOffset = 0;

    switch (connectedChannels) {
    case All16:
        numChannels = 16; break;
    case Inner16:
        numChannels = 16; innerOffset = 8; break;
    case Outer16:
        numChannels = 16; outerOffset = 16; break;
    case All32:
        numChannels = 32; break;
    case Inner32:
        numChannels = 32; innerOffset = 16; break;
    case Outer32:
        numChannels = 32; innerOffset = 32; break;
    case All64:
        numChannels = 64; break;
    case All128:
        numChannels = 128; break;
    default:
        qDebug() << "UNRECOGNIZED...";
    }

    validFastSettleChannels.resize(numChannels);
    for (int channel = 0; channel < numChannels; ++channel) {
        validFastSettleChannels[channel].resize(numSamples);
        for (int sample = 0; sample < numSamples; ++sample) {
            if (channel < numChannels/2)
                validFastSettleChannels[channel][sample] = fastSettleChannels[channel + innerOffset][sample];
            else
                validFastSettleChannels[channel][sample] = fastSettleChannels[channel + innerOffset + outerOffset][sample];
        }
    }
}

// Use the 'Downhill Simplex Method in Multidimensions' algorithm (aka amoeba) described on pg. 408 of Numerical Recipes in C
// This algorithm minimizes the rmsError() function, determining the frequency, amplitude, and phase that result in the smallest error between the target waveform and an ideal triangle wave
int TestControlPanel::amoeba(QVector<double> &t, QVector<double> &ytarget, QVector<QVector<double> > &p, QVector<double> &y, int ndim, double ftol)
{
    int i, ihi, ilo, inhi, j, mpts = ndim + 1;
    double rtol, sum;

    QVector<double> psum;
    psum.resize(ndim);
    for (int i = 0; i < ndim; i++) {
        psum[i] = 0;
    }
    int nfunk = 0;

    //GET_PSUM
    for (j = 0; j < ndim; j++) {
        double sum = 0;
        for (int i = 0; i < mpts; i++) {
            sum += p[i][j];
        }
        psum[j] = sum;
    }

    while (true) {
        ilo = 0;
        //First we must determine which point is the highest (worst), next-highest, and lowest (best), by looping over the points in the simplex.
        if (y[0] > y[1]) {
            inhi = 2;
            ihi = 1;
        }
        else {
            inhi = 1;
            ihi = 2;
        }

        for (i = 0; i < mpts; i++) {
            if (y[i] <= y[ilo]) {
                ilo = i;
            }

            if (y[i] > y[ihi]) {
                inhi = ihi;
                ihi = i;
            }
            else if (y[i] > y[inhi] && i != ihi) {
                inhi = i;
            }
        }

        rtol = 2*abs(y[ihi] - y[ilo])/(abs(y[ihi]) + abs(y[ilo]));
        //Compute the fractional range from highest to lowest and return if satisfactory
        if (rtol < ftol) {
            double temp = y[0];
            y[0] = y[ilo];
            y[ilo] = temp;
            for (i = 0; i < ndim; i++) {
                temp = p[0][i];
                p[0][i] = p[ilo][i];
                p[ilo][i] = temp;
            }
            break;
        }

        if (nfunk >= 5000) {
            qDebug() << "NMAX exceeded";
            break;
        }

        nfunk += 2;

        //Begin a new iteration. First extrapolate by a factor -1 through the face of the simplex across from the high point,
        //i.e., reflect the simplex from the high point
        double ytry = amotry(t, ytarget, p, y, psum, ndim, ihi, -1);

        if (ytry <= y[ilo]) {
            //Gives a result better than the best point, so try an additional extrapolation by a factor of 2
            ytry = amotry(t, ytarget, p, y, psum, ndim, ihi, 2);
        }

        if (ytry >= y[inhi]) {
            //The reflected point is worse than the second-highest, so look for an intermediate lower point,
            //i.e., do a one-dimensional contraction
            double ysave = y[ihi];
            ytry = amotry(t, ytarget, p, y, psum, ndim, ihi, 0.5);
            if (ytry >= ysave) {
                //Can't seem to get rid of that high point. Better contract around the lowest (best) point
                for (i = 0; i < mpts; i++) {
                    if (i != ilo) {
                        for (j = 0; j < ndim; j++) {
                            psum[j] = 0.5 * (p[i][j] + p[ilo][j]);
                            p[i][j] = psum[j];
                        }
                        y[i] = rmsError(t, ytarget, psum[0], psum[1], psum[2]);
                        //y[i] = funk(psum);
                    }
                }

                //Keep track of function evaluations
                nfunk += ndim;

                //Recompute psum
                for (j = 0; j < ndim; j++) {
                    sum = 0;
                    for (i = 0; i < mpts; i++) {
                        sum += p[i][j];
                    }
                    psum[j] = sum;
                }
            }
        }

        else {
            nfunk = nfunk - 1;
        }
    }
    return nfunk;
}

// Execute a single try (evaluation) that is repeated multiple times over the course of the amoeba() function
double TestControlPanel::amotry(QVector<double> &t, QVector<double> &ytarget, QVector<QVector<double> > &p, QVector<double> &y, QVector<double> &psum, int ndim, int ihi, double fac)
{
    //Extrapolates by a factor fac through the face of the simplex across from the high point, tries it, and replaces the high point if the new point is better
    QVector<double> ptry;
    ptry.resize(ndim);
    double fac1 = (1 - fac)/ndim;
    double fac2 = fac1 - fac;

    for (int j = 0; j < ndim; j++) {
        ptry[j] = fac1*psum[j] - fac2*p[ihi][j];
    }

    //Evaluate the function at the trial point. If it's better than the highest, then replace the highest
    //ytry = funk(ptry);
    double ytry = rmsError(t, ytarget, ptry[0], ptry[1], ptry[2]);
    if (ytry < y[ihi]) {
        y[ihi] = ytry;
        for (int j = 0; j < ndim; j++) {
            psum[j] = psum[j] + ptry[j] - p[ihi][j];
            p[ihi][j] = ptry[j];
        }
    }
    return ytry;
}

void TestControlPanel::viewReport()
{
    ReportDialog reportDialog(this);

    QString report_string = "";
    bool firstLine = true;

    QString redHtml = "<font color=\"Red\">";
    QString greenHtml = "<font color=\"Green\">";
    QString blackHtml = "<font color=\"Black\">";
    QString orangeHtml = "<font color=\"Orange\">";

    // Recalculate report (in case parameters were changed since test was run)
    checkDCWaveforms();
    generateReport(channels);

    for (int channel = 0; channel < channels_report.size(); channel++) {
        if (!firstLine) {
            report_string.append("<br>");
        } else {
            firstLine = false;
        }

        bool goodTriangle = report[channel]->triangleError < triangleErrorThresholdLineEdit->text().toFloat();
        bool goodVariable;
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            goodVariable = report[channel]->variableError == 0;
        } else {
            goodVariable = report[channel]->variableError < settleErrorThresholdLineEdit->text().toFloat();
        }

        report_string.append(blackHtml % "Channel ");
        report_string.append(QString::number(report[channel]->actualChannelNumber));
        report_string.append(". Triangle error: ");
        report_string.append(goodTriangle ? greenHtml : redHtml);
        report_string.append(QString::number(report[channel]->triangleError));
        report_string.append(blackHtml);

        report_string.append(state->getControllerTypeEnum() == ControllerStimRecord ? ". Stim error: " : ". Settle error: ");
        report_string.append(goodVariable ? greenHtml : redHtml);
        report_string.append(QString::number(report[channel]->variableError));
        report_string.append(blackHtml);
        report_string.append(".");

        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            QString posAvgHtml = posAvgInBounds(report[channel]->posAvg) ? orangeHtml : redHtml;
            QString negAvgHtml = negAvgInBounds(report[channel]->negAvg) ? orangeHtml : redHtml;
            report_string.append(" Pos Avg: ");
            report_string.append(posAvgHtml);
            report_string.append(QString::number(report[channel]->posAvg, 'f', 2));
            report_string.append(blackHtml % ". Neg Avg: ");
            report_string.append(negAvgHtml);
            report_string.append(QString::number(report[channel]->negAvg, 'f', 2));
            report_string.append(blackHtml % ".");
        }
    }

    if (state->getControllerTypeEnum() == ControllerStimRecord) {
        if (channels_report.size() == 16) {
            double posAvg = vectorAvg(channels_report_pos);
            double negAvg = vectorAvg(channels_report_neg);
            report_string.append("<br>");
            report_string.append("Chip 0 Positive Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(posAvg, 'f', 2));
            report_string.append(blackHtml % ". Negative Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(negAvg, 'f', 2));
            report_string.append(blackHtml % "<br>");
        } else {
            double posAvg0 = vectorAvg(channels_report_pos, 0, 16);
            double posAvg1 = vectorAvg(channels_report_pos, 16, 32);
            double negAvg0 = vectorAvg(channels_report_neg, 0, 16);
            double negAvg1 = vectorAvg(channels_report_neg, 16, 32);
            report_string.append("<br>");
            report_string.append("Chip 0 Positive Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(posAvg0, 'f', 2));
            report_string.append(blackHtml % ". Negative Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(negAvg0, 'f', 2));
            report_string.append(blackHtml % "<br>");
            report_string.append("Chip 1 Positive Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(posAvg1, 'f', 2));
            report_string.append(blackHtml % ". Negative Voltage Avg: ");
            report_string.append(orangeHtml % QString::number(negAvg1, 'f', 2));
            report_string.append(blackHtml % "<br>");
        }
    }

    else {
        if (state->testAuxIns->getValue()) {
            QString auxIn1MinHtml = isOutsideExpectedValue(auxIn1Min, auxInExpectedMin) ? redHtml : greenHtml;
            QString auxIn1MaxHtml = isOutsideExpectedValue(auxIn1Max, auxInExpectedMax) ? redHtml : greenHtml;
            QString auxIn1MedianHtml = isOutsideExpectedValue(auxIn1Median, auxInExpectedMedian) ? redHtml : greenHtml;

            QString auxIn2MinHtml = isOutsideExpectedValue(auxIn2Min, auxInExpectedMin) ? redHtml : greenHtml;
            QString auxIn2MaxHtml = isOutsideExpectedValue(auxIn2Max, auxInExpectedMax) ? redHtml : greenHtml;
            QString auxIn2MedianHtml = isOutsideExpectedValue(auxIn2Median, auxInExpectedMedian) ? redHtml : greenHtml;

            QString auxIn3MinHtml = isOutsideExpectedValue(auxIn3Min, auxInExpectedMin) ? redHtml : greenHtml;
            QString auxIn3MaxHtml = isOutsideExpectedValue(auxIn3Max, auxInExpectedMax) ? redHtml : greenHtml;
            QString auxIn3MedianHtml = isOutsideExpectedValue(auxIn3Median, auxInExpectedMedian) ? redHtml : greenHtml;

            report_string.append(blackHtml % "<br>AuxIn1: Min: ");
            report_string.append(auxIn1MinHtml % QString::number(auxIn1Min, 'f', 2));
            report_string.append(blackHtml % ". Max: ");
            report_string.append(auxIn1MaxHtml % QString::number(auxIn1Max, 'f', 2));
            report_string.append(blackHtml % ". Median: ");
            report_string.append(auxIn1MedianHtml % QString::number(auxIn1Median, 'f', 2) % ".");

            report_string.append(blackHtml % "<br>AuxIn2: Min: ");
            report_string.append(auxIn2MinHtml % QString::number(auxIn2Min, 'f', 2));
            report_string.append(blackHtml % ". Max: ");
            report_string.append(auxIn2MaxHtml % QString::number(auxIn2Max, 'f', 2));
            report_string.append(blackHtml % ". Median: ");
            report_string.append(auxIn2MedianHtml % QString::number(auxIn2Median, 'f', 2) % ".");

            report_string.append(blackHtml % "<br>AuxIn3: Min: ");
            report_string.append(auxIn3MinHtml % QString::number(auxIn3Min, 'f', 2));
            report_string.append(blackHtml % ". Max: ");
            report_string.append(auxIn3MaxHtml % QString::number(auxIn3Max, 'f', 2));
            report_string.append(blackHtml % ". Median: ");
            report_string.append(auxIn3MedianHtml % QString::number(auxIn3Median, 'f', 2) % ".");
        }
    }

    QTextEdit *report_te = new QTextEdit;
    report_te->setText(report_string);
    report_te->setReadOnly(true);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(report_te);

    reportDialog.setLayout(mainLayout);

    reportDialog.setWindowTitle("Report of Channels");
    reportDialog.setMinimumWidth(500);
    reportDialog.exec();
}

void TestControlPanel::saveReport()
{
    QString csvFileName;
    csvFileName = QFileDialog::getSaveFileName(this,
                                            tr("Save Report Data As"), ".",
                                            tr("CSV (Comma delimited) (*.csv)"));

    // Recalculate report (in case parameters were changed since test was run)
    checkDCWaveforms();
    generateReport(channels);

    if (!csvFileName.isEmpty()) {
        QFile csvFile(csvFileName);

        if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            std::cerr << "Cannot open CSV file for writing: " <<
                    qPrintable(csvFile.errorString()) << std::endl;
        }
        QTextStream out(&csvFile);
        if (state->getControllerTypeEnum() == ControllerStimRecord) {
            out << "Channel Number,Triangle Error,Stim Error,Avg Positive Voltage, Avg Negative Voltage" << Qt::endl;
        } else {
            out << "Channel Number,Triangle Error,Settle Error" << Qt::endl;
        }

        for (int channel = 0; channel < report.size(); channel++) {
            out << report.at(channel)->actualChannelNumber << ",";
            out << report.at(channel)->triangleError << ",";
            if (state->getControllerTypeEnum() == ControllerStimRecord) {
                out << report.at(channel)->variableError << ",";
                out << report.at(channel)->posAvg << ",";
                out << report.at(channel)->negAvg << Qt::endl;
            } else {
                out << report.at(channel)->variableError << Qt::endl;
            }
            //out << report.at(channel)->variableError << Qt::endl;
        }
        csvFile.close();
    }
}

void TestControlPanel::uploadStimManual()
{
    QVector<int> connectedStreams = getConnectedCommandStreams();
    QProgressDialog progress(QObject::tr("Uploading Stim Parameters"), QString(), 0, connectedStreams.size());
    progress.setMinimumDuration(0);
    progress.setModal(true);
    for (auto it = connectedStreams.begin(); it != connectedStreams.end(); ++it) {
        auto i = std::distance(connectedStreams.begin(), it);
        progress.setValue(i);
        controllerInterface->uploadAutoStimParameters(*it);
    }
}

void TestControlPanel::clearStimManual()
{
    QVector<int> connectedStreams = getConnectedCommandStreams();
    QProgressDialog progress(QObject::tr("Clearing Stim Parameters"), QString(), 0, connectedStreams.size());
    progress.setMinimumDuration(0);
    progress.setModal(true);
    for (auto it = connectedStreams.begin(); it != connectedStreams.end(); ++it) {
        auto i = std::distance(connectedStreams.begin(), it);
        progress.setValue(i);
        controllerInterface->clearStimParameters(*it);
    }
}

QVector<int> TestControlPanel::getConnectedCommandStreams()
{
    QVector<int> connectedStreams;
    for (auto& name: state->signalSources->amplifierChannelsNameList()) {
        int commandStream = state->signalSources->channelByName(name)->getCommandStream();
        if (!connectedStreams.contains(commandStream)) { connectedStreams.append(commandStream); }
    }
    return connectedStreams;
}

double TestControlPanel::vectorAvg(QVector<double> vect)
{
    return vectorAvg(vect, 0, vect.size());
}

double TestControlPanel::vectorAvg(QVector<double> vect, int start, int end)
{
    double sum = 0;
    for (int i = start; i < end; ++i) {
        sum += vect[i];
    }
    return sum / (double) (end - start);
}


ReportDialog::ReportDialog(QWidget *parent)
{
    Q_UNUSED(parent);
}

void ReportDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_V) {
        close();
    }
    else {
        QDialog::keyPressEvent(event);
    }
}
