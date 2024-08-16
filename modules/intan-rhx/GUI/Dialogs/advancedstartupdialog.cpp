#include "advancedstartupdialog.h"
#include <QtWidgets>

AdvancedStartupDialog::AdvancedStartupDialog(bool &useOpenCL_, uint8_t &playbackPorts_, bool demoMode_, QWidget *parent) :
    QDialog(parent),
    useOpenCLDescription(nullptr),
    useOpenCLCheckBox(nullptr),
    synthMaxChannelsDescription(nullptr),
    synthMaxChannelsCheckBox(nullptr),
    playbackControlDescription(nullptr),
    playbackACheckBox(nullptr),
    playbackBCheckBox(nullptr),
    playbackCCheckBox(nullptr),
    playbackDCheckBox(nullptr),
    playbackECheckBox(nullptr),
    playbackFCheckBox(nullptr),
    playbackGCheckBox(nullptr),
    playbackHCheckBox(nullptr),
    buttonBox(nullptr),
    useOpenCL(&useOpenCL_),
    tempUseOpenCL(useOpenCL_),
    playbackPorts(&playbackPorts_),
    demoMode(demoMode_)
{
    useOpenCLDescription = new QLabel(tr(
        "OpenCL is a platform-independent framework that allows the CPU to\n"
        "send data to a GPU for processing. This can result in faster data\n"
        "processing and filtering, and is most effective with higher channel\n"
        "counts and faster sample rates. CPUs and GPUs can be selected for use\n"
        "in the XPU section of the Performance Optimization dialog, from the\n"
        "Performance menu of the main software window.\n\n"
        "It is possible to disable this feature so that only the CPU will\n"
        "process data. This is suitable for systems incompatible with OpenCL,\n"
        "but will increase the workload of the CPU."), this);

    useOpenCLCheckBox = new QCheckBox(tr("Use OpenCL"), this);
    useOpenCLCheckBox->setChecked(useOpenCL_);
    connect(useOpenCLCheckBox, SIGNAL(clicked(bool)), this, SLOT(changeUseOpenCL(bool)));

    playbackControlDescription = new QLabel(tr(
        "If running in playback mode, the RHX software will attempt to read\n"
        "all data that is present for all selected ports. If this instance of\n"
        "this software will only be used to read from specific ports, other\n"
        "ports can be de-selected to improve the performance by limiting how\n"
        "much data is read from disk."), this);

    portsBool = portsIntToBool(playbackPorts_);

    playbackACheckBox = new QCheckBox("Port A", this);
    playbackACheckBox->setChecked(portsBool[0]);

    playbackBCheckBox = new QCheckBox("Port B", this);
    playbackBCheckBox->setChecked(portsBool[1]);

    playbackCCheckBox = new QCheckBox("Port C", this);
    playbackCCheckBox->setChecked(portsBool[2]);

    playbackDCheckBox = new QCheckBox("Port D", this);
    playbackDCheckBox->setChecked(portsBool[3]);

    playbackECheckBox = new QCheckBox("Port E", this);
    playbackECheckBox->setChecked(portsBool[4]);

    playbackFCheckBox = new QCheckBox("Port F", this);
    playbackFCheckBox->setChecked(portsBool[5]);

    playbackGCheckBox = new QCheckBox("Port G", this);
    playbackGCheckBox->setChecked(portsBool[6]);

    playbackHCheckBox = new QCheckBox("Port H", this);
    playbackHCheckBox->setChecked(portsBool[7]);

    synthMaxChannelsDescription = new QLabel(tr("When running in demonstration mode, generate synthetic waveforms\n"
                                                "on the maximum possible number of channels for the controller."), this);

    synthMaxChannelsCheckBox = new QCheckBox(tr("Maximum Number of Channels in Demonstration Mode"), this);

    testModeDescription = new QLabel(tr("When running in chip test mode, replace typical recording functionality with\n"
                                           "automated testing.\n\nFor internal factory use only.  Headstage users should not use this mode."), this);

    testModeCheckBox = new QCheckBox(tr("Chip Test Mode"));

    QSettings settings;
    synthMaxChannelsCheckBox->setChecked(settings.value("synthMaxChannels", false).toBool());
    connect(synthMaxChannelsCheckBox, SIGNAL(clicked(bool)), this, SLOT(changeSynthMaxChannels(bool)));

    QString testModeString = settings.value("chipTestMode", "").toString();
    testModeCheckBox->setChecked(testModeString == "Intan Chip Test Mode");
    connect(testModeCheckBox, SIGNAL(clicked(bool)), this, SLOT(changeTestMode(bool)));

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *openCLLayout = new QVBoxLayout;
    openCLLayout->addWidget(useOpenCLDescription);
    openCLLayout->addWidget(useOpenCLCheckBox);

    QGroupBox *openCLGroupBox = new QGroupBox(tr("OpenCL"), this);
    openCLGroupBox->setLayout(openCLLayout);

    QVBoxLayout *playbackControlLayout = new QVBoxLayout;
    playbackControlLayout->addWidget(playbackControlDescription);
    playbackControlLayout->addWidget(playbackACheckBox);
    playbackControlLayout->addWidget(playbackBCheckBox);
    playbackControlLayout->addWidget(playbackCCheckBox);
    playbackControlLayout->addWidget(playbackDCheckBox);
    playbackControlLayout->addWidget(playbackECheckBox);
    playbackControlLayout->addWidget(playbackFCheckBox);
    playbackControlLayout->addWidget(playbackGCheckBox);
    playbackControlLayout->addWidget(playbackHCheckBox);

    playbackControlGroupBox = new QGroupBox(tr("Playback Control"), this);
    playbackControlGroupBox->setLayout(playbackControlLayout);

    QVBoxLayout *synthMaxChannelsLayout = new QVBoxLayout;
    synthMaxChannelsLayout->addWidget(synthMaxChannelsDescription);
    synthMaxChannelsLayout->addWidget(synthMaxChannelsCheckBox);

    synthMaxChannelsGroupBox = new QGroupBox(tr("Demonstration Mode Data Generation"), this);
    synthMaxChannelsGroupBox->setLayout(synthMaxChannelsLayout);

    QVBoxLayout *testModeLayout = new QVBoxLayout;
    testModeLayout->addWidget(testModeDescription);
    testModeLayout->addWidget(testModeCheckBox);

    QGroupBox *testModeGroupBox = new QGroupBox(tr("Chip Test Mode"), this);
    testModeGroupBox->setLayout(testModeLayout);
    testModeGroupBox->setDisabled(demoMode);

    QPalette palette;
    palette.setColor(QPalette::WindowText, demoMode ? Qt::gray : Qt::black);
    testModeGroupBox->setPalette(palette);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(openCLGroupBox);
    mainLayout->addWidget(playbackControlGroupBox);
    mainLayout->addWidget(synthMaxChannelsGroupBox);
    mainLayout->addWidget(testModeGroupBox);
    mainLayout->addWidget(buttonBox);

    QWidget *mainWidget = new QWidget(this);
    mainWidget->setLayout(mainLayout);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(mainWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QVBoxLayout *scrollLayout = new QVBoxLayout;
    scrollLayout->addWidget(scrollArea);

    setLayout(scrollLayout);

    setWindowTitle(tr("Advanced Startup Settings"));
    updateUIForTestMode();
}

// Encoded as HGFEDCBA
QVector<bool> AdvancedStartupDialog::portsIntToBool(uint8_t portsInt)
{
    QVector<bool> portsBool_;
    portsBool_.resize(8);
    for (int port = 0; port < 8; ++port) {
        portsBool_[port] = portsInt & (1 << port);
    }
    return portsBool_;
}

// Encoded as HGFEDCBA
uint8_t AdvancedStartupDialog::portsBoolToInt(QVector<bool> portsBool)
{
    uint8_t portsInt = 0;
    for (int port = 0; port < 8; ++port) {
        if (portsBool[port]) {
            portsInt += 1 << port;
        }
    }
    return portsInt;
}

void AdvancedStartupDialog::changeUseOpenCL(bool use)
{
    tempUseOpenCL = use;
}

void AdvancedStartupDialog::changeSynthMaxChannels(bool max)
{
    tempSynthMaxChannels = max;
}

void AdvancedStartupDialog::changeTestMode(bool test)
{
    tempTest = test;
    updateUIForTestMode();
}

void AdvancedStartupDialog::updateUIForTestMode()
{
    bool testModeSelected = testModeCheckBox->isChecked() && !demoMode;

    playbackControlGroupBox->setDisabled(testModeSelected);
    synthMaxChannelsGroupBox->setDisabled(testModeSelected);

    QPalette palette;
    palette.setColor(QPalette::WindowText, testModeSelected ? Qt::gray : Qt::black);
    playbackControlGroupBox->setPalette(palette);
    synthMaxChannelsGroupBox->setPalette(palette);
}

void AdvancedStartupDialog::accept()
{
    QSettings settings;
    settings.setValue("synthMaxChannels", tempSynthMaxChannels);
    QString testModeString = tempTest ? "Intan Chip Test Mode" : "";
    settings.setValue("chipTestMode", testModeString);

    *useOpenCL = tempUseOpenCL;

    portsBool[0] = playbackACheckBox->isChecked();
    portsBool[1] = playbackBCheckBox->isChecked();
    portsBool[2] = playbackCCheckBox->isChecked();
    portsBool[3] = playbackDCheckBox->isChecked();
    portsBool[4] = playbackECheckBox->isChecked();
    portsBool[5] = playbackFCheckBox->isChecked();
    portsBool[6] = playbackGCheckBox->isChecked();
    portsBool[7] = playbackHCheckBox->isChecked();
    *playbackPorts = portsBoolToInt(portsBool);

    done(Accepted);
}
