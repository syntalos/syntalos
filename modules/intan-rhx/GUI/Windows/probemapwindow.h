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

#ifndef PROBEMAPWINDOW_H
#define PROBEMAPWINDOW_H

#include <QtWidgets>

#include "rhxglobals.h"
#include "impedancegradient.h"
#include "pageview.h"
#include "systemstate.h"
#include "xmlinterface.h"
#include "controllerinterface.h"

typedef map<string, double> ChannelList;

class ProbeMapWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ProbeMapWindow(SystemState* state_, ControllerInterface* controllerInterface_, QMainWindow *parent = nullptr);
    ~ProbeMapWindow();

    QSize sizeHint() const override;

    void updateForRun();
    void updateForLoad();
    void updateForStop();

    QVector<ElectrodeSite*> getSitesWithName(const QString& nativeName); // Return references to all sites with the given nativeName

signals:
    // The mouse's role has been changed to a new MouseMode
    void modeChanged(MouseMode mode);

public slots:
    void updateFromState();
    void updateMouseStatus(float x, float y, bool mousePresent, QString hoveredSiteName);

    void enableChannel(QString nativeName, bool enabled);
    void changeChannelColor(QString nativeName, QString color);
    void changeChannelImpedance(QString nativeName, float impedanceMag, float impedancePhase);
    void linkAndUpdateSites();

private slots:
    void load();
    void pageChanged(int index);
    void mouseLeft();
    void changeMode();
    void endOfResize();
    void highlightSiteFromMap(QString nativeName, bool highlighted);
    void deselectAllSitesFromMap();
    void toggleView();
    void catchSpikeReport(QString names);
    void catchSpikeTimerTick();
    void changeSpikeDecay();

private:
    SystemState* state;
    ControllerInterface* controllerInterface;

    ChannelList activeChannels;
    QElapsedTimer spikeTimer;

    XMLInterface *probeMapSettingsInterface;

    QMenu *fileMenu;

    QToolBar *toolBar;
    QAction *loadAction;
    QActionGroup *mouseActions;

    QAction *bestFitAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;

    QAction *selectAction;
    QAction *scrollAction;
    QAction *resizeAction;

    QAction *scrollUpAction;
    QAction *scrollDownAction;
    QAction *scrollLeftAction;
    QAction *scrollRightAction;

    QAction *defaultViewAction;
    QAction *impedanceViewAction;
    QAction *spikeViewAction;

    QActionGroup *viewActions;

    QTabWidget *pageTabWidget;
    QLabel *statusCoords;
    QLabel *statusSiteInfo;

    ImpedanceGradient *impedanceGradient;
    SpikeGradient *spikeGradient;

    QLabel *spikeDecayLabel;
    QComboBox *spikeDecayComboBox;

    int currentPage;

    void populateTabWidget(); // Create new PageViews and populate the tab widget with them
    void clearTabWidget(); // Delete PageViews and clear the tab widget

    void enableActions(bool enable);

    void updateSpikeTimerSites();

    map<int, double> decayOptions;
};

#endif // PROBEMAPWINDOW_H
