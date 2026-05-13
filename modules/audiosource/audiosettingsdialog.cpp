/**
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "audiosettingsdialog.h"
#include "ui_audiosettingsdialog.h"

#include <QVariant>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <gst/gst.h>
#pragma GCC diagnostic pop

static QString audioDeviceStableId(GstDevice *device)
{
    g_autoptr(GstStructure) props = gst_device_get_properties(device);
    if (props != nullptr) {
        for (const char *key : {"node.name", "device.bus_path", "device.name", "alsa.card_name"}) {
            const gchar *val = gst_structure_get_string(props, key);
            if (val != nullptr && *val != '\0')
                return QString::fromUtf8(val);
        }
    }
    g_autofree gchar *displayName = gst_device_get_display_name(device);
    return displayName != nullptr ? QString::fromUtf8(displayName) : QString();
}

static const char *const WAVE_NAMES[] = {
    "Sine",
    "Square",
    "Saw",
    "Triangle",
    "Silence",
    "White Uniform Noise",
    "Pink Noise",
    "Sine Table",
    "Periodic Ticks",
    "White Gaussian Noise",
    "Red (Brownian) Noise",
    "Blue Noise",
    "Violet Noise",
};

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::AudioSettingsDialog)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-config"));

    for (int i = 0; i < (int)(sizeof(WAVE_NAMES) / sizeof(WAVE_NAMES[0])); ++i)
        ui->waveComboBox->addItem(QString::fromUtf8(WAVE_NAMES[i]), i);

    // Populate output device list
    ui->deviceComboBox->addItem(QStringLiteral("System Default"), QString());

    GstDeviceMonitor *monitor = gst_device_monitor_new();
    GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
    gst_device_monitor_add_filter(monitor, "Audio/Sink", caps);
    gst_caps_unref(caps);

    if (gst_device_monitor_start(monitor)) {
        GList *devices = gst_device_monitor_get_devices(monitor);
        for (GList *it = devices; it != nullptr; it = it->next) {
            GstDevice *dev = GST_DEVICE(it->data);
            g_autofree gchar *displayName = gst_device_get_display_name(dev);
            const QString id = audioDeviceStableId(dev);
            const QString label = displayName != nullptr ? QString::fromUtf8(displayName) : id;
            if (!id.isEmpty())
                ui->deviceComboBox->addItem(label, id);
            gst_object_unref(dev);
        }
        g_list_free(devices);
        gst_device_monitor_stop(monitor);
    }
    gst_object_unref(monitor);
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    delete ui;
}

bool AudioSettingsDialog::startImmediately() const
{
    return ui->immediatePlayCheckBox->isChecked();
}

void AudioSettingsDialog::setStartImmediately(bool value)
{
    ui->immediatePlayCheckBox->setChecked(value);
}

int AudioSettingsDialog::waveKind() const
{
    return ui->waveComboBox->currentData().toInt();
}

void AudioSettingsDialog::setWaveKind(int value)
{
    ui->waveComboBox->setCurrentIndex(ui->waveComboBox->findData(value));
}

QString AudioSettingsDialog::waveKindName() const
{
    const int kind = waveKind();
    if (kind >= 0 && kind < (int)(sizeof(WAVE_NAMES) / sizeof(WAVE_NAMES[0])))
        return QString::fromUtf8(WAVE_NAMES[kind]);
    return QStringLiteral("?");
}

QString AudioSettingsDialog::deviceId() const
{
    return ui->deviceComboBox->currentData().toString();
}

void AudioSettingsDialog::setDeviceId(const QString &value)
{
    const int idx = ui->deviceComboBox->findData(value);
    ui->deviceComboBox->setCurrentIndex(idx >= 0 ? idx : 0);
}

double AudioSettingsDialog::frequency() const
{
    return ui->freqSpinBox->value();
}

void AudioSettingsDialog::setFrequency(double value)
{
    ui->freqSpinBox->setValue(value);
}

double AudioSettingsDialog::volume() const
{
    return ui->volumeSlider->value() / 100.0;
}

void AudioSettingsDialog::setVolume(double value)
{
    ui->volumeSlider->setValue(value * 100.0);
}
