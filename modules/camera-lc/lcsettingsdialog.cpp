/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lcsettingsdialog.h"
#include "ui_lcsettingsdialog.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <cmath>

// number of discrete steps every control slider is divided into; the slider
// position is mapped linearly onto the paired spinbox's current [min, max]
static constexpr int kSliderSteps = 1000;

LcSettingsDialog::LcSettingsDialog(LcCamera *camera, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::LcSettingsDialog)
{
    ui->setupUi(this);
    m_camera = camera;

    linkSlider(ui->sliderExposure, ui->sbExposure);
    linkSlider(ui->sliderGain, ui->sbGain);
    linkSlider(ui->sliderBrightness, ui->sbBrightness);
    linkSlider(ui->sliderContrast, ui->sbContrast);
    linkSlider(ui->sliderSaturation, ui->sbSaturation);
    linkSlider(ui->sliderGamma, ui->sbGamma);

    updateValues();
}

void LcSettingsDialog::linkSlider(QSlider *slider, QDoubleSpinBox *box)
{
    slider->setRange(0, kSliderSteps);
    m_sliderPairs.append(qMakePair(slider, box));

    // spinbox -> slider: reflect a typed/applied value as a slider position,
    // without re-triggering the slider's own handler
    connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [slider, box](double value) {
        const double min = box->minimum();
        const double max = box->maximum();
        const int pos = (max > min) ? static_cast<int>(std::lround((value - min) / (max - min) * kSliderSteps)) : 0;
        QSignalBlocker blocker(slider);
        slider->setValue(pos);
    });

    // slider -> spinbox: map the position back to a value and write it into the
    // spinbox, whose existing valueChanged slot applies it to the camera
    connect(slider, &QSlider::valueChanged, this, [box](int pos) {
        const double min = box->minimum();
        const double max = box->maximum();
        box->setValue(min + (static_cast<double>(pos) / kSliderSteps) * (max - min));
    });
}

LcSettingsDialog::~LcSettingsDialog()
{
    delete ui;
}

cv::Size LcSettingsDialog::resolution() const
{
    if (ui->resolutionComboBox->count() > 0) {
        const auto s = ui->resolutionComboBox->currentData().toSize();
        if (!s.isEmpty())
            return cv::Size(s.width(), s.height());
    }
    return m_camera->resolution();
}

double LcSettingsDialog::framerate() const
{
    return ui->fpsSpinBox->value();
}

void LcSettingsDialog::setFramerate(double fps)
{
    ui->fpsSpinBox->setValue(fps);
}

QString LcSettingsDialog::pixelFormatName() const
{
    return m_pixFmtName;
}

void LcSettingsDialog::setPixelFormatName(const QString &pixFmtName)
{
    if (pixFmtName.isEmpty())
        return;
    for (int i = 0; i < ui->captureFormatComboBox->count(); i++) {
        if (ui->captureFormatComboBox->itemText(i) == pixFmtName) {
            ui->captureFormatComboBox->setCurrentIndex(i);
            on_captureFormatComboBox_currentIndexChanged(i);
            break;
        }
    }
}

void LcSettingsDialog::setRunning(bool running)
{
    ui->cameraGroupBox->setEnabled(!running);
}

void LcSettingsDialog::updateValues()
{
    const auto prevCamId = m_camera->cameraId();
    const auto prevPixFmtName = m_pixFmtName;

    ui->cameraComboBox->clear();
    for (const auto &camInfo : LcCamera::availableCameras())
        ui->cameraComboBox->addItem(camInfo.first, camInfo.second);

    for (int i = 0; i < ui->cameraComboBox->count(); i++) {
        if (ui->cameraComboBox->itemData(i).toString() == prevCamId) {
            ui->cameraComboBox->setCurrentIndex(i);
            on_cameraComboBox_currentIndexChanged(i);
            break;
        }
    }

    setPixelFormatName(prevPixFmtName);

    ui->fpsSpinBox->setValue(m_camera->framerate());
    refreshControls();
}

void LcSettingsDialog::on_cameraComboBox_currentIndexChanged(int)
{
    m_camera->setCameraId(ui->cameraComboBox->currentData().toString());

    ui->captureFormatComboBox->clear();
    for (const auto &fmt : m_camera->readPixelFormats())
        ui->captureFormatComboBox->addItem(fmt);

    // default to uncompressed YUYV when available: for data acquisition we prefer
    // exact pixels over MJPEG's lossy compression (a saved selection overrides this)
    const int yuyvIdx = ui->captureFormatComboBox->findText(QStringLiteral("YUYV"));
    if (yuyvIdx >= 0)
        ui->captureFormatComboBox->setCurrentIndex(yuyvIdx);

    refreshControls();
}

void LcSettingsDialog::on_captureFormatComboBox_currentIndexChanged(int)
{
    m_pixFmtName = ui->captureFormatComboBox->currentText();
    m_camera->setPixelFormat(m_pixFmtName);

    // the set of supported frame sizes depends on the pixel format, so refresh them here
    refreshResolutions();
}

void LcSettingsDialog::refreshResolutions()
{
    const auto frameSizes = m_camera->readFrameSizes(m_pixFmtName);
    const auto currentRes = m_camera->resolution();

    ui->resolutionComboBox->clear();
    int selectIndex = 0;
    for (const auto &size : frameSizes) {
        ui->resolutionComboBox->addItem(
            QStringLiteral("%1 x %2").arg(size.width).arg(size.height),
            QVariant(QSize(size.width, size.height)));
        if (size.width == currentRes.width && size.height == currentRes.height)
            selectIndex = ui->resolutionComboBox->count() - 1;
    }
    if (ui->resolutionComboBox->count() > 0)
        ui->resolutionComboBox->setCurrentIndex(selectIndex);
}

void LcSettingsDialog::refreshControls()
{
    // configure a single control row: set range and current value from the camera,
    // or hide the row entirely if the camera does not expose the control
    auto configureRow = [&](const QString &name, QLabel *label, QDoubleSpinBox *box, double value) {
        const auto range = m_camera->controlRange(name);
        // the spinbox lives inside a wrapper widget alongside its slider; hide
        // the whole wrapper so an unavailable control leaves no empty row behind
        QWidget *field = box->parentWidget();
        label->setVisible(range.available);
        field->setVisible(range.available);
        if (!range.available)
            return;
        box->blockSignals(true);
        box->setMinimum(range.min);
        box->setMaximum(range.max);
        box->setValue(value);
        box->blockSignals(false);
    };

    const auto aeRange = m_camera->controlRange(QStringLiteral("ae"));
    ui->autoExposureLabel->setVisible(aeRange.available);
    ui->autoExposureCheckBox->setVisible(aeRange.available);
    ui->autoExposureCheckBox->blockSignals(true);
    ui->autoExposureCheckBox->setChecked(m_camera->autoExposure());
    ui->autoExposureCheckBox->blockSignals(false);

    configureRow(QStringLiteral("exposure"), ui->exposureLabel, ui->sbExposure, m_camera->exposureTime());
    configureRow(QStringLiteral("gain"), ui->gainLabel, ui->sbGain, m_camera->gain());
    configureRow(QStringLiteral("brightness"), ui->brightnessLabel, ui->sbBrightness, m_camera->brightness());
    configureRow(QStringLiteral("contrast"), ui->contrastLabel, ui->sbContrast, m_camera->contrast());
    configureRow(QStringLiteral("saturation"), ui->saturationLabel, ui->sbSaturation, m_camera->saturation());
    configureRow(QStringLiteral("gamma"), ui->gammaLabel, ui->sbGamma, m_camera->gamma());

    // configureRow() blocks the spinbox signals while setting values, so the
    // sliders did not follow along; sync their positions to the new values here
    // (each slider hides automatically with its wrapper when unavailable)
    for (const auto &pair : m_sliderPairs) {
        QSlider *slider = pair.first;
        const QDoubleSpinBox *box = pair.second;
        const double min = box->minimum();
        const double max = box->maximum();
        const int pos = (max > min) ? static_cast<int>(std::lround((box->value() - min) / (max - min) * kSliderSteps))
                                    : 0;
        QSignalBlocker blocker(slider);
        slider->setValue(pos);
    }

    // manual exposure and gain are only meaningful when auto-exposure is off; the AE
    // algorithm owns both while enabled, so we neither send nor let the user set them
    const bool aeOff = !m_camera->autoExposure();
    ui->sbExposure->setEnabled(aeOff);
    ui->sliderExposure->setEnabled(aeOff);
    ui->sbGain->setEnabled(aeOff);
    ui->sliderGain->setEnabled(aeOff);

    // power-line frequency (anti-flicker); combo index maps to the V4L2 value
    const bool plfSupported = m_camera->powerLineFrequencySupported();
    ui->powerLineLabel->setVisible(plfSupported);
    ui->powerLineComboBox->setVisible(plfSupported);
    if (plfSupported) {
        int v = m_camera->powerLineFrequency();
        if (v < 0)
            v = m_camera->readPowerLineFrequency(); // adopt the device's current setting
        if (v < 0 || v >= ui->powerLineComboBox->count())
            v = 0;
        ui->powerLineComboBox->blockSignals(true);
        ui->powerLineComboBox->setCurrentIndex(v);
        ui->powerLineComboBox->blockSignals(false);
        m_camera->setPowerLineFrequency(v);
    }
}

void LcSettingsDialog::on_autoExposureCheckBox_toggled(bool checked)
{
    m_camera->setAutoExposure(checked);
    // the AE algorithm owns both exposure and gain, so disable both while it is on
    ui->sbExposure->setEnabled(!checked);
    ui->sliderExposure->setEnabled(!checked);
    ui->sbGain->setEnabled(!checked);
    ui->sliderGain->setEnabled(!checked);
}

void LcSettingsDialog::on_sbExposure_valueChanged(double value)
{
    m_camera->setExposureTime(value);
}

void LcSettingsDialog::on_sbGain_valueChanged(double value)
{
    m_camera->setGain(value);
}

void LcSettingsDialog::on_sbBrightness_valueChanged(double value)
{
    m_camera->setBrightness(value);
}

void LcSettingsDialog::on_sbContrast_valueChanged(double value)
{
    m_camera->setContrast(value);
}

void LcSettingsDialog::on_sbSaturation_valueChanged(double value)
{
    m_camera->setSaturation(value);
}

void LcSettingsDialog::on_sbGamma_valueChanged(double value)
{
    m_camera->setGamma(value);
}

void LcSettingsDialog::on_powerLineComboBox_currentIndexChanged(int index)
{
    m_camera->setPowerLineFrequency(index);
}
