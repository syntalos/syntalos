//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.5.0
//
//  Copyright (c) 2020-2026 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <https://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#ifndef BOARDSELECTDIALOG_H
#define BOARDSELECTDIALOG_H

#include <QDialog>

#include "demodialog.h"
#include "startupdialog.h"
#include "rhxcontroller.h"
#include "syntheticrhxcontroller.h"
#include "playbackrhxcontroller.h"
#include "rhxglobals.h"
#include "controlwindow.h"
#include "controllerinterface.h"
#include "systemstate.h"
#include "commandparser.h"

const QString RHDBoardString = "RHD USB Interface Board";
const QString RHD512chString = "RHD 512ch Recording Controller";
const QString RHD1024chString = "RHD 1024ch Recording Controller";
const QString RHS128chString = "RHS 128ch Stim/Recording Controller";
const QString CLAMP2chString = "2ch CLAMP Controller";
const QString CLAMP8chString = "8ch CLAMP Controller";
const QString UnknownUSB2String = "Unknown USB2 Device";
const QString UnknownUSB3String = "Unknown USB3 Device";
const QString UnknownString = "Unknown Device";
const QString RHS128ch_7310String = "RHS 128ch Stim/Recording Controller (7310)";
const QString RHD512ch_7310String = "RHD 512ch Recording Controller (7310)";
const QString RHD1024ch_7310String = "RHD 1024ch Recording Controller (7310)";

constexpr std::array<std::pair<int, AmplifierSampleRate>, 17> sampleRateTable {{
    {1000, SampleRate1000Hz},
    {1250, SampleRate1250Hz},
    {1500, SampleRate1500Hz},
    {2000, SampleRate2000Hz},
    {2500, SampleRate2500Hz},
    {3000, SampleRate3000Hz},
    {3333, SampleRate3333Hz},
    {4000, SampleRate4000Hz},
    {5000, SampleRate5000Hz},
    {6250, SampleRate6250Hz},
    {8000, SampleRate8000Hz},
    {10000, SampleRate10000Hz},
    {12500, SampleRate12500Hz},
    {15000, SampleRate15000Hz},
    {20000, SampleRate20000Hz},
    {25000, SampleRate25000Hz},
    {30000, SampleRate30000Hz}
}};

constexpr std::array<std::pair<double, StimStepSize>, 10> stimStepSizeTable {{
    {0.01, StimStepSize10nA},
    {0.02, StimStepSize20nA},
    {0.05, StimStepSize50nA},
    {0.1,  StimStepSize100nA},
    {0.2,  StimStepSize200nA},
    {0.5,  StimStepSize500nA},
    {1.0,  StimStepSize1uA},
    {2.0,  StimStepSize2uA},
    {5.0,  StimStepSize5uA},
    {10.0, StimStepSize10uA}
}};

enum UsbVersion {
    USB2,
    USB3,
    USB3_7310
};

struct ControllerInfo {
    QString serialNumber;
    UsbVersion usbVersion;
    bool expConnected;
    int numSPIPorts;
    BoardMode boardMode;
};

class BoardIdentifier
{
public:
    BoardIdentifier(const QString &bitfileDir, QWidget* parent_);
    ~BoardIdentifier();

    static QString getBoardTypeString(BoardMode mode, int numSpiPorts);
    static QIcon getIcon(const QString& boardType, QStyle *style, int size);

    QVector<ControllerInfo*> getConnectedControllersInfo();

private:
    void identifyController(ControllerInfo* controller, int index);
    QString opalKellyModelName(int model) const;
    bool uploadFpgaBitfileQMessageBox(const QString& filename);

    QVector<ControllerInfo*> controllers;
    QWidget *parent;

    okCFrontPanel *dev;
    QString bitfileRootDir;
};

class QPushButton;
class QTableWidget;

class BoardSelectDialog : public QDialog
{
    Q_OBJECT
public:
    BoardSelectDialog(IntanRhxModule *mod, QString settingsFileName, QWidget *parent = nullptr);
    ~BoardSelectDialog();

    static bool validControllersPresent(QVector<ControllerInfo*> cInfo);

    ControlWindow *getControlWindow() const;
    ControllerInterface *getControllerInterface() const;
    SystemState *systemState() const;

private slots:
    void openSelectedBoard();
    void newRowSelected(int row);
    void startBoard(int row);
    void playbackDataFile();
    void advanced();

private:
    void populateTable();
    QSize calculateTableSize();

    void showDemoMessageBox();
    void startSoftware(ControllerType controllerType, AmplifierSampleRate sampleRate, StimStepSize stimStepSize,
                       int numSPIPorts, bool expanderConnected, const QString& boardSerialNumber, AcquisitionMode mode, bool is7310, DataFileReader* dataFileReader=nullptr);
    void startSoftwareFromSettings(QString settingsFileName);

    AmplifierSampleRate parseSampleRate(const QString& sampleRateStr, ControllerType controllerType);
    StimStepSize parseStimStepSize(const QString& stimStepSizeStr);
    ControllerType getControllerType(const ControllerInfo& info);

    QTableWidget *boardTable;
    QPushButton *openButton;
    QPushButton *advancedButton;

    QCheckBox *defaultSampleRateCheckBox;
    QCheckBox *defaultSettingsFileCheckBox;

    BoardIdentifier *boardIdentifier;
    QVector<ControllerInfo*> controllersInfo;
    DataFileReader *dataFileReader;

    AbstractRHXController *rhxController;
    SystemState *state;
    ControllerInterface *controllerInterface;
    CommandParser *parser;
    ControlWindow *controlWindow;

    bool useOpenCL;
    uint8_t playbackPorts;

    IntanRhxModule *syMod;
};

#endif // BOARDSELECTDIALOG_H
