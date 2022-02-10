#include "advancedstartupdialog.h"
#include <QtWidgets>

AdvancedStartupDialog::AdvancedStartupDialog(bool &useOpenCL_, QWidget *parent) :
    QDialog(parent),
    useOpenCLDescription(nullptr),
    useOpenCLCheckBox(nullptr),
    buttonBox(nullptr),
    useOpenCL(&useOpenCL_),
    tempUseOpenCL(useOpenCL_)
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

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *openCLLayout = new QVBoxLayout;
    openCLLayout->addWidget(useOpenCLDescription);
    openCLLayout->addWidget(useOpenCLCheckBox);

    QGroupBox *openCLGroupBox = new QGroupBox(tr("OpenCL"), this);
    openCLGroupBox->setLayout(openCLLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(openCLGroupBox);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    setWindowTitle(tr("Advanced Startup Settings"));
}

void AdvancedStartupDialog::changeUseOpenCL(bool use)
{
    tempUseOpenCL = use;
}

void AdvancedStartupDialog::accept()
{
    *useOpenCL = tempUseOpenCL;
    done(Accepted);
}
