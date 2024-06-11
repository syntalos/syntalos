/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012-2014 Jure Varlec <jure.varlec@ad-vega.si>
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

#pragma once

#include <gio/gio.h> // Workaround for gdbusintrospection's use of "signal".
#include "ui_configwindow.h"

#include "glvideowidget.h"
#include "qarv/qarvcamera.h"
#include "qarv/qarvdecoder.h"

#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QTransform>
#include <QStandardItemModel>
#include <QProgressBar>
#include <QFutureWatcher>

using namespace QArv;

class QArvGui;

/**
 * @brief Image transformation parameters
 */
struct TransformParams {
    int flip;
    int rot;
    bool invert;
    QTransform qtf;

    TransformParams()
        : flip(-100),
          rot(0),
          invert(false)
    {
    }
};

class ArvConfigWindow : public QMainWindow, private Ui::ArvConfigWindowUI
{
    Q_OBJECT

public:
    explicit ArvConfigWindow(const QString &modId, QWidget *parent = nullptr);
    ~ArvConfigWindow();

    void setCameraInUseExternal(bool camInUse);
    TransformParams *currentTransformParams();

    void serializeSettings(QVariantHash &settings, QByteArray &camFeatures);
    void loadSettings(const QVariantHash &settings, const QByteArray &camFeatures);

signals:
    void recordingStarted(bool started);
    void cameraSelected(const std::shared_ptr<QArvCamera> &camera, const std::shared_ptr<QArvDecoder> &decoder);

private slots:
    void on_refreshCamerasButton_clicked(bool clicked = false);
    void on_unzoomButton_toggled(bool checked);
    void on_cameraSelector_currentIndexChanged(int index);
    void on_exposureAutoButton_toggled(bool checked);
    void on_gainAutoButton_toggled(bool checked);
    void on_pixelFormatSelector_currentIndexChanged(int index);
    void on_playButton_toggled(bool checked);
    void on_fpsSpinbox_valueChanged(int value);
    void on_gainSlider_valueChanged(int value);
    void on_exposureSlider_valueChanged(int value);
    void on_resetROIButton_clicked(bool clicked);
    void on_applyROIButton_clicked(bool clicked);
    void on_binSpinBox_valueChanged(int value);
    void on_editExposureButton_clicked(bool checked);
    void on_editGainButton_clicked(bool checked);
    void on_exposureSpinbox_editingFinished();
    void on_gainSpinbox_editingFinished();
    void on_showVideoAction_toggled(bool checked);
    void on_videodock_visibilityChanged(bool visible);
    void on_videodock_topLevelChanged(bool floating);
    void on_messageAction_toggled(bool checked);
    void on_messageDock_visibilityChanged(bool visible);
    void on_messageDock_topLevelChanged(bool floating);
    void on_ROIsizeCombo_newSizeSelected(QSize size);
    void on_sliderUpdateSpinbox_valueChanged(int i);
    void on_statusTimeoutSpinbox_valueChanged(int i);
    void on_registerCacheCheck_stateChanged(int state);
    void pickedROI(QRect roi);
    void readExposure();
    void readGain();
    void toggleVideoPreview(bool start);
    void updateBandwidthEstimation();
    void updateImageTransform();
    void readAllValues();
    void setupListOfSavedWidgets();
    void bufferUnderrunOccured();
    void previewFrameReceived(const QByteArray &frame, ArvBuffer *aravisFrame);

private:
    void updateDecoder();
    void readROILimits();
    void closeEvent(QCloseEvent *event) override;
    void setCameraInUse(bool camInUse);

    inline QArvDebug logMessage() const
    {
        return {m_modId};
    }

    QString m_modId;
    std::shared_ptr<QArvCamera> camera;
    std::shared_ptr<QArvDecoder> decoder;

    QRect roirange, roidefault;
    QPair<double, double> gainrange, exposurerange;
    QTimer *autoreadexposure;
    bool playing, started;
    TransformParams transformParams;
    QByteArray oldstate, oldgeometry;
    QSize oldsize;
    int statusTimeoutMsec;
    QMap<QString, QMap<QString, QWidget *>> saved_widgets;
    QFile timestampFile;

    friend class ::QArvGui;
};

/* Qt event filter that intercepts ToolTipChange events and replaces the
 * tooltip with a rich text representation if needed. This assures that Qt
 * can word-wrap long tooltip messages. Tooltips longer than the provided
 * size threshold (in characters) are wrapped. Only effective if the widget's
 * ancestors include a QArvMainWindow.
 */
class ToolTipToRichTextFilter : public QObject
{
    Q_OBJECT

public:
    ToolTipToRichTextFilter(int size_threshold, QObject *parent);

protected:
    bool eventFilter(QObject *obj, QEvent *evt) override;

private:
    int size_threshold;
};
