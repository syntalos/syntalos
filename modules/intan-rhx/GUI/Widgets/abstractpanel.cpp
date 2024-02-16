#include "abstractpanel.h"
#include "stimparamdialog.h"
#include "anoutdialog.h"
#include "digoutdialog.h"
#include "controlwindow.h"
#include "controlpanelconfiguretab.h"

ColorWidget::ColorWidget(QWidget *parent) :
    QWidget(parent)
{
    checkerboardIcon = QIcon(":/images/checkerboard.png");

    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(204, 204, 204));
    blankIcon = QIcon(pixmap);

    chooseColorToolButton = new QToolButton(this);
    chooseColorToolButton->setIcon(blankIcon);
    connect(chooseColorToolButton, SIGNAL(clicked()), this, SIGNAL(clicked()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(chooseColorToolButton);
    setLayout(mainLayout);
}

void ColorWidget::setColor(QColor color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(QColor(color));
    chooseColorToolButton->setIcon(QIcon(pixmap));
}

AbstractPanel::AbstractPanel(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser* parser_, ControlWindow *parent) :
    controlWindow(parent),
    controllerInterface(controllerInterface_),
    state(state_),
    parser(parser_),
    filterDisplaySelector(nullptr),
    tabWidget(nullptr),
    configureTab(nullptr),
    stimParamDialog(nullptr),
    anOutDialog(nullptr),
    digOutDialog(nullptr),
    selectionNameLabel(nullptr),
    selectionImpedanceLabel(nullptr),
    selectionReferenceLabel(nullptr),
    selectionStimTriggerLabel(nullptr),
    colorAttribute(nullptr),
    enableCheckBox(nullptr),
    renameButton(nullptr),
    setRefButton(nullptr),
    setStimButton(nullptr),
    wideSlider(nullptr),
    variableSlider(nullptr),
    wideLabel(nullptr),
    variableLabel(nullptr),
    clipWaveformsCheckBox(nullptr),
    timeScaleComboBox(nullptr)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);

    filterDisplaySelector = new FilterDisplaySelector(state, this);

    configureTab = new ControlPanelConfigureTab(controllerInterface, state, parser, this);

    tabWidget = new QTabWidget(this);
}

AbstractPanel::~AbstractPanel()
{
    if (stimParamDialog) delete stimParamDialog;
    if (anOutDialog) delete anOutDialog;
    if (digOutDialog) delete digOutDialog;
}

void AbstractPanel::updateForLoad()
{
    setEnabled(false);
}

void AbstractPanel::promptColorChange()
{
    // Identify all currently selected channels.
    state->signalSources->getSelectedSignals(selectedSignals);

    if (!selectedSignals.empty()) {
        // Get QColor from modal dialog from user.
        QColor result = QColorDialog::getColor(selectedSignals.at(0)->getColor(), this);

        // If this color is valid (user didn't cancel), set the color for all currently selected channels.
        if (result.isValid()) {
            state->signalSources->undoManager->pushStateToUndoStack();
            for (int i = 0; i < selectedSignals.size(); i++) {
                selectedSignals[i]->setColor(result);
                state->forceUpdate();
            }
        }
    }
}

void AbstractPanel::clipWaveforms(int checked)
{
    state->clipWaveforms->setValue(checked != 0);
}

void AbstractPanel::changeTimeScale(int index)
{
    state->tScale->setIndex(index);
}

void AbstractPanel::changeWideScale(int index)
{
    state->yScaleWide->setIndex(index);
}

void AbstractPanel::changeLowScale(int index)
{
    state->yScaleLow->setIndex(index);
}

void AbstractPanel::changeHighScale(int index)
{
    state->yScaleHigh->setIndex(index);
}

void AbstractPanel::changeAuxScale(int index)
{
    state->yScaleAux->setIndex(index);
}

void AbstractPanel::changeAnaScale(int index)
{
    state->yScaleAnalog->setIndex(index);
}

void AbstractPanel::changeDCScale(int index)
{
    state->yScaleDC->setIndex(index);
}

void AbstractPanel::openStimParametersDialog()
{
    // This slot should only be called when a single channel is selected AND board is not running.
    Channel* selectedChannel = state->signalSources->selectedChannel();
    SignalType type = selectedChannel->getSignalType();

    if (type == AmplifierSignal) {
        if (stimParamDialog) {
            disconnect(this, nullptr, stimParamDialog, nullptr);
            delete stimParamDialog;
            stimParamDialog = nullptr;
        }
        stimParamDialog = new StimParamDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), stimParamDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (stimParamDialog->exec() == QDialog::Accepted) {
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    } else if (type == BoardDigitalOutSignal) {
        if (digOutDialog) {
            disconnect(this, nullptr, digOutDialog, nullptr);
            delete digOutDialog;
            digOutDialog = nullptr;
        }
        digOutDialog = new DigOutDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), digOutDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (digOutDialog->exec() == QDialog::Accepted){
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    } else if (type == BoardDacSignal) {
        if (anOutDialog) {
            disconnect(this, nullptr, anOutDialog, nullptr);
            delete anOutDialog;
            anOutDialog = nullptr;
        }
        anOutDialog = new AnOutDialog(state, selectedChannel, this);
        connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), anOutDialog, SLOT(notifyFocusChanged(QWidget*, QWidget*)));

        if (anOutDialog->exec() == QDialog::Accepted) {
            state->stimParamsHaveChanged = true;
            controllerInterface->uploadStimParameters(selectedChannel);
        }
    }
    state->forceUpdate();
}

void AbstractPanel::enableChannelsSlot()
{
    bool enable = enableCheckBox->checkState() == Qt::Checked;
    controlWindow->enableSelectedChannels(enable);
}

void AbstractPanel::updateStimParamDialogButton()
{
    // If a single channel with stim capability is selected and board is not currently running, then enable the button.
    bool hasStimCapability = false;
    if (state->signalSources->numChannelsSelected() == 1) {
        Channel* channel = state->signalSources->selectedChannel();
        hasStimCapability = channel->isStimCapable();
    }

    setStimButton->setEnabled(hasStimCapability && !(state->running || state->sweeping));
}

void AbstractPanel::updateSelectionName()
{
    if (selectedSignals.isEmpty()) {
        selectionNameLabel->setText(tr("no selection"));
        renameButton->setEnabled(false);
    } else if (selectedSignals.size() == 1) {
        QString name = selectedSignals.at(0)->getNativeAndCustomNames();
        selectionNameLabel->setText(name);
        renameButton->setEnabled(true);
    } else {
        bool allSameGroup = true;
        int groupID = selectedSignals.at(0)->getGroupID();
        if (groupID != 0) {
            for (int i = 1; i < selectedSignals.size(); ++i) {
                if (selectedSignals.at(i)->getGroupID() != groupID) {
                    allSameGroup = false;
                    break;
                }
            }
        } else {
            allSameGroup = false;
        }
        if (allSameGroup) {
            QString name = selectedSignals.at(0)->getNativeAndCustomNames();
            selectionNameLabel->setText(name + tr(" GROUP"));
        } else {
            selectionNameLabel->setText("");
        }
        renameButton->setEnabled(false);
    }
    if (state->running || state->sweeping) renameButton->setEnabled(false);
}

void AbstractPanel::updateElectrodeImpedance()
{
    QString impedanceText;
    if (selectedSignals.isEmpty()) {
        impedanceText = tr("no selection");
    } else if (selectedSignals.at(0)->getSignalType() != AmplifierSignal) {
        impedanceText = tr("n/a");
    } else if (!selectedSignals.at(0)->isImpedanceValid()) {
        impedanceText = tr("not measured");
    } else if (selectedSignals.size() == 1) {
        impedanceText = selectedSignals.at(0)->getImpedanceMagnitudeString() + " " +
                selectedSignals.at(0)->getImpedancePhaseString();
    }
    selectionImpedanceLabel->setText(impedanceText);
}

void AbstractPanel::updateReferenceChannel()
{
    if (selectedSignals.isEmpty()) {
        selectionReferenceLabel->setText(tr("no selection"));
        setRefButton->setEnabled(false);
        return;
    }

    if (selectedSignals.at(0)->getSignalType() != AmplifierSignal) {
        selectionReferenceLabel->setText(tr("n/a"));
        setRefButton->setEnabled(false);
        return;
    }

    bool sameReference = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getReference() != selectedSignals.at(i + 1)->getReference()) {
            sameReference = false;
            break;
        }
    }
    if (sameReference) {
        QString refText = selectedSignals.at(0)->getReference();
        if (refText.length() > 36) refText = refText.left(36) + "...";
        selectionReferenceLabel->setText(refText);
    } else {
        selectionReferenceLabel->setText("");
    }
    setRefButton->setEnabled(!(state->running || state->sweeping) && state->signalSources->onlyAmplifierChannelsSelected());
}

void AbstractPanel::updateStimTrigger()
{
    if (selectedSignals.isEmpty()) {
        selectionStimTriggerLabel->setText(tr("no selection"));
        setStimButton->setEnabled(false);
        return;
    }

    if (!selectedSignals.at(0)->isStimCapable()) {
        selectionStimTriggerLabel->setText(tr("n/a"));
        setStimButton->setEnabled(false);
        return;
    }

    bool sameStimTrigger = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getStimTrigger() != selectedSignals.at(i + 1)->getStimTrigger() ||
                !selectedSignals.at(i)->isStimEnabled() || !selectedSignals.at(i + 1)->isStimEnabled()) {
            sameStimTrigger = false;
            break;
        }
    }
    if (sameStimTrigger) {
        if (selectedSignals.at(0)->isStimEnabled()) {
            selectionStimTriggerLabel->setText(selectedSignals.at(0)->getStimTrigger());
        } else {
            selectionStimTriggerLabel->setText("");
        }
    } else {
        selectionStimTriggerLabel->setText("");
    }
    if (selectedSignals.size() == 1) {
        setStimButton->setEnabled(!(state->running || state->sweeping));
    }
}

void AbstractPanel::updateColor()
{
    if (selectedSignals.isEmpty()) {
        colorAttribute->setBlank();
        colorAttribute->setEnabled(false);
        return;
    }

    bool sameColor = true;
    for (int i = 0; i < selectedSignals.size() - 1; i++) {
        if (selectedSignals.at(i)->getColor() != selectedSignals.at(i + 1)->getColor()) {
            sameColor = false;
            break;
        }
    }
    if (sameColor) {
        colorAttribute->setColor(selectedSignals.at(0)->getColor());
        colorAttribute->setEnabled(!(state->running || state->sweeping));
    } else {
        colorAttribute->setCheckerboard();
        colorAttribute->setEnabled(!(state->running || state->sweeping));
    }
}

void AbstractPanel::updateEnableCheckBox()
{
    int enableState = state->signalSources->enableStateOfSelectedChannels();
    enableCheckBox->setEnabled(enableState >= 0);
    if (enableState >= 0) {
        Qt::CheckState checkState = (Qt::CheckState) enableState;
        enableCheckBox->setCheckState(checkState);
        if (checkState == Qt::Unchecked || checkState == Qt::Checked) {
            enableCheckBox->setTristate(false);
        }
    }
}
