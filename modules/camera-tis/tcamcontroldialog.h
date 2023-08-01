/**
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-3.0+
 */

#pragma once

#include <QDialog>

#include "definitions.h"
#include "gst/app/gstappsink.h"
#include "indexer.h"
#include "propertiesbox.h"

struct TcamCaptureConfig {
    FormatHandling format_selection_type = FormatHandling::Auto;
    ConversionElement conversion_element = ConversionElement::Auto;
    QString video_sink_element = "xvimagesink";
    // expectations
    // output element name: sink
    // if a capsfilter element named device-caps exists it will have the configured caps set
    // all tcam-property elements are named: tcam0, tcam1, etc
    // tcam0 is always source
    QString pipeline =
        "tcambin name=tcam0 ! video/x-raw,format=BGRx ! queue leaky=downstream ! videoconvert n-threads=4 "
        "! appsink name=sink";
};

namespace Ui
{
class TcamControlDialog;
}

class TcamControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TcamControlDialog(std::shared_ptr<TcamCaptureConfig> config, QWidget *parent = nullptr);
    ~TcamControlDialog();

    GstElement *pipeline() const;
    GstAppSink *videoSink() const;
    GstCaps *currentCaps() const;

    Device selectedDevice() const;
    bool setDevice(const QString &model, const QString &serial, const QString &type, GstCaps *caps);

    TcamCollection *tcamCollection() const;

    void refreshPropertiesInfo();
    void setRunning(bool running);

    void closePipeline();

    void emitDeviceLostBySerial(const QString &serial);

signals:
    void deviceLost(const QString &message);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void on_selectDeviceButton_clicked();
    void on_selectFormatButton_clicked();
    void on_refreshButton_clicked();

    void emitDeviceLost(const Device &dev);

private:
    void createPropertiesBox();
    void deletePropertiesBox();
    GstCaps *showFormatDialog();
    void openPipeline(FormatHandling handling);

private:
    Ui::TcamControlDialog *ui;

    std::shared_ptr<Indexer> m_index;
    std::shared_ptr<TcamCaptureConfig> m_capConfig;
    TcamCollection *m_tcamCollection;

    PropertiesBox *m_propsBox = nullptr;

    Device m_selectedDevice;
    GstElement *m_pipeline = nullptr;
    GstElement *m_source = nullptr;
    GstAppSink *m_videoSink = nullptr;

    GstCaps *m_currentCaps = nullptr;
    GstCaps *m_selectedCaps = nullptr;
    QString m_device_caps;
};
