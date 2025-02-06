/*
 * Copyright (C) 2010 Matthias Klumpp <matthias@tenstral.net>
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

#include "firmatactldialog.h"
#include "ui_firmatactldialog.h"

#include <QDebug>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

FirmataOutputWidget::FirmataOutputWidget(
    std::shared_ptr<DataStream<FirmataControl>> fmCtlStream,
    bool analog,
    QWidget *parent)
    : QWidget(parent),
      m_isAnalog(analog),
      m_fmCtlStream(fmCtlStream)
{
    m_btnRemove = new QPushButton(this);
    m_btnRemove->setFlat(true);
    m_btnRemove->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_btnRemove->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(m_btnRemove, &QPushButton::clicked, [&] {
        deleteLater();
    });

    m_sbPinId = new QSpinBox(this);
    m_sbPinId->setPrefix(QStringLiteral("Pin: "));
    m_sbPinId->setRange(0, 255);
    m_sbPinId->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(m_sbPinId, SIGNAL(valueChanged(int)), this, SLOT(onPinIdChange(int)));

    m_btnSend = new QPushButton(this);
    m_btnSend->setText(analog ? QStringLiteral("Send") : QStringLiteral("Off"));
    m_btnSend->setCheckable(!analog);

    m_sbValue = new QSpinBox(this);
    m_sbValue->setVisible(analog);
    m_sbValue->setPrefix(QStringLiteral("Value: "));
    m_sbValue->setRange(0, 65535);
    m_sbValue->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_btnPulse = new QPushButton(this);
    m_btnPulse->setText(QStringLiteral("Pulse"));
    m_btnPulse->setVisible(!analog);
    m_btnPulse->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    const auto vLine1 = new QFrame(this);
    vLine1->setFrameShape(QFrame::VLine);
    const auto vLine2 = new QFrame(this);
    vLine2->setFrameShape(QFrame::VLine);

    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    layout->addWidget(m_btnRemove);
    layout->addWidget(vLine1);

    layout->addWidget(m_sbPinId);
    layout->addWidget(vLine2);
    if (analog) {
        const auto vLine3 = new QFrame(this);
        vLine3->setFrameShape(QFrame::VLine);

        layout->addWidget(m_sbValue);
        layout->addWidget(vLine3);
    } else {
        layout->addWidget(m_btnPulse);
    }

    layout->addWidget(m_btnSend);

    setLayout(layout);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    connect(m_btnSend, &QPushButton::clicked, [&] {
        FirmataControl ctl;
        ctl.pinId = m_sbPinId->value();

        if (m_isAnalog) {
            ctl.command = FirmataCommandKind::WRITE_ANALOG;
            ctl.value = m_sbValue->value();
        } else {
            ctl.command = FirmataCommandKind::WRITE_DIGITAL;
            ctl.value = m_btnSend->isChecked() ? 1 : 0;
            m_btnSend->setText(m_btnSend->isChecked() ? QStringLiteral("On") : QStringLiteral("Off"));
        }

        m_fmCtlStream->push(ctl);
    });

    connect(m_btnPulse, &QPushButton::clicked, [&] {
        m_btnSend->setChecked(false);
        m_btnSend->setText(QStringLiteral("Off"));
        FirmataControl ctl;
        ctl.pinId = m_sbPinId->value();
        ctl.command = FirmataCommandKind::WRITE_DIGITAL_PULSE;
        ctl.value = 1;
        m_fmCtlStream->push(ctl);
    });

    // register the new pin, in case we are already running
    onPinIdChange(m_sbPinId->value());
}

bool FirmataOutputWidget::isAnalog() const
{
    return m_isAnalog;
}

int FirmataOutputWidget::pinId() const
{
    return m_sbPinId->value();
}

void FirmataOutputWidget::setPinId(int pinId)
{
    m_sbPinId->setValue(pinId);
}

void FirmataOutputWidget::submitNewPinCommand()
{
    // New output pin
    FirmataControl newPinCtl;
    newPinCtl.pinId = m_sbPinId->value();
    newPinCtl.isOutput = true;
    newPinCtl.command = m_isAnalog ? FirmataCommandKind::NEW_ANA_PIN : FirmataCommandKind::NEW_DIG_PIN;
    m_fmCtlStream->push(newPinCtl);

    qDebug() << "New pin change pushed" << newPinCtl.pinId;
}

int FirmataOutputWidget::value() const
{
    return m_sbValue->value();
}

void FirmataOutputWidget::setValue(int value)
{
    m_sbValue->setValue(value);
}

void FirmataOutputWidget::onPinIdChange(int)
{
    submitNewPinCommand();
}

FirmataInputWidget::FirmataInputWidget(
    std::shared_ptr<DataStream<FirmataControl>> fmCtlStream,
    bool analog,
    QWidget *parent)
    : QWidget(parent),
      m_isAnalog(analog),
      m_fmCtlStream(fmCtlStream)
{
    m_btnRemove = new QPushButton(this);
    m_btnRemove->setFlat(true);
    m_btnRemove->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_btnRemove->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(m_btnRemove, &QPushButton::clicked, [&] {
        deleteLater();
    });

    m_lblType = new QLabel(this);
    auto font = m_lblType->font();
    font.setBold(true);
    font.setPointSize(10);
    m_lblType->setFont(font);
    m_lblType->setText(analog ? QStringLiteral("A") : QStringLiteral("D"));
    m_lblType->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_sbPinId = new QSpinBox(this);
    m_sbPinId->setPrefix(QStringLiteral("Pin: "));
    m_sbPinId->setRange(0, 255);
    m_sbPinId->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(m_sbPinId, SIGNAL(valueChanged(int)), this, SLOT(onPinIdChange(int)));

    m_lblValue = new QLabel(this);
    m_lblValue->setText(analog ? QStringLiteral("0") : QStringLiteral("false"));

    const auto vLine1 = new QFrame(this);
    vLine1->setFrameShape(QFrame::VLine);
    const auto vLine2 = new QFrame(this);
    vLine2->setFrameShape(QFrame::VLine);

    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_btnRemove);
    layout->addWidget(vLine1);

    layout->addWidget(m_lblType);
    layout->addWidget(m_sbPinId);
    layout->addWidget(vLine2);

    layout->addWidget(m_lblValue);

    setLayout(layout);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
}

bool FirmataInputWidget::isAnalog() const
{
    return m_isAnalog;
}

int FirmataInputWidget::pinId() const
{
    return m_sbPinId->value();
}

void FirmataInputWidget::setPinId(int pinId)
{
    m_sbPinId->setValue(pinId);
}

void FirmataInputWidget::setValue(int value)
{
    if (m_isAnalog)
        m_lblValue->setText(QString::number(value));
    else
        m_lblValue->setText((value > 0) ? QStringLiteral("true") : QStringLiteral("false"));
}

void FirmataInputWidget::submitNewPinCommand()
{
    // New input pin
    FirmataControl newPinCtl;
    newPinCtl.pinId = m_sbPinId->value();
    newPinCtl.isOutput = false;
    newPinCtl.command = m_isAnalog ? FirmataCommandKind::NEW_ANA_PIN : FirmataCommandKind::NEW_DIG_PIN;
    m_fmCtlStream->push(newPinCtl);
}

void FirmataInputWidget::onPinIdChange(int)
{
    submitNewPinCommand();
}

FirmataCtlDialog::FirmataCtlDialog(std::shared_ptr<DataStream<FirmataControl>> fmCtlStream, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::FirmataCtlDialog),
      m_lastPinId(0),
      m_fmCtlStream(fmCtlStream)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-view"));
}

FirmataCtlDialog::~FirmataCtlDialog()
{
    delete ui;
}

void FirmataCtlDialog::initializeAllPins()
{
    for (const auto w : ui->saOutputContents->children()) {
        const auto ow = qobject_cast<FirmataOutputWidget *>(w);
        if (ow == nullptr)
            continue;
        ow->submitNewPinCommand();
    }

    for (const auto w : ui->saInputContents->children()) {
        const auto iw = qobject_cast<FirmataInputWidget *>(w);
        if (iw == nullptr)
            continue;
        iw->submitNewPinCommand();
    }
}

void FirmataCtlDialog::pinValueChanged(const FirmataData &data)
{
    for (const auto w : ui->saInputContents->children()) {
        const auto iw = qobject_cast<FirmataInputWidget *>(w);
        if (iw == nullptr)
            continue;
        if (iw->pinId() == data.pinId) {
            iw->setValue(data.value);
            return;
        }
    }
}

QVariantHash FirmataCtlDialog::serializeSettings()
{
    QVariantList outputCtls;
    QVariantList inputViews;

    for (const auto w : ui->saOutputContents->children()) {
        const auto ow = qobject_cast<FirmataOutputWidget *>(w);
        if (ow == nullptr)
            continue;
        QVariantHash var;
        var.insert("pin_id", ow->pinId());
        var.insert("is_analog", ow->isAnalog());
        if (ow->isAnalog())
            var.insert("value", ow->value());
        outputCtls.append(var);
    }

    for (const auto w : ui->saInputContents->children()) {
        const auto iw = qobject_cast<FirmataInputWidget *>(w);
        if (iw == nullptr)
            continue;
        QVariantHash var;
        var.insert("pin_id", iw->pinId());
        var.insert("is_analog", iw->isAnalog());
        inputViews.append(var);
    }

    QVariantHash settings;
    settings.insert("output_ctls", outputCtls);
    settings.insert("input_views", inputViews);
    return settings;
}

void FirmataCtlDialog::restoreFromSettings(const QVariantHash &settings)
{
    for (const auto w : ui->saOutputContents->children()) {
        const auto ow = qobject_cast<FirmataOutputWidget *>(w);
        if (ow == nullptr)
            continue;
        delete ow;
    }
    for (const auto w : ui->saInputContents->children()) {
        const auto iw = qobject_cast<FirmataInputWidget *>(w);
        if (iw == nullptr)
            continue;
        delete iw;
    }

    QVariantList outputCtls;
    QVariantList inputViews;

    outputCtls = settings.value("output_ctls").toList();
    inputViews = settings.value("input_views").toList();

    for (const auto &varRaw : outputCtls) {
        const auto var = varRaw.toHash();
        auto w = new FirmataOutputWidget(m_fmCtlStream, var.value("is_analog", false).toBool(), ui->saOutputContents);
        const auto layout = qobject_cast<QVBoxLayout *>(ui->saOutputContents->layout());
        layout->insertWidget(layout->count() - 1, w);
        w->setPinId(var.value("pin_id", 0).toInt());
        if (w->isAnalog())
            w->setValue(var.value("value", 0).toInt());
    }

    for (const auto &varRaw : inputViews) {
        const auto var = varRaw.toHash();
        auto w = new FirmataInputWidget(m_fmCtlStream, var.value("is_analog", false).toBool(), ui->saInputContents);
        const auto layout = qobject_cast<QVBoxLayout *>(ui->saInputContents->layout());
        layout->insertWidget(layout->count() - 1, w);
        w->setPinId(var.value("pin_id", 0).toInt());
    }
}

void FirmataCtlDialog::on_btnAddOutputControl_clicked()
{
    bool ok;
    auto item = QInputDialog::getItem(
        this,
        QStringLiteral("Select Data Type"),
        QStringLiteral("Data modality to add output control for:"),
        QStringList() << "Digital"
                      << "Analog",
        0,
        false,
        &ok);
    if (!ok || item.isEmpty())
        return;

    auto w = new FirmataOutputWidget(m_fmCtlStream, item.startsWith(QStringLiteral("Analog")), ui->saOutputContents);
    const auto layout = qobject_cast<QVBoxLayout *>(ui->saOutputContents->layout());
    layout->insertWidget(layout->count() - 1, w);

    w->setPinId(m_lastPinId);
    m_lastPinId++;
    if (m_lastPinId > 255)
        m_lastPinId = 0;
}

void FirmataCtlDialog::on_btnAddInputWatch_clicked()
{
    bool ok;
    auto item = QInputDialog::getItem(
        this,
        QStringLiteral("Select Data Type"),
        QStringLiteral("Data modality to add input watcher for:"),
        QStringList() << "Digital"
                      << "Analog",
        0,
        false,
        &ok);
    if (!ok || item.isEmpty())
        return;

    auto w = new FirmataInputWidget(m_fmCtlStream, item.startsWith(QStringLiteral("Analog")), ui->saInputContents);
    const auto layout = qobject_cast<QVBoxLayout *>(ui->saInputContents->layout());
    layout->insertWidget(layout->count() - 1, w);

    w->setPinId(m_lastPinId);
    m_lastPinId++;
    if (m_lastPinId > 255)
        m_lastPinId = 0;
}
