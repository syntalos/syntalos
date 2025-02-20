//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
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

#include <QtXml>
#include <QSettings>
#include "signalsources.h"
#include "probemapwindow.h"

ProbeMapWindow::ProbeMapWindow(SystemState* state_, ControllerInterface* controllerInterface_, QMainWindow *parent) :
    QMainWindow(parent),
    state(state_),
    controllerInterface(controllerInterface_)
{
    setAcceptDrops(true);
    connect(state, SIGNAL(stateChanged()), this, SLOT(updateFromState()));
    connect(state, SIGNAL(spikeReportSignal(QString)), this, SLOT(catchSpikeReport(QString)));
    connect(state, SIGNAL(spikeTimerTick()), this, SLOT(catchSpikeTimerTick()));
    currentPage = -1;

    setWindowTitle(tr("Probe Map"));

    loadAction = new QAction(tr("Load Probe Description File"), this);

    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(loadAction);

    mouseActions = new QActionGroup(this);

    resizeAction = new QAction(QIcon(":/images/resize.png"), "Resize mode", this);
    resizeAction->setCheckable(true);
    mouseActions->addAction(resizeAction);

    selectAction = new QAction(QIcon(":/images/select.png"), "Select mode", this);
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    resizeAction->setEnabled(false);
    mouseActions->addAction(selectAction);

    scrollAction = new QAction(QIcon(":/images/scroll.png"), "Scroll mode", this);
    scrollAction->setCheckable(true);
    mouseActions->addAction(scrollAction);

    bestFitAction = new QAction(QIcon(":/images/bestfit.png"), "Zoom to best fit. Shortcut: 'Home'", this);
    QList<QKeySequence> bestFitShortcuts = QList<QKeySequence>() << Qt::Key_Home;
    bestFitAction->setShortcuts(bestFitShortcuts);

    zoomInAction = new QAction(QIcon(":/images/zoomin.png"), "Zoom in. Shortcut: 'MouseWheelUp' or 'Shift+UpArrow' or 'Ctrl+UpArrow' or '+'", this);
    QList<QKeySequence> zoomInShortcuts = QList<QKeySequence>() << Qt::Key_Plus << Qt::Key_Equal << (Qt::SHIFT | Qt::Key_Up) << (Qt::CTRL | Qt::Key_Up);
    zoomInAction->setShortcuts(zoomInShortcuts);

    zoomOutAction = new QAction(QIcon(":/images/zoomout.png"), "Zoom out. Shortcut: 'MouseWheelDown' or 'Shift+DownArrow' or 'Ctrl+DownArrow' or '-'", this);
    QList<QKeySequence> zoomOutShortcuts = QList<QKeySequence>() << Qt::Key_Minus << Qt::Key_Underscore << (Qt::SHIFT | Qt::Key_Down) << (Qt::CTRL | Qt::Key_Down);
    zoomOutAction->setShortcuts(zoomOutShortcuts);

    scrollUpAction = new QAction(QIcon(":/images/uparrow.png"), "Scroll up. Shortcut: 'Shift+MouseWheelUp' or 'UpArrow' or 'PageUp'", this);
    QList<QKeySequence> scrollUpShortcuts = QList<QKeySequence>() << Qt::Key_Up << Qt::Key_PageUp;
    scrollUpAction->setShortcuts(scrollUpShortcuts);

    scrollDownAction = new QAction(QIcon(":/images/downarrow.png"), "Scroll down. Shortcut: 'Shift+MouseWheelUp' or 'DownArrow' or 'PageDown'", this);
    QList<QKeySequence> scrollDownShortcuts = QList<QKeySequence>() << Qt::Key_Down << Qt::Key_PageDown;
    scrollDownAction->setShortcuts(scrollDownShortcuts);

    scrollLeftAction = new QAction(QIcon(":/images/leftarrow.png"), "Scroll left. Shortcut: 'Ctrl+MouseWheelDown' or 'LeftArrow'", this);
    QList<QKeySequence> scrollLeftShortcuts = QList<QKeySequence>() << Qt::Key_Left;
    scrollLeftAction->setShortcuts(scrollLeftShortcuts);

    scrollRightAction = new QAction(QIcon(":/images/rightarrow.png"), "Scroll right. Shortcut: 'Ctrl+MouseWheelUp' or 'RightArrow'", this);
    QList<QKeySequence> scrollRightShortcuts = QList<QKeySequence>() << Qt::Key_Right;
    scrollRightAction->setShortcuts(scrollRightShortcuts);

    defaultViewAction = new QAction(QIcon(":/images/default.png"), "Default view", this);
    defaultViewAction->setCheckable(true);

    impedanceViewAction = new QAction(QIcon(":/images/omega.png"), "Impedance view", this);
    impedanceViewAction->setCheckable(true);

    spikeViewAction = new QAction(QIcon(":/images/spike.png"), "Spike view", this);
    spikeViewAction->setCheckable(true);

    viewActions = new QActionGroup(this);
    viewActions->addAction(defaultViewAction);
    viewActions->addAction(impedanceViewAction);
    viewActions->addAction(spikeViewAction);
    defaultViewAction->setChecked(true);

    pageTabWidget = new QTabWidget(this);
    pageTabWidget->setTabPosition(QTabWidget::South);
    pageTabWidget->setTabShape(QTabWidget::Triangular);
    pageTabWidget->setAcceptDrops(true);

    statusCoords = new QLabel(" ", this);
    statusSiteInfo = new QLabel(" ", this);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(statusCoords);
    statusLayout->addWidget(statusSiteInfo);

    QFrame *statusRow = new QFrame(this);
    statusRow->setLayout(statusLayout);

    QVBoxLayout *viewLayout = new QVBoxLayout();
    viewLayout->addWidget(pageTabWidget);
    viewLayout->addWidget(statusRow);

    QFrame *view = new QFrame(this);
    view->setLayout(viewLayout);

    toolBar = new QToolBar(this);
    setContextMenuPolicy(Qt::NoContextMenu);

    toolBar->addAction(bestFitAction);
    toolBar->addAction(zoomInAction);
    toolBar->addAction(zoomOutAction);
    toolBar->addSeparator();

    toolBar->addActions(mouseActions->actions());
    toolBar->addSeparator();

    toolBar->addAction(scrollUpAction);
    toolBar->addAction(scrollDownAction);
    toolBar->addAction(scrollLeftAction);
    toolBar->addAction(scrollRightAction);
    toolBar->addSeparator();

    toolBar->addAction(defaultViewAction);
    toolBar->addAction(impedanceViewAction);
    toolBar->addAction(spikeViewAction);

    addToolBar(Qt::RightToolBarArea, toolBar);

    impedanceGradient = new ImpedanceGradient();
    impedanceGradient->hide();

    spikeGradient = new SpikeGradient(state, this);
    spikeGradient->hide();

    spikeDecayLabel = new QLabel(tr("Display decay time:"), this);
    spikeDecayLabel->hide();

    decayOptions[0] = 0.5;
    decayOptions[1] = 1.0;
    decayOptions[2] = 2.0;
    decayOptions[3] = 5.0;
    decayOptions[4] = 10.0;

    spikeDecayComboBox = new QComboBox(this);
    for (int i = 0; i < 5; i++) {
        spikeDecayComboBox->addItem(QString::number(decayOptions[i]) + " s");
    }
    spikeDecayComboBox->setCurrentIndex(1);
    spikeDecayComboBox->hide();

    connect(spikeDecayComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeSpikeDecay()));

    QVBoxLayout *mainLayout = new QVBoxLayout();

    QHBoxLayout *spikeDecayRow = new QHBoxLayout();
    spikeDecayRow->addStretch();
    spikeDecayRow->addWidget(spikeDecayLabel);
    spikeDecayRow->addWidget(spikeDecayComboBox);
    spikeDecayRow->addStretch();

    mainLayout->addWidget(view);
    mainLayout->addWidget(impedanceGradient);
    mainLayout->setAlignment(impedanceGradient, Qt::AlignCenter);
    mainLayout->addLayout(spikeDecayRow);
    mainLayout->addWidget(spikeGradient);
    mainLayout->setAlignment(spikeGradient, Qt::AlignCenter);

    QWidget *central = new QWidget(this);
    central->setLayout(mainLayout);
    setCentralWidget(central);

    probeMapSettingsInterface = new XMLInterface(state, controllerInterface, XMLIncludeProbeMapSettings);

    connect(loadAction, SIGNAL(triggered()), this, SLOT(load()));
    connect(pageTabWidget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));

    connect(resizeAction, SIGNAL(toggled(bool)), this, SLOT(changeMode()));
    connect(scrollAction, SIGNAL(toggled(bool)), this, SLOT(changeMode()));
    connect(selectAction, SIGNAL(toggled(bool)), this, SLOT(changeMode()));

    connect(viewActions, SIGNAL(triggered(QAction*)), this, SLOT(toggleView()));

    enableActions(false); // Default to greying-out actions until a valid Probe Map XML file is loaded.

    // Locate default position to center of parent window.
    move(parent->x() + (parent->width()/2 - sizeHint().width()/2), parent->y() + (parent->height()/2 - sizeHint().height()/2));

    if (state->running) updateForRun();
    else updateForStop();

    spikeTimer.start();
}

ProbeMapWindow::~ProbeMapWindow()
{
    delete impedanceGradient;
    delete spikeGradient;
    delete probeMapSettingsInterface;
}

QSize ProbeMapWindow::sizeHint() const
{
    return QSize(600, 600);
}

void ProbeMapWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        // Only accept events with a single local file url
        if (event->mimeData()->urls().size() == 1) {
            if (event->mimeData()->urls().at(0).isLocalFile())
                event->accept();
        }
    }
}

void ProbeMapWindow::dropEvent(QDropEvent *event)
{
    QSettings settings;
    QString filename = event->mimeData()->urls().at(0).toLocalFile();

    QString errorMessage;
    bool loadSuccess = probeMapSettingsInterface->loadFile(filename, errorMessage, false, true); // Specify that this XML parsing is probe-map specific.

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading from XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }

    // Once a load has successfully completed, enable QActions
    enableActions(true);

    QFileInfo fileInfo(filename);
    settings.setValue("probeMapDirectory", fileInfo.absolutePath());

    // Temporarily suppress 'currentChanged' signal so that the current page can be initialized.
    disconnect(pageTabWidget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));
    currentPage = -1;
    clearTabWidget();
    populateTabWidget();

    // Reactive 'currentChanged' signal.
    connect(pageTabWidget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));

    // Initialize current page to the first page.
    pageTabWidget->setCurrentIndex(0);
    pageChanged(0);

    update();
}

void ProbeMapWindow::updateFromState()
{
    linkAndUpdateSites();
    if (pageTabWidget->count() > 0)
        pageTabWidget->currentWidget()->update();
}

void ProbeMapWindow::updateForRun()
{
    loadAction->setEnabled(false);
}

void ProbeMapWindow::updateForLoad()
{
    updateForRun();
}

void ProbeMapWindow::updateForStop()
{
    loadAction->setEnabled(true);
}

void ProbeMapWindow::catchSpikeReport(QString names)
{
    // Separate QString out using ','
    QStringList nameList = names.split(',', Qt::SkipEmptyParts);

    // Insert or assign each name within nameList
    for (QStringList::const_iterator it = nameList.constBegin(); it != nameList.constEnd(); ++it) {
        activeChannels[(*it).toStdString()] = 0.0;
    }
}

void ProbeMapWindow::catchSpikeTimerTick()
{
    // For now, every 0.1 second (100 milliseconds), advance
    if (spikeTimer.elapsed() >= 100) {
        for (ChannelList::const_iterator p = activeChannels.begin(); p != activeChannels.end() ; ) {
            if (p->second >= state->getDecayTime() - 0.1) {
                QVector<ElectrodeSite*> sitesWithName = getSitesWithName(QString::fromStdString(p->first));
                for (int i = 0; i < sitesWithName.size(); i++) {
                    ElectrodeSite* thisSite = sitesWithName[i];
                    thisSite->spikeTime = state->getDecayTime();
                }
                activeChannels.erase(p++);
            } else {
                activeChannels[p->first] = p->second + 0.1;
                ++p;
            }
        }
        spikeTimer.restart();
    }
    updateSpikeTimerSites();

    // Guarantee an update to repaint the current page
    for (int page = 0; page < pageTabWidget->count(); ++page) {
        pageTabWidget->widget(page)->update();
    }
}

void ProbeMapWindow::changeSpikeDecay()
{
    state->setDecayTime(decayOptions[spikeDecayComboBox->currentIndex()]);

    // Update the spikeGradient widget
    spikeGradient->update();
}

void ProbeMapWindow::updateSpikeTimerSites()
{
    // Go through all active channels, linking their channel name to all ElectrodeSites.
    for (ChannelList::const_iterator p = activeChannels.begin(); p != activeChannels.end(); p++) {
        QVector<ElectrodeSite*> sitesWithName = getSitesWithName(QString::fromStdString(p->first));
        for (int i = 0; i < sitesWithName.size(); i++) {
            ElectrodeSite* thisSite = sitesWithName[i];
            thisSite->spikeTime = p->second;
        }
    }
}

// Catch signal from PageView signifying that the mouse status should be updated. Update status to reflect (x,y) coordinates and info of any hovered sites.
void ProbeMapWindow::updateMouseStatus(float x, float y, bool mousePresent, QString hoveredSiteName)
{
    // If mouse is present, display (x,y) coordinates and, if a site is hovered, that site's info.
    if (mousePresent) {
        QString coordText = "(x,y): " + QString::number(x) + "," + QString::number(y);
        statusCoords->setText(coordText);

        // If a site is hovered, display that site's name and information.
        if (hoveredSiteName.length() > 0) {

            // A site was hovered. Determine the second line of text to display (based on the state of the hovered site).
            QVector<ElectrodeSite*> sitesWithName = getSitesWithName(hoveredSiteName);
            if (sitesWithName.length() == 0) {
                return;
            }
            ElectrodeSite* thisSite = getSitesWithName(hoveredSiteName).at(0);

            // If the site is unlinked, display 'Unlinked'.
            if (!thisSite->linked) {
                statusSiteInfo->setText(hoveredSiteName + " - Unlinked");
            } else if (!thisSite->enabled) {
                // If the site is disabled, display 'Disabled'.
                statusSiteInfo->setText(hoveredSiteName + " - Disabled");
            } else {
                // If the site is linked and enabled, display the impedance magnitude and phase.
                QString unitPrefix;
                int precision;
                double scale;
                if (thisSite->impedanceMag >= 1.0e6) {
                    scale = 1.0e6;
                    unitPrefix = "M";
                } else {
                    scale = 1.0e3;
                    unitPrefix = "k";
                }

                if (thisSite->impedanceMag >= 100.0e6) {
                    precision = 0;
                } else if (thisSite->impedanceMag >= 10.0e6) {
                    precision = 1;
                } else if (thisSite->impedanceMag >= 1.0e6) {
                    precision = 2;
                } else if (thisSite->impedanceMag >= 100.0e3) {
                    precision = 0;
                } else if (thisSite->impedanceMag >= 10.0e3) {
                    precision = 1;
                } else {
                    precision = 2;
                }

                QString impedanceText = QString::number(thisSite->impedanceMag / scale, 'f', precision) +
                        " " + unitPrefix + OmegaSymbol + " " + AngleSymbol +
                        QString::number(thisSite->impedancePhase, 'f', 0) + DegreeSymbol;

                statusSiteInfo->setText(hoveredSiteName + " - " + impedanceText);
            }
        } else {
            // If no site is hovered, clear the site info status label.
            statusSiteInfo->setText(" ");
        }
    } else {
        // If mouse is not present, don't display anything.
        // Set status labels to empty.
        statusCoords->setText(" ");
        statusSiteInfo->setText(" ");
    }
}

// Catch signal from ControlWindow signifying that a channel should be enabled. Update 'enabled' member of all ElectrodeSites with the given nativeName.
void ProbeMapWindow::enableChannel(QString nativeName, bool enabled)
{
    QVector<ElectrodeSite*> sites = getSitesWithName(nativeName);

    // Update data structure with enable info.
    for (int site = 0; site < sites.size(); site++) {
        sites[site]->enabled = enabled;
    }

    activateWindow();
}

// Catch signal from ControlWindow signifying that a channel's color should be changed. Update 'color' member of all ElectrodeSites with the given nativeName.
void ProbeMapWindow::changeChannelColor(QString nativeName, QString color)
{
    QVector<ElectrodeSite*> sites = getSitesWithName(nativeName);

    // Update data structure with color info.
    for (int site = 0; site < sites.size(); site++) {
        sites[site]->color = color;
    }

    activateWindow();
}

// Catch signal from ControlWindow signifying that a channel's impedance should be changed.
// Update 'impedanceMag' and 'impedancePhase' members of all ElectrodeSites with the given nativeName.
void ProbeMapWindow::changeChannelImpedance(QString nativeName, float impedanceMag, float impedancePhase)
{
    QVector<ElectrodeSite*> sites = getSitesWithName(nativeName);

    // Update data structure with impedance info.
    for (int site = 0; site < sites.size(); site++) {
        sites[site]->impedanceValid = true;
        sites[site]->impedanceMag = impedanceMag;
        sites[site]->impedancePhase = impedancePhase;
    }

    activateWindow();
}

// Re-link all sites and update them from state.
void ProbeMapWindow::linkAndUpdateSites()
{
    // Go through each amplifier channel in state.
    std::vector<std::string> amplifierChannelNames = state->signalSources->amplifierChannelsNameList();

    for (unsigned int channel = 0; channel < amplifierChannelNames.size(); channel++) {
        // Find all sites with this name.
        QVector<ElectrodeSite*> thisChannelSites = getSitesWithName(QString::fromStdString(amplifierChannelNames.at(channel)));
        // Go through all sites for this channel, matching its attributes to the corresponding SignalChannel.
        for (int site = 0; site < thisChannelSites.size(); site++) {
            Channel* thisChannel = state->signalSources->channelByName(amplifierChannelNames.at(channel));
            thisChannelSites[site]->linked = true;
            thisChannelSites[site]->color = thisChannel->getColor().name();
            thisChannelSites[site]->enabled = thisChannel->isEnabled();
            thisChannelSites[site]->highlighted = thisChannel->isSelected();
            thisChannelSites[site]->impedanceValid = thisChannel->isImpedanceValid();
            thisChannelSites[site]->impedanceMag = thisChannel->getImpedanceMagnitude();
            thisChannelSites[site]->impedancePhase = thisChannel->getImpedancePhase();
        }
    }
}

void ProbeMapWindow::enableActions(bool enable)
{
    resizeAction->setEnabled(enable);
    selectAction->setEnabled(enable);
    scrollAction->setEnabled(enable);
    bestFitAction->setEnabled(enable);
    zoomInAction->setEnabled(enable);
    zoomOutAction->setEnabled(enable);
    scrollUpAction->setEnabled(enable);
    scrollDownAction->setEnabled(enable);
    scrollLeftAction->setEnabled(enable);
    scrollRightAction->setEnabled(enable);
    defaultViewAction->setEnabled(enable);
    impedanceViewAction->setEnabled(enable);
    spikeViewAction->setEnabled(enable);
}

// Determine how many pages need to be displayed, create this many new PageViews, and populate pageTabWidget with these PageViews.
void ProbeMapWindow::populateTabWidget()
{
    for (int page = 0; page < state->probeMapSettings.pages.size(); page++) {
        ViewMode viewMode = DefaultView;
        if (impedanceViewAction->isChecked()) viewMode = ImpedanceView;
        else if (spikeViewAction->isChecked()) viewMode = SpikeView;
        PageView *singlePage = new PageView(state, page, viewMode, this);
        //PageView *singlePage = new PageView(state->probeMapSettings.pages[page], state->probeMapSettings.backgroundColor, state->probeMapSettings.lineColor, state->probeMapSettings.siteOutlineColor, state->probeMapSettings.fontHeight, state->probeMapSettings.fontColor, state->probeMapSettings.textAlignment, state->probeMapSettings.siteShape, state->probeMapSettings.siteWidth, state->probeMapSettings.siteHeight, viewMode, this);
        pageTabWidget->addTab(singlePage, state->probeMapSettings.pages[page].name);
        pageTabWidget->setCurrentIndex(page);
        singlePage->bestFit();
        //connect(singlePage, SIGNAL(stateChanged()), this, SIGNAL(stateChanged()));
    }
    linkAndUpdateSites();
}

// Delete all PageViews within pageTabWidget, and clear pageTabWidget.
void ProbeMapWindow::clearTabWidget() {
    for (int page = 0; page < pageTabWidget->count(); page++) {
        delete pageTabWidget->widget(page);
    }
    pageTabWidget->clear();
}

// Return references to all sites with the given nativeName.
QVector<ElectrodeSite*> ProbeMapWindow::getSitesWithName(const QString& nativeName)
{
    QString name = nativeName.left(1);
    int channelNum = QStringView{nativeName}.right(nativeName.length() - 2).toInt();
    QVector<ElectrodeSite*> sites;

    for (int pageIndex = 0; pageIndex < state->probeMapSettings.pages.size(); pageIndex++) {
        for (int portIndex = 0; portIndex < state->probeMapSettings.pages[pageIndex].ports.size(); portIndex++) {
            if (state->probeMapSettings.pages[pageIndex].ports[portIndex].name != name) {
                continue;
            }
            for (int siteIndex = 0; siteIndex < state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites.size(); siteIndex++) {
                if (state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites[siteIndex].channelNumber == channelNum) {
                    sites.append(&state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites[siteIndex]);
                }
            }
        }
    }
    return sites;
}

// Slot to catch signal from loadAction. Read XML file, attempts to link sites to ControlWindow, populates pageTabWidget, and initializes display on the first PageView.
void ProbeMapWindow::load()
{
    QSettings settings;
    QString defaultDirectory = settings.value("probeMapDirectory", ".").toString();
    QString filename = QFileDialog::getOpenFileName(this, "Select Probe Description File", defaultDirectory, "XML files (*.xml)");

    // If user clicked 'cancel', then abort.
    if (filename.isEmpty()) return;

    QString errorMessage;
    bool loadSuccess = probeMapSettingsInterface->loadFile(filename, errorMessage, false, true); // Specify that this XML parsing is probe-map specific.

    if (!loadSuccess) {
        QMessageBox::critical(this, tr("Error: Loading From XML"), errorMessage);
        return;
    } else if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("Warning: Loading from XML"), errorMessage);
    }

    // Once a load has successfully completed, enable QActions.
    enableActions(true);

    QFileInfo fileInfo(filename);
    settings.setValue("probeMapDirectory", fileInfo.absolutePath());

    // Temporarily suppress 'currentChanged' signal so that the current page can be initialized.
    disconnect(pageTabWidget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));
    currentPage = -1;
    clearTabWidget();
    populateTabWidget();

    // Reactive 'currentChanged' signal.
    connect(pageTabWidget, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));

    // Initialize current page to the first page.
    pageTabWidget->setCurrentIndex(0);
    pageChanged(0);

    update();
    setWindowTitle(tr("Probe Map") + " (" + fileInfo.fileName() + ")");
}

// Catch signal from pageTabWidget. When the visible page in pageTabWidget is changed,
// disconnect signals/slots from previous page, and connect them to new page.
void ProbeMapWindow::pageChanged(int index)
{
    // If current page is not -1 (indicating that the pageTabWidget has already been initialized),
    // disconnect signals/slots from previous page.
    if (currentPage > -1) {
        PageView *previousPage = ((PageView*)pageTabWidget->widget(currentPage));
        disconnect(bestFitAction, SIGNAL(triggered()), previousPage, SLOT(bestFit()));
        disconnect(scrollUpAction, SIGNAL(triggered()), previousPage, SLOT(scrollUp()));
        disconnect(scrollDownAction, SIGNAL(triggered()), previousPage, SLOT(scrollDown()));
        disconnect(scrollLeftAction, SIGNAL(triggered()), previousPage, SLOT(scrollLeft()));
        disconnect(scrollRightAction, SIGNAL(triggered()), previousPage, SLOT(scrollRight()));
        disconnect(zoomInAction, SIGNAL(triggered()), previousPage, SLOT(zoomIn()));
        disconnect(zoomOutAction, SIGNAL(triggered()), previousPage, SLOT (zoomOut()));
        disconnect(this, SIGNAL(modeChanged(MouseMode)), previousPage, SLOT(changeMode(MouseMode)));
        disconnect(previousPage, SIGNAL(mouseMoved(float,float,bool,QString)), this, SLOT(updateMouseStatus(float,float,bool,QString)));
        disconnect(previousPage, SIGNAL(mouseExit()), this, SLOT(mouseLeft()));
        disconnect(previousPage, SIGNAL(resizeComplete()), this, SLOT(endOfResize()));
        disconnect(previousPage, SIGNAL(siteHighlighted(QString,bool)), this, SLOT(highlightSiteFromMap(QString,bool)));
        disconnect(previousPage, SIGNAL(deselectAllSites()), this, SLOT(deselectAllSitesFromMap()));
    }

    // Connect signals/slots to new page.
    currentPage = index;
    PageView *thisPage = ((PageView*)pageTabWidget->widget(currentPage));
    connect(bestFitAction, SIGNAL(triggered()), thisPage, SLOT(bestFit()));
    connect(scrollUpAction, SIGNAL(triggered()), thisPage, SLOT(scrollUp()));
    connect(scrollDownAction, SIGNAL(triggered()), thisPage, SLOT(scrollDown()));
    connect(scrollLeftAction, SIGNAL(triggered()), thisPage, SLOT(scrollLeft()));
    connect(scrollRightAction, SIGNAL(triggered()), thisPage, SLOT(scrollRight()));
    connect(zoomInAction, SIGNAL(triggered()), thisPage, SLOT(zoomIn()));
    connect(zoomOutAction, SIGNAL(triggered()), thisPage, SLOT(zoomOut()));
    connect(this, SIGNAL(modeChanged(MouseMode)), thisPage, SLOT(changeMode(MouseMode)));
    connect(thisPage, SIGNAL(mouseMoved(float,float,bool,QString)), this, SLOT(updateMouseStatus(float,float,bool,QString)));
    connect(thisPage, SIGNAL(mouseExit()), this, SLOT(mouseLeft()));
    connect(thisPage, SIGNAL(resizeComplete()), this, SLOT(endOfResize()));
    connect(thisPage, SIGNAL(siteHighlighted(QString,bool)), this, SLOT(highlightSiteFromMap(QString,bool)));
    connect(thisPage, SIGNAL(deselectAllSites()), this, SLOT(deselectAllSitesFromMap()));

    statusCoords->setFixedHeight(statusCoords->height());
    statusSiteInfo->setFixedHeight(statusSiteInfo->height());
}

// Catch signal from PageView signifying that the mouse has left the PageView. Clear 'status' QLabels.
void ProbeMapWindow::mouseLeft()
{
    statusCoords->setText(" ");
    statusSiteInfo->setText(" ");
}

// Catch signals indicating that the mouse mode has changed. Emit 'modeChanged' signal that gets caught by PageView.
void ProbeMapWindow::changeMode()
{
    if (selectAction->isChecked())
        emit modeChanged(Select);
    else if (scrollAction->isChecked())
        emit modeChanged(Scroll);
    else if (resizeAction->isChecked())
        emit modeChanged(Resize);
}

// Slot to catch signal indicating that a resize has been finished. Return MouseMode to 'Select'.
void ProbeMapWindow::endOfResize()
{
    selectAction->setChecked(true);
}

// Slot to catch a signal from PageView signifying that a site has been highlighted. Use state's intelligent functions to change state.
void ProbeMapWindow::highlightSiteFromMap(QString nativeName, bool highlighted)
{
    state->signalSources->setSelected(nativeName, highlighted);
    state->forceUpdate();
}

// Catch a signal from Pageview signifying that all sites should be deselected. Use state's intelligent functions to change state.
void ProbeMapWindow::deselectAllSitesFromMap()
{
    for (int pageIndex = 0; pageIndex < state->probeMapSettings.pages.size(); pageIndex++) {
        for (int portIndex = 0; portIndex < state->probeMapSettings.pages[pageIndex].ports.size(); portIndex++) {
            for (int siteIndex = 0; siteIndex < state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites.size(); siteIndex++) {
                if (state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites[siteIndex].linked) {
                    state->signalSources->setSelected(state->probeMapSettings.pages[pageIndex].ports[portIndex].electrodeSites[siteIndex].nativeName, false);
                }
            }
        }
    }
    state->forceUpdate();
}

void ProbeMapWindow::toggleView()
{
    ViewMode viewMode;
    if (impedanceViewAction->isChecked()) {
        viewMode = ImpedanceView;
        impedanceGradient->show();
        spikeGradient->hide();
        spikeDecayLabel->hide();
        spikeDecayComboBox->hide();
        state->setReportSpikes(false);
    } else if (spikeViewAction->isChecked()) {
        viewMode = SpikeView;
        impedanceGradient->hide();
        spikeGradient->show();
        spikeDecayLabel->show();
        spikeDecayComboBox->show();
        state->setReportSpikes(true);
    } else {
        viewMode = DefaultView;
        impedanceGradient->hide();
        spikeGradient->hide();
        spikeDecayLabel->hide();
        spikeDecayComboBox->hide();
        state->setReportSpikes(false);
    }

    for (int page = 0; page < pageTabWidget->count(); ++page) {
        ((PageView*) pageTabWidget->widget(page))->changeView(viewMode);
        pageTabWidget->widget(page)->update();
    }

    update();
}
