#ifndef ABSTRACTPANEL_H
#define ABSTRACTPANEL_H

#include <QtWidgets>
#include "controllerinterface.h"
#include "systemstate.h"
#include "commandparser.h"
#include "filterdisplayselector.h"

class ControlWindow;
class StimParamDialog;
class AnOutDialog;
class DigOutDialog;
class ControlPanelConfigureTab;

class ColorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWidget(QWidget *parent = nullptr);

    void setEnabled(bool enabled) { chooseColorToolButton->setEnabled(enabled); }
    void setColor(QColor color);
    void setBlank() { chooseColorToolButton->setIcon(blankIcon); }
    void setCheckerboard() { chooseColorToolButton->setIcon(checkerboardIcon); }

signals:
    void clicked();

private:
    QToolButton *chooseColorToolButton;

    QIcon checkerboardIcon;
    QIcon blankIcon;
};

class AbstractPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AbstractPanel(ControllerInterface* controllerInterface_, SystemState* state_, CommandParser *parser_, ControlWindow *parent = nullptr);
    ~AbstractPanel();

    virtual void updateForRun() = 0;
    void updateForLoad();
    virtual void updateForStop() = 0;
    virtual void updateSlidersEnabled(YScaleUsed yScaleUsed) = 0;
    virtual YScaleUsed slidersEnabled() const = 0;

    virtual void setCurrentTabName(QString tabName) = 0;
    virtual QString currentTabName() const = 0;

public slots:
    virtual void updateFromState() = 0;

protected slots:
    void promptColorChange();
    void clipWaveforms(int checked);
    void changeTimeScale(int index);
    void changeWideScale(int index);
    void changeLowScale(int index);
    void changeHighScale(int index);
    void changeAuxScale(int index);
    void changeAnaScale(int index);
    void changeDCScale(int index);
    void openStimParametersDialog();
    void enableChannelsSlot();

protected:
    virtual QHBoxLayout* createSelectionLayout() = 0;
    virtual QHBoxLayout* createSelectionToolsLayout() = 0;
    virtual QHBoxLayout* createDisplayLayout() = 0;

    virtual void updateYScales() = 0;
    void updateStimParamDialogButton();
    void updateSelectionName();
    void updateElectrodeImpedance();
    void updateReferenceChannel();
    void updateStimTrigger();
    void updateColor();
    void updateEnableCheckBox();

    SystemState* state;
    CommandParser* parser;
    ControlWindow* controlWindow;
    ControllerInterface* controllerInterface;

    FilterDisplaySelector *filterDisplaySelector;
    QTabWidget *tabWidget;
    ControlPanelConfigureTab *configureTab;

    QList<Channel*> selectedSignals;

    StimParamDialog* stimParamDialog;
    AnOutDialog* anOutDialog;
    DigOutDialog* digOutDialog;

    QLabel *selectionNameLabel;
    QLabel *selectionImpedanceLabel;
    QLabel *selectionReferenceLabel;
    QLabel *selectionStimTriggerLabel;

    ColorWidget *colorAttribute;
    QCheckBox *enableCheckBox;
    QPushButton *renameButton;
    QPushButton *setRefButton;
    QPushButton *setStimButton;

    QSlider *wideSlider;
    QSlider *variableSlider;

    QLabel *wideLabel;
    QLabel *variableLabel;

    QCheckBox* clipWaveformsCheckBox;
    QComboBox *timeScaleComboBox;

signals:

};

#endif // ABSTRACTPANEL_H
