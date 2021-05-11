//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
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
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
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

enum UsbVersion {
    USB2,
    USB3
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

class ScrollableMessageBox : public QDialog
{
    Q_OBJECT
public:
    explicit ScrollableMessageBox(QWidget *parent = nullptr, const QString &title = "", const QString &text = "");

private:
    QLabel *message;
    QPushButton *okButton;
};

class QPushButton;
class QTableWidget;

class BoardSelectDialog : public QDialog
{
    Q_OBJECT
public:
    BoardSelectDialog(IntanRhxModule *mod, QWidget *parent = nullptr);
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

private:
    void populateTable();
    QSize calculateTableSize();

    void showDemoMessageBox();
    void startSoftware(ControllerType controllerType, AmplifierSampleRate sampleRate, StimStepSize stimStepSize,
                       int numSPIPorts, bool expanderConnected, const QString& boardSerialNumber, AcquisitionMode mode);

    QTableWidget *boardTable;
    QPushButton *openButton;

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

    IntanRhxModule *syMod;
};

#endif // BOARDSELECTDIALOG_H
