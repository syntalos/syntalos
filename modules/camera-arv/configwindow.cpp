/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2015 Jure Varlec <jure.varlec@ad-vega.si>
                            Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qarv/qarv-globals.h"
#include "configwindow.h"
#include "qarv/qarvcameradelegate.h"
#include "getmtu_linux.h"
#include "qarv/decoders/unsupported.h"

#include <QNetworkInterface>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <qdatetime.h>
#include <QProcess>
#include <QTextDocument>
#include <QStatusBar>
#include <QtConcurrentRun>
#include <QPluginLoader>
#include <QMenu>
#include <QToolButton>
#include <QLineEdit>
#include <QTimeEdit>

Q_DECLARE_METATYPE(cv::Mat)

using namespace QArv;

ArvConfigWindow::ArvConfigWindow(const QString &modId, QWidget *parent)
    : QMainWindow(parent),
      m_modId(modId),
      camera(nullptr),
      decoder(nullptr),
      playing(false),
      started(false)
{
    setupUi(this);
    on_statusTimeoutSpinbox_valueChanged(statusTimeoutSpinbox->value());
    m_debugConnection = connect(
        &QArvDebug::messageSender, &MessageSender::newDebugMessage, [this](const QString &scope, const QString &msg) {
            if (scope == m_modId || scope.isEmpty())
                messageList->appendPlainText(msg);
        });

    // Setup theme icons if available.
    QHash<QAbstractButton *, QString> icons;
    icons[unzoomButton] = "zoom-original";
    icons[playButton] = "media-playback-start";
    icons[refreshCamerasButton] = "view-refresh";
    icons[editGainButton] = "edit-clear-locationbar-rtl";
    icons[editExposureButton] = "edit-clear-locationbar-rtl";
    icons[pickROIButton] = "edit-select";
    QHash<QAction *, QString> aicons;
    aicons[showVideoAction] = "video-display";
    aicons[messageAction] = "dialog-information";

    auto plugins = QPluginLoader::staticInstances();

    autoreadexposure = new QTimer(this);
    autoreadexposure->setInterval(sliderUpdateSpinbox->value());
    this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(readExposure()));
    this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(readGain()));
    this->connect(autoreadexposure, SIGNAL(timeout()), SLOT(updateBandwidthEstimation()));

    videoWidget->connect(pickROIButton, SIGNAL(toggled(bool)), SLOT(enableSelection(bool)));
    this->connect(videoWidget, SIGNAL(selectionComplete(QRect)), SLOT(pickedROI(QRect)));

    rotationSelector->addItem(tr("No rotation"), 0);
    rotationSelector->addItem(tr("90 degrees"), 90);
    rotationSelector->addItem(tr("180 degrees"), 180);
    rotationSelector->addItem(tr("270 degrees"), 270);
    this->connect(rotationSelector, SIGNAL(currentIndexChanged(int)), SLOT(updateImageTransform()));
    this->connect(invertColors, SIGNAL(stateChanged(int)), SLOT(updateImageTransform()));
    this->connect(flipHorizontal, SIGNAL(stateChanged(int)), SLOT(updateImageTransform()));
    this->connect(flipVertical, SIGNAL(stateChanged(int)), SLOT(updateImageTransform()));

    setupListOfSavedWidgets();
    updateImageTransform();

    QTimer::singleShot(0, this, SLOT(on_refreshCamerasButton_clicked()));
}

ArvConfigWindow::~ArvConfigWindow()
{
    toggleVideoPreview(false);
    camera.reset();
    disconnect(m_debugConnection);
}

void ArvConfigWindow::on_refreshCamerasButton_clicked(bool clicked)
{
    cameraSelector->blockSignals(true);
    cameraSelector->clear();
    cameraSelector->setEnabled(false);
    cameraSelector->addItem(tr("Looking for cameras..."));
    QApplication::processEvents();
    cameraSelector->clear();
    auto cameras = QArvCamera::listCameras();
    foreach (auto cam, cameras) {
        QString display;
        display = display + cam.vendor + " (" + cam.model + ")";
        cameraSelector->addItem(display, QVariant::fromValue<QArvCameraId>(cam));
    }
    cameraSelector->setCurrentIndex(-1);
    cameraSelector->setEnabled(true);
    cameraSelector->blockSignals(false);
    QString message = tr("Found %n cameras.", "Number of cameras", cameraSelector->count());
    statusBar()->showMessage(statusBar()->currentMessage() + " " + message, statusTimeoutMsec);
    logMessage() << message;
}

void ArvConfigWindow::on_unzoomButton_toggled(bool checked)
{
    if (checked) {
        oldstate = saveState();
        oldgeometry = saveGeometry();
        oldsize = videoWidget->size();
        QSize newsize = videoWidget->getImageSize();
        videoWidget->setFixedSize(newsize);
    } else {
        videoWidget->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        restoreState(oldstate);
        restoreGeometry(oldgeometry);
        videoWidget->setFixedSize(oldsize);
        videodock->resize(1, 1);
        QApplication::processEvents();
        videoWidget->setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        videoWidget->setMinimumSize(QSize(64, 64));
        QApplication::processEvents();
        on_videodock_topLevelChanged(videodock->isFloating());
    }
}

static inline double slider2value(int slidervalue, QPair<double, double> &range)
{
    return range.first + (range.second - range.first) * slidervalue / slidersteps;
}

static inline int value2slider(double value, QPair<double, double> &range)
{
    return (value - range.first) / (range.second - range.first) * slidersteps;
}

void ArvConfigWindow::readROILimits()
{
    auto wBounds = camera->getROIWidthBounds();
    auto hBounds = camera->getROIHeightBounds();
    roirange = QRect(QPoint(0, 0), QSize(wBounds.second, hBounds.second));
    xSpinbox->setRange(0, wBounds.second);
    ySpinbox->setRange(0, hBounds.second);
    wSpinbox->setRange(wBounds.first, wBounds.second);
    hSpinbox->setRange(hBounds.first, hBounds.second);
}

void ArvConfigWindow::readAllValues()
{
    fpsSpinbox->setValue(camera->getFPS());

    auto formats = camera->getPixelFormats();
    auto formatnames = camera->getPixelFormatNames();
    int noofframes = formats.length();
    pixelFormatSelector->blockSignals(true);
    pixelFormatSelector->clear();
    for (int i = 0; i < noofframes; i++)
        pixelFormatSelector->addItem(formatnames.at(i), formats.at(i));
    auto format = camera->getPixelFormat();
    pixelFormatSelector->setCurrentIndex(pixelFormatSelector->findData(format));
    pixelFormatSelector->setEnabled(noofframes > 1 && !started);
    pixelFormatSelector->blockSignals(false);

    QSize binsize = camera->getBinning();
    binSpinBox->setValue(binsize.width());

    gainrange = camera->getGainBounds();
    exposurerange = camera->getExposureBounds();
    gainSlider->setRange(0, slidersteps);
    exposureSlider->setRange(0, slidersteps);
    gainSpinbox->setRange(gainrange.first, gainrange.second);
    exposureSpinbox->setRange(exposurerange.first / 1000., exposurerange.second / 1000.);
    readGain();
    readExposure();
    gainAutoButton->setEnabled(camera->hasAutoGain());
    exposureAutoButton->setEnabled(camera->hasAutoExposure());

    readROILimits();
    QRect roi = camera->getROI();
    xSpinbox->setValue(roi.x());
    ySpinbox->setValue(roi.y());
    wSpinbox->setValue(roi.width());
    hSpinbox->setValue(roi.height());
}

void ArvConfigWindow::on_cameraSelector_currentIndexChanged(int index)
{
    autoreadexposure->stop();

    auto camid = cameraSelector->itemData(index).value<QArvCameraId>();
    if (camera) {
        toggleVideoPreview(false);
        camera.reset();
    }
    try {
        camera = std::make_shared<QArvCamera>(camid, m_modId);
    } catch (const std::exception &e) {
        logMessage() << "Failed to reference camera:" << e.what();
        cameraSelector->setCurrentIndex(-1);
        return;
    }
    connect(camera.get(), &QArvCamera::frameReady, this, &ArvConfigWindow::previewFrameReceived);
    connect(camera.get(), &QArvCamera::bufferUnderrun, this, &ArvConfigWindow::bufferUnderrunOccured);

    logMessage() << "Pixel formats:" << camera->getPixelFormats();

    auto ifaceIP = camera->getHostIP();
    QNetworkInterface cameraIface;
    if (!ifaceIP.isNull()) {
        auto ifaces = QNetworkInterface::allInterfaces();
        bool process_loop = true;
        foreach (QNetworkInterface iface, ifaces) {
            if (!process_loop)
                break;
            auto addresses = iface.addressEntries();
            foreach (QNetworkAddressEntry addr, addresses) {
                if (addr.ip() == ifaceIP) {
                    cameraIface = iface;
                    process_loop = false;
                    break;
                }
            }
        }

        if (cameraIface.isValid()) {
            int mtu = getMTU(cameraIface.name());
            camera->setMTU(mtu);
        }
    } else {
        QString message = tr(
            "Network address not found, "
            "trying best-effort MTU %1.");
        int mtu = 1500;
        message = message.arg(mtu);
        statusBar()->showMessage(message, statusTimeoutMsec);
        logMessage() << message;
        camera->setMTU(mtu);
    }

    if (camera->getMTU() == 0)
        cameraMTUDescription->setText(tr("Not an ethernet camera."));
    else {
        int mtu = camera->getMTU();
        QString ifname = cameraIface.name();
        QString description = tr("Camera is on interface %1,\nMTU set to %2.");
        description = description.arg(ifname);
        description = description.arg(QString::number(mtu));
        if (mtu < 3000)
            description += tr("\nConsider increasing the MTU!");
        cameraMTUDescription->setText(description);
    }

    camera->setAutoGain(false);
    camera->setAutoExposure(false);
    on_registerCacheCheck_stateChanged(registerCacheCheck->checkState());
    readAllValues();

    advancedTree->setModel(camera.get());
    advancedTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    advancedTree->setItemDelegate(new QArvCameraDelegate);

    autoreadexposure->start();
    this->connect(camera.get(), SIGNAL(dataChanged(QModelIndex, QModelIndex)), SLOT(readAllValues()));

    updateDecoder();
    Q_EMIT cameraSelected(camera, decoder);
}

void ArvConfigWindow::readExposure()
{
    bool blocked = exposureSlider->blockSignals(true);
    exposureSlider->setValue(value2slider_log(camera->getExposure(), exposurerange));
    exposureSlider->blockSignals(blocked);
    exposureSpinbox->setValue(camera->getExposure() / 1000.);
}

void ArvConfigWindow::readGain()
{
    bool blocked = gainSlider->blockSignals(true);
    gainSlider->setValue(value2slider(camera->getGain(), gainrange));
    gainSlider->blockSignals(blocked);
    gainSpinbox->setValue(camera->getGain());
}

void ArvConfigWindow::on_exposureSlider_valueChanged(int value)
{
    camera->setExposure(slider2value_log(value, exposurerange));
}

void ArvConfigWindow::on_gainSlider_valueChanged(int value)
{
    camera->setGain(slider2value(value, gainrange));
}

void ArvConfigWindow::on_exposureAutoButton_toggled(bool checked)
{
    exposureSlider->setEnabled(!checked);
    exposureSpinbox->setEnabled(!checked);
    camera->setAutoExposure(checked);
}

void ArvConfigWindow::on_gainAutoButton_toggled(bool checked)
{
    gainSlider->setEnabled(!checked);
    gainSpinbox->setEnabled(!checked);
    camera->setAutoGain(checked);
}

void ArvConfigWindow::on_pixelFormatSelector_currentIndexChanged(int index)
{
    auto format = pixelFormatSelector->itemData(index).toString();
    camera->setPixelFormat(format);

    updateDecoder();
    Q_EMIT cameraSelected(camera, decoder);
}

void ArvConfigWindow::on_applyROIButton_clicked(bool clicked)
{
    xSpinbox->setValue((xSpinbox->value() / 2) * 2);
    ySpinbox->setValue((ySpinbox->value() / 2) * 2);
    double tmp;
    tmp = (wSpinbox->value() / 2) * 2;
    tmp = tmp < 8 ? 8 : tmp;
    wSpinbox->setValue(tmp);
    tmp = (hSpinbox->value() / 2) * 2;
    tmp = tmp < 8 ? 8 : tmp;
    hSpinbox->setValue(tmp);
    QRect ROI(xSpinbox->value(), ySpinbox->value(), wSpinbox->value(), hSpinbox->value());

    {
        auto ROI2 = roirange.intersected(ROI);
        if (ROI2 != ROI)
            statusBar()->showMessage(tr("Region of interest too large, shrinking."), statusTimeoutMsec);
        ROI = ROI2;
        ROI.setX((ROI.x() / 2) * 2);
        ROI.setY((ROI.y() / 2) * 2);
        ROI.setWidth((ROI.width() / 2) * 2);
        ROI.setHeight((ROI.height() / 2) * 2);
    }

    bool tostart = started;
    toggleVideoPreview(false);
    camera->setROI(ROI);
    toggleVideoPreview(tostart);
}

void ArvConfigWindow::on_resetROIButton_clicked(bool clicked)
{
    auto hBounds = camera->getROIHeightBounds();
    auto wBounds = camera->getROIWidthBounds();
    xSpinbox->setValue(0);
    ySpinbox->setValue(0);
    wSpinbox->setValue(wBounds.second);
    hSpinbox->setValue(hBounds.second);
    on_applyROIButton_clicked(true);
}

void ArvConfigWindow::on_binSpinBox_valueChanged(int value)
{
    bool tostart = started;
    toggleVideoPreview(false);
    int bin = binSpinBox->value();
    camera->setBinning(QSize(bin, bin));
    toggleVideoPreview(tostart);
}

void ArvConfigWindow::updateDecoder()
{
    decoder.reset();
    decoder = std::shared_ptr<QArvDecoder>(QArvDecoder::makeDecoder(
        camera->getPixelFormatId(), camera->getROI().size(), useFastInterpolator->isChecked()));
    if (!decoder) {
        QString message = tr("Decoder for %1 doesn't exist!");
        message = message.arg(camera->getPixelFormat());
        logMessage() << message;
        statusBar()->showMessage(message, statusTimeoutMsec);
        decoder = std::make_shared<Unsupported>(camera->getPixelFormatId(), camera->getROI().size());
    }
}

void ArvConfigWindow::setCameraInUse(bool camInUse)
{
    QList<QWidget *> protectedWidgets = {cameraSelector, refreshCamerasButton, useFastInterpolator, fpsSpinbox};

    for (auto wgt : protectedWidgets)
        wgt->setEnabled(!camInUse);

    started = camInUse;
    if (!camInUse) {
        if (camera)
            camera->setFPS(fpsSpinbox->value());
    }
}

void ArvConfigWindow::setCameraInUseExternal(bool camInUse)
{
    playButton->setChecked(false);
    toggleVideoPreview(false);
    videoWidget->setImage();

    setCameraInUse(camInUse);
    rotationSelector->setEnabled(!camInUse);
    roiBox->setEnabled(!camInUse);
    playButton->setEnabled(!camInUse);
}

TransformParams *ArvConfigWindow::currentTransformParams()
{
    return &transformParams;
}

void ArvConfigWindow::toggleVideoPreview(bool start)
{
    if (!camera)
        return;

    setEnabled(false);
    if (start && !started) {
        updateDecoder();
        if (decoder) {
            setCameraInUse(true);

            Q_EMIT cameraSelected(camera, decoder);
            camera->setFrameQueueSize(20);

            // we only record with a low framerate for the preview
            m_realFps = camera->getFPS();
            camera->setFPS(10);
            pixelFormatSelector->setEnabled(false);
            camera->startAcquisition();
        }
    } else if (!start && started) {
        started = false;
        camera->stopAcquisition();
        camera->setFPS(m_realFps);
        decoder.reset();

        setCameraInUse(false);
        pixelFormatSelector->setEnabled(pixelFormatSelector->count() > 1 && !started);
    }
    setEnabled(true);
}

void ArvConfigWindow::on_playButton_toggled(bool checked)
{
    playing = checked;
    toggleVideoPreview(playing);
    playing = checked && started;
    playButton->setChecked(playing);
}

void ArvConfigWindow::on_fpsSpinbox_valueChanged(int value)
{
    camera->setFPS(value);
    fpsSpinbox->setValue(camera->getFPS());
}

void ArvConfigWindow::pickedROI(QRect roi)
{
    pickROIButton->setChecked(false);
    QRect current = camera->getROI();

    // Compensate for the transform of the image. The actual transform must
    // be calculated using the size of the actual image, so we get this size
    // from the camera.
    auto imagesize = camera->getROI().size();
    auto truexform = QImage::trueMatrix(transformParams.qtf, imagesize.width(), imagesize.height());
    roi = truexform.inverted().map(QRegion(roi)).boundingRect();

    xSpinbox->setValue(current.x() + roi.x());
    ySpinbox->setValue(current.y() + roi.y());
    wSpinbox->setValue(roi.width());
    hSpinbox->setValue(roi.height());
    on_applyROIButton_clicked(true);
}

void ArvConfigWindow::updateBandwidthEstimation()
{
    int bw = camera->getEstimatedBW();
    if (bw == 0) {
        bandwidthDescription->setText(tr("Not an ethernet camera."));
    } else {
        QString unit(" B/s");
        if (bw >= 1024) {
            bw /= 1024;
            unit = " kB/s";
        }
        if (bw >= 1024) {
            bw /= 1024;
            unit = " MB/s";
        }
        bandwidthDescription->setText(QString::number(bw) + unit);
    }
}

void ArvConfigWindow::updateImageTransform()
{
    transformParams.qtf.reset();
    transformParams.qtf.scale(flipHorizontal->isChecked() ? -1 : 1, flipVertical->isChecked() ? -1 : 1);
    int angle = rotationSelector->itemData(rotationSelector->currentIndex()).toInt();
    transformParams.qtf.rotate(angle);

    if (flipHorizontal->isChecked() && flipVertical->isChecked())
        transformParams.flip = -1;
    else if (flipHorizontal->isChecked() && !flipVertical->isChecked())
        transformParams.flip = 1;
    else if (!flipHorizontal->isChecked() && flipVertical->isChecked())
        transformParams.flip = 0;
    else if (!flipHorizontal->isChecked() && !flipVertical->isChecked())
        transformParams.flip = -100; // Magic value

    transformParams.rot = angle / 90;
    transformParams.invert = invertColors->isChecked();
}

void ArvConfigWindow::on_editExposureButton_clicked(bool checked)
{
    autoreadexposure->stop();
    exposureSpinbox->setReadOnly(false);
    exposureSpinbox->setFocus(Qt::OtherFocusReason);
    exposureSpinbox->selectAll();
}

void ArvConfigWindow::on_editGainButton_clicked(bool checked)
{
    autoreadexposure->stop();
    gainSpinbox->setReadOnly(false);
    gainSpinbox->setFocus(Qt::OtherFocusReason);
    gainSpinbox->selectAll();
}

void ArvConfigWindow::on_gainSpinbox_editingFinished()
{
    camera->setGain(gainSpinbox->value());
    gainSpinbox->setReadOnly(true);
    gainSpinbox->clearFocus();
    readGain();
    autoreadexposure->start();
}

void ArvConfigWindow::on_exposureSpinbox_editingFinished()
{
    camera->setExposure(exposureSpinbox->value() * 1000);
    exposureSpinbox->setReadOnly(true);
    exposureSpinbox->clearFocus();
    readExposure();
    autoreadexposure->start();
}

static void makeDockAWindow(QDockWidget *dock)
{
    // Currently disabled as it causes jerkyness when undocking.
    // dock->setWindowFlags(Qt::Window);
    // dock->show();
    return;
}

void ArvConfigWindow::on_showVideoAction_toggled(bool checked)
{
    videodock->setVisible(checked);
}

void ArvConfigWindow::on_videodock_visibilityChanged(bool visible)
{
    showVideoAction->blockSignals(true);
    showVideoAction->setChecked(!videodock->isHidden());
    showVideoAction->blockSignals(false);
}

void ArvConfigWindow::on_videodock_topLevelChanged(bool floating)
{
    if (floating)
        makeDockAWindow(videodock);
}

void ArvConfigWindow::on_messageAction_toggled(bool checked)
{
    messageDock->setVisible(checked);
}

void ArvConfigWindow::on_messageDock_visibilityChanged(bool visible)
{
    messageAction->blockSignals(true);
    messageAction->setChecked(!messageDock->isHidden());
    messageAction->blockSignals(false);
}

void ArvConfigWindow::on_messageDock_topLevelChanged(bool floating)
{
    if (floating)
        makeDockAWindow(messageDock);
}

void ArvConfigWindow::on_ROIsizeCombo_newSizeSelected(QSize size)
{
    videoWidget->setSelectionSize(size);
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold, QObject *parent)
    : QObject(parent),
      size_threshold(size_threshold)
{
}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt)
{
    if (evt->type() == QEvent::ToolTipChange) {
        QWidget *widget = static_cast<QWidget *>(obj);

        QObject *parent = qobject_cast<QObject *>(widget);
        bool doEnrich = false;
        while (NULL != (parent = parent->parent())) {
            if (parent->metaObject()->className() == QString("QArv::QArvMainWindow")) {
                doEnrich = true;
                break;
            }
        }

        if (doEnrich) {
            QString tooltip = widget->toolTip();
            if (!Qt::mightBeRichText(tooltip) && tooltip.size() > size_threshold) {
                // Prefix <qt/> to make sure Qt detects this as rich text
                // Escape the current message as HTML and replace \n by <br>
                tooltip = "<qt/>" + QString((tooltip)).toHtmlEscaped();
                widget->setToolTip(tooltip);
                return true;
            }
        }
    }
    return QObject::eventFilter(obj, evt);
}

void ArvConfigWindow::on_sliderUpdateSpinbox_valueChanged(int i)
{
    autoreadexposure->setInterval(i);
}

void ArvConfigWindow::on_statusTimeoutSpinbox_valueChanged(int i)
{
    statusTimeoutMsec = 1000 * i;
}

void ArvConfigWindow::setupListOfSavedWidgets()
{
    // settings tab
    saved_widgets["general"]["invert_colors"] = invertColors;
    saved_widgets["general"]["flip_horizontal"] = flipHorizontal;
    saved_widgets["general"]["flip_vertical"] = flipVertical;
    saved_widgets["general"]["rotation"] = rotationSelector;
    saved_widgets["general"]["drop_invalid_frames"] = dropInvalidFrames;
    saved_widgets["general"]["exposure_update_ms"] = sliderUpdateSpinbox;
    saved_widgets["general"]["statusbar_timeout"] = statusTimeoutSpinbox;
    saved_widgets["general"]["fast_swscale"] = useFastInterpolator;

    // ROI box
    saved_widgets["roi"]["x"] = xSpinbox;
    saved_widgets["roi"]["y"] = ySpinbox;
    saved_widgets["roi"]["width"] = wSpinbox;
    saved_widgets["roi"]["height"] = hSpinbox;
    saved_widgets["roi"]["binning"] = binSpinBox;
    saved_widgets["roi"]["constraint"] = ROIsizeCombo;

    // display widgets
    saved_widgets["videodisplay"]["actual_size"] = unzoomButton;

    // advanced features tab
    saved_widgets["features"]["cache_policy"] = registerCacheCheck;
    saved_widgets["features"]["save_advanced"] = saveAdvancedCb;
}

void ArvConfigWindow::serializeSettings(QVariantHash &settings, QByteArray &camFeatures)
{
    // buttons, combo boxes, text fields etc.
    for (auto i = saved_widgets.begin(); i != saved_widgets.end(); i++) {
        ;
        const auto entry = i.value();

        QVariantHash sgroup;
        for (auto k = entry.begin(); k != entry.end(); k++) {
            const auto confKey = k.key();
            QWidget *widget = k.value();

            if (auto *w = qobject_cast<QCheckBox *>(widget))
                sgroup[confKey] = w->checkState();
            else if (auto *w = qobject_cast<QAbstractButton *>(widget))
                sgroup[confKey] = w->isChecked();
            else if (auto *w = qobject_cast<QComboBox *>(widget))
                sgroup[confKey] = w->currentIndex();
            else if (auto *w = qobject_cast<QLineEdit *>(widget))
                sgroup[confKey] = w->text();
            else if (auto *w = qobject_cast<QSpinBox *>(widget))
                sgroup[confKey] = w->value();
            else if (auto *w = qobject_cast<QTimeEdit *>(widget))
                sgroup[confKey] = w->time();
            else
                logMessage() << "FIXME: don't know what to save under setting" << i.key();
        }

        settings[i.key()] = sgroup;
    }

    if (cameraSelector->currentIndex() >= 0) {
        QVariantHash camSettings;
        const auto camInfo = cameraSelector->currentData().value<QArvCameraId>();
        camSettings["device"] = camInfo.id;
        if (camera) {
            camSettings["pixel_format"] = camera->getPixelFormat();
            camSettings["fps"] = camera->getFPS();
        }
        settings["camera"] = camSettings;

        if (saveAdvancedCb->isChecked()) {
            QTextStream file(&camFeatures);
            file << camera.get();
        }
    }
}

void ArvConfigWindow::loadSettings(const QVariantHash &settings, const QByteArray &camFeatures)
{
    // buttons, combo boxes, text fields etc.
    for (auto i = saved_widgets.begin(); i != saved_widgets.end(); i++) {
        const auto entry = i.value();
        const auto sgroup = settings.value(i.key()).toHash();

        for (auto k = entry.begin(); k != entry.end(); k++) {
            auto widget = k.value();
            const auto data = sgroup.value(k.key());

            if (!data.isValid())
                continue;

            if (auto *w = qobject_cast<QCheckBox *>(widget)) {
                w->blockSignals(true);
                w->setCheckState(Qt::CheckState(data.toInt()));
                w->blockSignals(false);
            } else if (auto *w = qobject_cast<QAbstractButton *>(widget)) {
                w->blockSignals(true);
                w->setChecked(data.toBool());
                w->blockSignals(false);
            } else if (auto *w = qobject_cast<QComboBox *>(widget)) {
                w->blockSignals(true);
                w->setCurrentIndex(data.toInt());
                w->blockSignals(false);
            } else if (auto *w = qobject_cast<QLineEdit *>(widget)) {
                w->blockSignals(true);
                w->setText(data.toString());
                w->blockSignals(false);
            } else if (auto *w = qobject_cast<QSpinBox *>(widget)) {
                w->blockSignals(true);
                w->setValue(data.toInt());
                w->blockSignals(false);
            } else if (auto *w = qobject_cast<QTimeEdit *>(widget)) {
                w->blockSignals(true);
                w->setTime(data.toTime());
                w->blockSignals(false);
            } else {
                logMessage() << "FIXME: don't know how to restore setting" << i.key();
            }
        }
    }

    // ensure any timers run to update the list of available cameras or modify settings
    QApplication::processEvents();

    const auto camSettings = settings["camera"].toHash();
    QVariant data = camSettings.value("device");
    int prevCamIdx = -1;
    for (int i = 0; i < cameraSelector->count(); i++) {
        if (cameraSelector->itemData(i).value<QArvCameraId>().id == data.toString()) {
            prevCamIdx = i;
            break;
        }
    }
    if (prevCamIdx >= 0)
        cameraSelector->setCurrentIndex(prevCamIdx);
    else
        cameraSelector->setCurrentIndex(0);

    if (!camera || prevCamIdx < 0) {
        logMessage() << "Not loading camera settings: No suitable camera selected";
        return;
    }

    const auto pixelFormat = camSettings["pixel_format"].toString();
    if (!pixelFormat.isEmpty())
        pixelFormatSelector->setCurrentIndex(pixelFormatSelector->findData(pixelFormat));

    const auto roiSettings = settings["roi"].toHash();
    camera->setROI(QRect(
        roiSettings["x"].toInt(),
        roiSettings["y"].toInt(),
        roiSettings["width"].toInt(),
        roiSettings["height"].toInt()));
    camera->setFPS(camSettings["fps"].toInt());

    // reload pixel format and update decoder with new ROI as well
    on_pixelFormatSelector_currentIndexChanged(pixelFormatSelector->currentIndex());

    // if no advanced features were saved, we can skip loading them
    if (!saveAdvancedCb->isChecked())
        return;

    QByteArray camFeatureBuffer = camFeatures;
    QTextStream file(&camFeatureBuffer);
    auto wholefile_ = file.readAll();
    QString readBack_;
    QTextStream wholefile(&wholefile_);
    QTextStream readBack(&readBack_);

    // Try setting it several times, then check if successful.
    for (int i = 0; i < 20; i++) {
        wholefile.seek(0);
        readBack.seek(0);
        wholefile >> camera.get();
        readBack << camera.get();
        readBack << Qt::endl << Qt::endl;
        QApplication::processEvents();
        QThread::msleep(10);
    }
    QStringList failures;
    wholefile.seek(0);
    while (!wholefile.atEnd()) {
        QString wanted = wholefile.readLine();
        QString actual = readBack.readLine();

        if (wanted.trimmed().startsWith("DeviceTemperature")) {
            // Skip temperature-related settings
            continue;
        }

        if (wanted != actual) {
            logMessage() << "Setting failure, wanted:" << wanted << Qt::endl << "actual:" << actual;
            failures << wanted;
        }
    }
    if (failures.count() != 0) {
        QString message = "<html><head/><body><p>"
                          + tr(
                              "Settings could not be completely loaded. "
                              "This can happen because camera features are interdependent and may "
                              "require a specific loading order. The following settings failed:")
                          + "</p>";
        foreach (auto fail, failures)
            message += fail;
        message += "</body></html>";
        QMessageBox::warning(
            this, QStringLiteral("%1 - Failed to load settings").arg(cameraSelector->currentText()), message);
    }
}

void ArvConfigWindow::refreshCameras()
{
    on_refreshCamerasButton_clicked();
}

void ArvConfigWindow::closeEvent(QCloseEvent *event)
{
    playButton->setChecked(false);
    toggleVideoPreview(false);
    event->accept();
}

void ArvConfigWindow::on_registerCacheCheck_stateChanged(int state)
{
    bool enable = state != Qt::Unchecked;
    bool debug = state == Qt::PartiallyChecked;
    camera->enableRegisterCache(enable, debug);
}

void ArvConfigWindow::bufferUnderrunOccured()
{
    QString msg = tr("Buffer underrun!");
    logMessage() << msg;
    statusBar()->showMessage(msg, statusTimeoutMsec);
}

void ArvConfigWindow::previewFrameReceived(const QByteArray &frame, ArvBuffer *aravisFrame)
{
    if (!decoder)
        return;
    if (frame.isEmpty())
        return;

    decoder->decode(frame);
    auto img = decoder->getCvImage();

    if (transformParams.invert) {
        int bits = img.depth() == CV_8U ? 8 : 16;
        cv::subtract((1 << bits) - 1, img, img);
    }

    if (transformParams.flip != -100)
        cv::flip(img, img, transformParams.flip);

    switch (transformParams.rot) {
    case 1:
        cv::transpose(img, img);
        cv::flip(img, img, 0);
        break;

    case 2:
        cv::flip(img, img, -1);
        break;

    case 3:
        cv::transpose(img, img);
        cv::flip(img, img, 1);
        break;
    }

    QArvDecoder::CV2QImage(img, *(videoWidget->unusedFrame()));
    videoWidget->swapFrames();
}
